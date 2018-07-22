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
#pragma alloc_text (PAGE, PendingAuthQueueInitialize)
#pragma alloc_text (PAGE, PendingCreateRequestsQueueInitialize)
#pragma alloc_text (PAGE, CreateRequestsQueueInitialize)
#pragma alloc_text (PAGE, EvtWdfCreateRequestsQueueIoDefault)
#endif

NTSTATUS
HidGuardianQueueInitialize(
    _In_ WDFDEVICE Device
)
{
    WDFQUEUE                queue;
    NTSTATUS                status;
    WDF_IO_QUEUE_CONFIG     queueConfig;


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

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_QUEUE,
            "WdfIoQueueCreate failed %!STATUS!", status);
        return status;
    }

    return status;
}

NTSTATUS
PendingAuthQueueInitialize(
    _In_ WDFDEVICE hDevice
)
{
    WDF_IO_QUEUE_CONFIG     queueConfig;
    PDEVICE_CONTEXT         pDeviceCtx;

    PAGED_CODE();

    pDeviceCtx = DeviceGetContext(hDevice);

    WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);

    return WdfIoQueueCreate(hDevice,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &pDeviceCtx->PendingAuthQueue
    );
}

NTSTATUS
PendingCreateRequestsQueueInitialize(
    _In_ WDFDEVICE hDevice
)
{
    WDF_IO_QUEUE_CONFIG     queueConfig;
    PDEVICE_CONTEXT         pDeviceCtx;

    PAGED_CODE();

    pDeviceCtx = DeviceGetContext(hDevice);

    WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);

    return WdfIoQueueCreate(hDevice,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &pDeviceCtx->PendingCreateRequestsQueue
    );
}

NTSTATUS
CreateRequestsQueueInitialize(
    _In_ WDFDEVICE hDevice
)
{
    NTSTATUS                status;
    WDF_IO_QUEUE_CONFIG     queueConfig;
    PDEVICE_CONTEXT         pDeviceCtx;

    PAGED_CODE();

    pDeviceCtx = DeviceGetContext(hDevice);

    WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchSequential);
    queueConfig.EvtIoDefault = EvtWdfCreateRequestsQueueIoDefault;

    status = WdfIoQueueCreate(hDevice,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &pDeviceCtx->CreateRequestsQueue
    );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_QUEUE,
            "WdfIoQueueCreate (CreateRequestsQueue) failed with %!STATUS!", status);
        return status;
    }

    return WdfDeviceConfigureRequestDispatching(hDevice,
        pDeviceCtx->CreateRequestsQueue,
        WdfRequestTypeCreate
    );
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
EvtWdfCreateRequestsQueueIoDefault(
    WDFQUEUE  Queue,
    WDFREQUEST  Request
)
{
    NTSTATUS                    status;
    WDFDEVICE                   device;
    PDEVICE_CONTEXT             pDeviceCtx;
    DWORD                       pid;
    BOOLEAN                     allowed;
    BOOLEAN                     ret;
    WDF_REQUEST_SEND_OPTIONS    options;
    WDF_OBJECT_ATTRIBUTES       requestAttribs;
    PCREATE_REQUEST_CONTEXT     pRequestCtx;
    PCONTROL_DEVICE_CONTEXT     pControlCtx;


    PAGED_CODE();

    UNREFERENCED_PARAMETER(Request);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "%!FUNC! Entry");

    device = WdfIoQueueGetDevice(Queue);
    pDeviceCtx = DeviceGetContext(device);
    pControlCtx = ControlDeviceGetContext(ControlDevice);
    pid = CURRENT_PROCESS_ID();

    if (pControlCtx->IsCerberusConnected == TRUE
        && PID_LIST_CONTAINS(&pControlCtx->SystemPidList, pid, NULL))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_QUEUE,
            "Request belongs to system PID %d, allowing access",
            pid);

        goto allowAccess;
    }

    //
    // If this is Cerberus, allow
    // 
    if (pControlCtx->IsCerberusConnected == TRUE
        && pControlCtx->CerberusPid == pid)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_QUEUE,
            "Cerberus (PID %d) connected, access granted",
            pid);

        goto allowAccess;
    }

    //
    // Check PID against internal list to speed up validation
    // 
    if (PID_LIST_CONTAINS(&pDeviceCtx->StickyPidList, pid, &allowed)) {
        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_QUEUE,
            "Request belongs to sticky PID %d, processing",
            pid);

        if (allowed) {
            //
            // Sticky PID allowed, forward request instantly
            // 
            goto allowAccess;
        }
        else {
            //
            // Sticky PID denied, fail request instantly
            // 
            goto blockAccess;
        }
    }

    //
    // No Cerberus, so default actions apply
    // 
    if (!pControlCtx->IsCerberusConnected) {
        TraceEvents(TRACE_LEVEL_VERBOSE,
            TRACE_QUEUE,
            "Cerberus not present (PID %d), applying default action",
            pid);
        goto defaultAction;
    }

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&requestAttribs, CREATE_REQUEST_CONTEXT);
    requestAttribs.ParentObject = Request;

    //
    // Add custom context to request so we can validate it later
    // 
    status = WdfObjectAllocateContext(
        Request,
        &requestAttribs,
        (PVOID)&pRequestCtx
    );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_QUEUE,
            "WdfObjectAllocateContext failed with status %!STATUS!", status);

        goto defaultAction;
    }

    pRequestCtx->ProcessId = CURRENT_PROCESS_ID();

    status = WdfRequestForwardToIoQueue(Request, pDeviceCtx->PendingCreateRequestsQueue);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_QUEUE,
            "WdfRequestForwardToIoQueue failed with status %!STATUS!", status);

        goto defaultAction;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "%!FUNC! Exit (access pending)");

    return;

defaultAction:

    if (pDeviceCtx->AllowByDefault) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "Default action requested: allow");
        goto allowAccess;
    }
    else {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "Default action requested: deny");
        goto blockAccess;
    }

allowAccess:

    WdfRequestFormatRequestUsingCurrentType(Request);

    //
    // Send request down the stack
    // 
    WDF_REQUEST_SEND_OPTIONS_INIT(&options,
        WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

    ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(device), &options);

    if (ret == FALSE) {
        status = WdfRequestGetStatus(Request);
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_QUEUE,
            "WdfRequestSend failed: %!STATUS!", status);
        WdfRequestComplete(Request, status);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "%!FUNC! Exit (access granted)");

    return;

blockAccess:

    //
    // If forwarding fails, fall back to blocking access
    // 
    WdfRequestComplete(Request, STATUS_ACCESS_DENIED);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "%!FUNC! Exit (access blocked)");
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
    NTSTATUS                            status;
    size_t                              bufferLength;
    PHIDGUARDIAN_GET_CREATE_REQUEST     pGetCreateRequest;
    PHIDGUARDIAN_SET_CREATE_REQUEST     pSetCreateRequest;
    WDFDEVICE                           device;
    WDFREQUEST                          authRequest;
    WDFREQUEST                          createRequest;
    WDFREQUEST                          tagRequest;
    WDFREQUEST                          prevTagRequest;
    PDEVICE_CONTEXT                     pDeviceCtx;
    WDF_REQUEST_SEND_OPTIONS            options;
    BOOLEAN                             ret;
    PCREATE_REQUEST_CONTEXT             pRequestCtx;
    ULONG                               hwidBufferLength;


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

        if (pDeviceCtx->IsShuttingDown) {
            status = STATUS_DEVICE_DOES_NOT_EXIST;
            break;
        }

        //
        // Get buffer of request
        // 
        status = WdfRequestRetrieveOutputBuffer(
            Request,
            sizeof(HIDGUARDIAN_GET_CREATE_REQUEST),
            (void*)&pGetCreateRequest,
            &bufferLength);

        //
        // Validate output buffer
        // 
        if (!NT_SUCCESS(status) || bufferLength != pGetCreateRequest->Size)
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "Packet size mismatch: %d != %d", (ULONG)bufferLength, pGetCreateRequest->Size);

            break;
        }

        //
        // Pop pending create request
        // 
        status = WdfIoQueueRetrieveNextRequest(pDeviceCtx->PendingCreateRequestsQueue, &createRequest);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_WARNING,
                TRACE_QUEUE,
                "WdfIoQueueRetrieveNextRequest (PendingCreateRequestsQueue) failed with status %!STATUS!", status);
            break;
        }

        pRequestCtx = CreateRequestGetContext(createRequest);

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_DEVICE,
            "Request ID: %d",
            pGetCreateRequest->RequestId);

        pGetCreateRequest->ProcessId = pRequestCtx->ProcessId;

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_DEVICE,
            "PID associated to this request: %d",
            pGetCreateRequest->ProcessId);

        wcscpy_s(pGetCreateRequest->DeviceId, MAX_DEVICE_ID_SIZE, pDeviceCtx->DeviceID);
        wcscpy_s(pGetCreateRequest->InstanceId, MAX_INSTANCE_ID_SIZE, pDeviceCtx->InstanceID);

        hwidBufferLength = pGetCreateRequest->Size - sizeof(HIDGUARDIAN_GET_CREATE_REQUEST);

        TraceEvents(TRACE_LEVEL_VERBOSE,
            TRACE_DEVICE,
            "Size for string: %d", hwidBufferLength);

        if (hwidBufferLength >= pDeviceCtx->HardwareIDsLength)
        {
            RtlCopyMemory(
                pGetCreateRequest->HardwareIds,
                pDeviceCtx->HardwareIDs,
                pDeviceCtx->HardwareIDsLength
            );
        }

        //
        // Pass identification information to request context
        // 
        pRequestCtx->ProcessId = pGetCreateRequest->ProcessId;
        pRequestCtx->RequestId = pGetCreateRequest->RequestId;

        //
        // Information has been passed to user-land, queue this request for 
        // later confirmation (or block) action.
        // 
        status = WdfRequestForwardToIoQueue(createRequest, pDeviceCtx->PendingAuthQueue);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_DEVICE,
                "WdfRequestForwardToIoQueue failed with status %!STATUS!", status);

            break;
        }

        WdfRequestCompleteWithInformation(Request, status, bufferLength);

        return;

#pragma endregion

#pragma region IOCTL_HIDGUARDIAN_SET_CREATE_REQUEST

        //
        // Receives a decision made by the user-mode service.
        // 
    case IOCTL_HIDGUARDIAN_SET_CREATE_REQUEST:

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_QUEUE, ">> IOCTL_HIDGUARDIAN_SET_CREATE_REQUEST");

        if (pDeviceCtx->IsShuttingDown) {
            status = STATUS_DEVICE_DOES_NOT_EXIST;
            break;
        }

        status = WdfRequestRetrieveInputBuffer(
            Request,
            sizeof(HIDGUARDIAN_SET_CREATE_REQUEST),
            (void*)&pSetCreateRequest,
            &bufferLength);

        //
        // Validate buffer size of request
        // 
        if (!NT_SUCCESS(status) || InputBufferLength != sizeof(HIDGUARDIAN_SET_CREATE_REQUEST))
        {
            TraceEvents(TRACE_LEVEL_WARNING,
                TRACE_QUEUE,
                "Buffer size mismatch: %d != %d",
                (ULONG)bufferLength,
                (ULONG)sizeof(HIDGUARDIAN_SET_CREATE_REQUEST));

            break;
        }

        prevTagRequest = tagRequest = NULL;

        //
        // The access request response might come in out of sync (because the
        // user-mode components process them in an asynchronous fashion) so we
        // need to loop through the pending requests queue and validate the 
        // assigned ID value to match this request to the pending request.
        // 
        do 
        {
            //
            // Start looking for pending request
            // 
            status = WdfIoQueueFindRequest(
                pDeviceCtx->PendingAuthQueue,
                prevTagRequest,
                NULL,
                NULL,
                &tagRequest
            );

            //
            // Decrease reference count of previous match
            // 
            if (prevTagRequest) {
                WdfObjectDereference(prevTagRequest);
            }

            //
            // Reached the end of the queue without success
            // 
            if (status == STATUS_NO_MORE_ENTRIES) {
                status = STATUS_UNSUCCESSFUL;
                break;
            }

            if (status == STATUS_NOT_FOUND) {
                //
                // The prevTagRequest request has disappeared from the
                // queue. There might be other requests that match
                // the criteria, so restart the search. 
                //
                prevTagRequest = tagRequest = NULL;
                continue;
            }
            if (!NT_SUCCESS(status)) {
                status = STATUS_UNSUCCESSFUL;
                break;
            }
            
            //
            // Note: this is allowed but only read from the context
            // since at this point we don't own the request object!
            // 
            pRequestCtx = CreateRequestGetContext(tagRequest);

            //
            // Validate the request ID
            // 
            if (pSetCreateRequest->RequestId == pRequestCtx->RequestId) {
                //
                // Found a match. Retrieve the request from the queue.
                //
                status = WdfIoQueueRetrieveFoundRequest(
                    Queue,
                    tagRequest,
                    &authRequest
                );
                WdfObjectDereference(tagRequest);
                if (status == STATUS_NOT_FOUND) {
                    //
                    // The tagRequest request has disappeared from the
                    // queue. There might be other requests that match 
                    // the criteria, so restart the search. 
                    //
                    prevTagRequest = tagRequest = NULL;
                    continue;
                }
                if (!NT_SUCCESS(status)) {
                    status = STATUS_UNSUCCESSFUL;
                    break;
                }

                //
                // Found the request.
                //
                status = STATUS_SUCCESS;
                pRequestCtx = CreateRequestGetContext(authRequest);

                //
                // Cache result in driver to improve speed
                // 
                if (pSetCreateRequest->IsSticky) {
                    if (!PID_LIST_CONTAINS(
                        &pDeviceCtx->StickyPidList, 
                        pRequestCtx->ProcessId,
                        NULL
                    )) {
                        PID_LIST_PUSH(
                            &pDeviceCtx->StickyPidList, 
                            pRequestCtx->ProcessId, 
                            pSetCreateRequest->IsAllowed
                        );
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

                break;
            }

            //
            // This request is not the correct one. Drop the reference 
            // on the tagRequest after the driver obtains the next request.
            //
            prevTagRequest = tagRequest;
            continue;

        } while (TRUE);

        //
        // Trace error
        // 
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "Processing request with ID %X failed with status %!STATUS!", 
                pSetCreateRequest->RequestId,
                status
            );
            break;
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

    if (status != STATUS_PENDING) {
        WdfRequestComplete(Request, status);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "%!FUNC! Exit (default)");
}

