/*
MIT License

Copyright (c) 2016 Benjamin "Nefarius" Höglinger

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#include "driver.h"
#include "queue.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, HidGuardianQueueInitialize)
#endif

NTSTATUS
HidGuardianQueueInitialize(
    _In_ WDFDEVICE Device
    )
{
    WDFQUEUE queue;
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG    queueConfig;

    PAGED_CODE();
    
    //
    // Configure a default queue so that requests that are not
    // configure-fowarded using WdfDeviceConfigureRequestDispatching to goto
    // other queues get dispatched here.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
         &queueConfig,
        WdfIoQueueDispatchParallel
        );

    queueConfig.EvtIoDefault = HidGuardianEvtIoDefault;
    queueConfig.EvtIoDeviceControl = HidGuardianEvtIoDeviceControl;

    status = WdfIoQueueCreate(
                 Device,
                 &queueConfig,
                 WDF_NO_OBJECT_ATTRIBUTES,
                 &queue
                 );

    if( !NT_SUCCESS(status) ) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "WdfIoQueueCreate failed %!STATUS!", status);
        return status;
    }

    return status;
}

VOID HidGuardianEvtIoDefault(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request
)
{
    WDF_REQUEST_SEND_OPTIONS        options;
    NTSTATUS                        status;
    BOOLEAN                         ret;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_QUEUE, "%!FUNC! Entry");

    WdfRequestFormatRequestUsingCurrentType(Request);

    WDF_REQUEST_SEND_OPTIONS_INIT(&options,
        WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

    ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(WdfIoQueueGetDevice(Queue)), &options);

    if (ret == FALSE) {
        status = WdfRequestGetStatus(Request);
        TraceEvents(TRACE_LEVEL_ERROR, 
            TRACE_QUEUE,
            "WdfRequestSend failed: %!STATUS!", status);
        WdfRequestComplete(Request, status);
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_QUEUE, "%!FUNC! Exit");
}

_Use_decl_annotations_
VOID
HidGuardianEvtIoDeviceControl(
    IN WDFQUEUE  Queue,
    WDFREQUEST  Request,
    size_t  OutputBufferLength,
    size_t  InputBufferLength,
    ULONG  IoControlCode
)
{
    NTSTATUS                            status = STATUS_INVALID_PARAMETER;
    size_t                              bufferLength;
    PHIDGUARDIAN_SET_CREATE_REQUEST     pSetCreateRequest;
    WDFDEVICE                           device;
    WDFREQUEST                          authRequest;
    PDEVICE_CONTEXT                     pDeviceCtx;
    WDF_REQUEST_SEND_OPTIONS            options;
    BOOLEAN                             ret;
    PCREATE_REQUEST_CONTEXT             pRequestCtx;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "%!FUNC! Entry");

    device = WdfIoQueueGetDevice(Queue);
    pDeviceCtx = DeviceGetContext(device);

    switch (IoControlCode)
    {
#pragma region IOCTL_HIDGUARDIAN_GET_CREATE_REQUEST

        //
        // Queues an inverted call for the driver to respond 
        // back to the user-mode service.
        // 
    case IOCTL_HIDGUARDIAN_GET_CREATE_REQUEST:

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_QUEUE, ">> IOCTL_HIDGUARDIAN_GET_CREATE_REQUEST");

        status = WdfRequestForwardToIoQueue(Request, pDeviceCtx->InvertedCallQueue);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "WdfRequestForwardToIoQueue failed with status %!STATUS!", status);
            WdfRequestComplete(Request, status);
            return;
        }

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "%!FUNC! Exit (inverted call queued)");

        return;

        break;

#pragma endregion

#pragma region IOCTL_HIDGUARDIAN_SET_CREATE_REQUEST

        //
        // Receives a decision made by the user-mode service.
        // 
    case IOCTL_HIDGUARDIAN_SET_CREATE_REQUEST:

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_QUEUE, ">> IOCTL_HIDGUARDIAN_SET_CREATE_REQUEST");

        status = WdfRequestRetrieveInputBuffer(
            Request,
            sizeof(HIDGUARDIAN_SET_CREATE_REQUEST),
            (void*)&pSetCreateRequest,
            &bufferLength);

        //
        // Validate buffer size of request
        // 
        if (NT_SUCCESS(status) && InputBufferLength == sizeof(HIDGUARDIAN_SET_CREATE_REQUEST))
        {
            //
            // Pop auth request from device queue
            // 
            status = WdfIoQueueRetrieveNextRequest(pDeviceCtx->PendingAuthQueue, &authRequest);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_QUEUE,
                    "WdfIoQueueRetrieveNextRequest (PendingAuthQueue) failed with status %!STATUS!", status);
                break;
            }

            pRequestCtx = CreateRequestGetContext(authRequest);

            //
            // Validate that the response matches the request
            // 
            if (pSetCreateRequest->RequestId != pRequestCtx->RequestId) {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_QUEUE,
                    "Request ID mismatch: %X != %X",
                    pSetCreateRequest->RequestId,
                    pRequestCtx->RequestId);

                status = WdfRequestForwardToIoQueue(Request, pDeviceCtx->PendingAuthQueue);
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_QUEUE,
                        "Failed to re-enqueue request with ID %X", pRequestCtx->RequestId);
                }

                break;
            }

            //
            // Cache result in driver to improve speed
            // 
            if (pSetCreateRequest->IsSticky) {
                if (!PID_LIST_CONTAINS(&pDeviceCtx->StickyPidList, pRequestCtx->ProcessId, NULL)) {
                    PID_LIST_PUSH(&pDeviceCtx->StickyPidList, pRequestCtx->ProcessId, pSetCreateRequest->IsAllowed);
                }
                else {
                    TraceEvents(TRACE_LEVEL_WARNING,
                        TRACE_QUEUE,
                        "Sticky PID %d already present in cache", pRequestCtx->ProcessId);
                }
            }

            //
            // Request was permitted, pass it down the stack
            // 
            if (pSetCreateRequest->IsAllowed) {
                TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_QUEUE,
                    "!! Request %d from PID %d is allowed",
                    pRequestCtx->RequestId,
                    pRequestCtx->ProcessId);

                WdfRequestFormatRequestUsingCurrentType(authRequest);

                //
                // PID is white-listed, pass request down the stack
                // 
                WDF_REQUEST_SEND_OPTIONS_INIT(&options,
                    WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

                ret = WdfRequestSend(authRequest, WdfDeviceGetIoTarget(device), &options);

                //
                // Complete the request on failure
                // 
                if (ret == FALSE) {
                    status = WdfRequestGetStatus(authRequest);
                    TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_QUEUE,
                        "WdfRequestSend failed: %!STATUS!", status);
                    WdfRequestComplete(authRequest, status);
                }
            }
            else {
                TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_QUEUE,
                    "!! Request %d from PID %d is not allowed",
                    pRequestCtx->RequestId,
                    pRequestCtx->ProcessId);

                //
                // Request was denied, complete it with failure
                // 
                WdfRequestComplete(authRequest, STATUS_ACCESS_DENIED);
            }
        }
        else {
            TraceEvents(TRACE_LEVEL_WARNING,
                TRACE_QUEUE,
                "Buffer size mismatch: %d != %d",
                (ULONG)bufferLength,
                (ULONG)sizeof(HIDGUARDIAN_SET_CREATE_REQUEST));
        }

        break;

#pragma endregion

    default:
        WdfRequestFormatRequestUsingCurrentType(Request);

        WDF_REQUEST_SEND_OPTIONS_INIT(&options,
            WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

        ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(WdfIoQueueGetDevice(Queue)), &options);

        if (ret == FALSE) {
            status = WdfRequestGetStatus(Request);
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "WdfRequestSend failed: %!STATUS!", status);
            WdfRequestComplete(Request, status);
        }

        return;
    }

    WdfRequestComplete(Request, status);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "%!FUNC! Exit");
}
