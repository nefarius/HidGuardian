/*
MIT License

Copyright (c) 2018 Benjamin "Nefarius" Höglinger

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


#include "Sideband.h"
#include "Sideband.tmh"
#include <wdmsec.h>


WDFCOLLECTION   FilterDeviceCollection;
WDFWAITLOCK     FilterDeviceCollectionLock;
WDFDEVICE       ControlDevice = NULL;

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, HidGuardianCreateControlDevice)
#pragma alloc_text (PAGE, HidGuardianDeleteControlDevice)
#pragma alloc_text (PAGE, HidGuardianSidebandDeviceFileCreate)
#pragma alloc_text (PAGE, HidGuardianSidebandFileCleanup)
#endif

//
// Creates the control device for sideband communication.
// 
_Use_decl_annotations_
NTSTATUS
HidGuardianCreateControlDevice(
    WDFDEVICE Device
)
{
    PWDFDEVICE_INIT             pInit;
    WDFDEVICE                   controlDevice = NULL;
    WDF_OBJECT_ATTRIBUTES       controlAttributes;
    WDF_IO_QUEUE_CONFIG         ioQueueConfig;
    BOOLEAN                     bCreate = FALSE;
    NTSTATUS                    status;
    WDFQUEUE                    queue;
    WDF_FILEOBJECT_CONFIG       foCfg;
    PCONTROL_DEVICE_CONTEXT     pControlCtx;

    DECLARE_CONST_UNICODE_STRING(ntDeviceName, NTDEVICE_NAME_STRING);
    DECLARE_CONST_UNICODE_STRING(symbolicLinkName, SYMBOLIC_NAME_STRING);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_SIDEBAND, "%!FUNC! Entry");


    PAGED_CODE();

    //
    // First find out whether any ControlDevice has been created. If the
    // collection has more than one device then we know somebody has already
    // created or in the process of creating the device.
    //
    WdfWaitLockAcquire(FilterDeviceCollectionLock, NULL);

    if (WdfCollectionGetCount(FilterDeviceCollection) == 1) {
        bCreate = TRUE;
    }

    WdfWaitLockRelease(FilterDeviceCollectionLock);

    if (!bCreate) {
        //
        // Control device is already created. So return success.
        //
        return STATUS_SUCCESS;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_SIDEBAND,
        "Creating Control Device");

    //
    //
    // In order to create a control device, we first need to allocate a
    // WDFDEVICE_INIT structure and set all properties.
    //
    pInit = WdfControlDeviceInitAllocate(
        WdfDeviceGetDriver(Device),
        &SDDL_DEVOBJ_SYS_ALL_ADM_ALL    // admin only access
    );

    if (pInit == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_SIDEBAND,
            "WdfControlDeviceInitAllocate failed with %!STATUS!", status);
        goto Error;
    }

    //
    // Only one service may manage us
    // 
    WdfDeviceInitSetExclusive(pInit, TRUE);

    //
    // Assign name to expose
    // 
    status = WdfDeviceInitAssignName(pInit, &ntDeviceName);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_SIDEBAND,
            "WdfDeviceInitAssignName failed with %!STATUS!", status);
        goto Error;
    }

    //
    // Clean-up actions once service disconnects
    // 
    WDF_FILEOBJECT_CONFIG_INIT(
        &foCfg, 
        HidGuardianSidebandDeviceFileCreate,
        NULL, 
        HidGuardianSidebandFileCleanup
    );
    WdfDeviceInitSetFileObjectConfig(pInit, &foCfg, WDF_NO_OBJECT_ATTRIBUTES);

    //
    // Since control device is globally available we can use a device
    // context to handle global data.
    // 
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&controlAttributes, CONTROL_DEVICE_CONTEXT);

    status = WdfDeviceCreate(&pInit,
        &controlAttributes,
        &controlDevice);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_SIDEBAND,
            "WdfDeviceCreate failed with %!STATUS!", status);
        goto Error;
    }

    //
    // Create a symbolic link for the control object so that user-mode can open
    // the device.
    //

    status = WdfDeviceCreateSymbolicLink(controlDevice,
        &symbolicLinkName);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_SIDEBAND,
            "WdfDeviceCreateSymbolicLink failed with %!STATUS!", status);
        goto Error;
    }

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig,
        WdfIoQueueDispatchParallel);

    ioQueueConfig.EvtIoDeviceControl = HidGuardianSidebandIoDeviceControl;

    //
    // Framework by default creates non-power managed queues for
    // filter drivers.
    //
    status = WdfIoQueueCreate(controlDevice,
        &ioQueueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queue // pointer to default queue
    );
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_SIDEBAND,
            "WdfIoQueueCreate (Default) failed with %!STATUS!", status);
        goto Error;
    }

    pControlCtx = ControlDeviceGetContext(controlDevice);

    TraceEvents(TRACE_LEVEL_VERBOSE,
        TRACE_SIDEBAND,
        "ControlDeviceGetContext = 0x%p", pControlCtx);

#pragma region Create InvertedCallQueue I/O Queue

    WDF_IO_QUEUE_CONFIG_INIT(&ioQueueConfig, WdfIoQueueDispatchManual);

    status = WdfIoQueueCreate(controlDevice,
        &ioQueueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &pControlCtx->InvertedCallQueue
    );
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_SIDEBAND,
            "WdfIoQueueCreate (InvertedCallQueue) failed with %!STATUS!", status);
        goto Error;
    }

#pragma endregion

    //
    // Control devices must notify WDF when they are done initializing.   I/O is
    // rejected until this call is made.
    //
    WdfControlFinishInitializing(controlDevice);

    ControlDevice = controlDevice;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_SIDEBAND, "%!FUNC! Exit");

    return STATUS_SUCCESS;

Error:

    if (pInit != NULL) {
        WdfDeviceInitFree(pInit);
    }

    if (controlDevice != NULL) {
        //
        // Release the reference on the newly created object, since
        // we couldn't initialize it.
        //
        WdfObjectDelete(controlDevice);
        controlDevice = NULL;
    }

    TraceEvents(TRACE_LEVEL_ERROR, TRACE_SIDEBAND, "%!FUNC! Exit - failed with status %!STATUS!", status);

    return status;
}

//
// Deletes the control device.
// 
_Use_decl_annotations_
VOID
HidGuardianDeleteControlDevice(
    WDFDEVICE Device
)
{
    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_SIDEBAND,
        "Deleting Control Device");

    if (ControlDevice) {
        WdfObjectDelete(ControlDevice);
        ControlDevice = NULL;
    }
}

//
// Handles requests sent to the sideband control device.
// 
#pragma warning(push)
#pragma warning(disable:28118) // this callback will run at IRQL=PASSIVE_LEVEL
_Use_decl_annotations_
VOID HidGuardianSidebandIoDeviceControl(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode
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

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_SIDEBAND, "%!FUNC! Entry");

    device = WdfIoQueueGetDevice(Queue);
    pDeviceCtx = DeviceGetContext(device);

    UNREFERENCED_PARAMETER(OutputBufferLength);

    switch (IoControlCode)
    {

#pragma region IOCTL_HIDGUARDIAN_SET_CREATE_REQUEST

        //
        // Receives a decision made by the user-mode service.
        // 
    case IOCTL_HIDGUARDIAN_SET_CREATE_REQUEST:

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_SIDEBAND, "IOCTL_HIDGUARDIAN_SET_CREATE_REQUEST");

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
                    TRACE_SIDEBAND,
                    "WdfIoQueueRetrieveNextRequest (PendingAuthQueue) failed with status %!STATUS!", status);
                break;
            }

            pRequestCtx = CreateRequestGetContext(authRequest);

            //
            // Validate that the response matches the request
            // 
            if (pSetCreateRequest->RequestId != pRequestCtx->RequestId) {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_SIDEBAND,
                    "Request ID mismatch: %X != %X",
                    pSetCreateRequest->RequestId,
                    pRequestCtx->RequestId);
                
                status = WdfRequestForwardToIoQueue(Request, pDeviceCtx->PendingAuthQueue);
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_SIDEBAND,
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
                        TRACE_SIDEBAND,
                        "Sticky PID %d already present in cache", pRequestCtx->ProcessId);
                }
            }

            //
            // Request was permitted, pass it down the stack
            // 
            if (pSetCreateRequest->IsAllowed) {
                TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_SIDEBAND,
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
                        TRACE_SIDEBAND,
                        "WdfRequestSend failed: %!STATUS!", status);
                    WdfRequestComplete(authRequest, status);
                }
            }
            else {
                TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_SIDEBAND,
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
                TRACE_SIDEBAND,
                "Buffer size mismatch: %d != %d",
                (ULONG)bufferLength,
                (ULONG)sizeof(HIDGUARDIAN_SET_CREATE_REQUEST));
        }

        break;

#pragma endregion

    default:
        break;
    }

    WdfRequestComplete(Request, status);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_SIDEBAND, "%!FUNC! Exit");
}
#pragma warning(pop) // enable 28118 again

_Use_decl_annotations_
VOID
HidGuardianSidebandDeviceFileCreate(
    WDFDEVICE  Device,
    WDFREQUEST  Request,
    WDFFILEOBJECT  FileObject
)
{
    PCONTROL_DEVICE_CONTEXT pControlCtx;

    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(FileObject);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_SIDEBAND, "%!FUNC! Entry");

    pControlCtx = ControlDeviceGetContext(ControlDevice);

    pControlCtx->IsServicePresent = TRUE;

    WdfRequestComplete(Request, STATUS_SUCCESS);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_SIDEBAND, "%!FUNC! Exit");
}

_Use_decl_annotations_
VOID
HidGuardianSidebandFileCleanup(
    WDFFILEOBJECT  FileObject
)
{
    PCONTROL_DEVICE_CONTEXT     pControlCtx;
    ULONG                       index;
    WDFDEVICE                   device;
    PDEVICE_CONTEXT             pDeviceCtx;

    UNREFERENCED_PARAMETER(FileObject);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_SIDEBAND, "%!FUNC! Entry");

    pControlCtx = ControlDeviceGetContext(ControlDevice);

    pControlCtx->IsServicePresent = FALSE;

    //
    // Purge & restart queue so no orphaned requests remain
    // 
    WdfIoQueuePurgeSynchronously(pControlCtx->InvertedCallQueue);
    WdfIoQueueStart(pControlCtx->InvertedCallQueue);

    WdfWaitLockAcquire(FilterDeviceCollectionLock, NULL);

    //
    // Search for our device in the collection to get index
    // 
    for (
        index = 0;
        index < WdfCollectionGetCount(FilterDeviceCollection);
        index++
        )
    {
        //
        // Get device & context
        // 
        device = WdfCollectionGetItem(FilterDeviceCollection, index);
        pDeviceCtx = DeviceGetContext(device);

        //
        // Purge & restart queue so no orphaned requests remain
        // 
        WdfIoQueuePurgeSynchronously(pDeviceCtx->PendingAuthQueue);
        WdfIoQueueStart(pDeviceCtx->PendingAuthQueue);
    }

    WdfWaitLockRelease(FilterDeviceCollectionLock);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_SIDEBAND, "%!FUNC! Exit");
}

