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
    // Set exclusive to false so that more than one app can talk to the
    // control device simultaneously.
    //
    WdfDeviceInitSetExclusive(pInit, FALSE);

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

    // TODO: do we need this?
    WDF_FILEOBJECT_CONFIG_INIT(&foCfg, NULL, NULL, HidGuardianSidebandFileCleanup);
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

    //
    // Configure the default queue associated with the control device object
    // to be Serial so that request passed to EvtIoDeviceControl are serialized.
    //

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig,
        WdfIoQueueDispatchSequential);

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

#pragma region Create PendingAuthQueue I/O Queue

    WDF_IO_QUEUE_CONFIG_INIT(&ioQueueConfig, WdfIoQueueDispatchManual);

    status = WdfIoQueueCreate(controlDevice,
        &ioQueueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &pControlCtx->PendingAuthQueue
    );
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_SIDEBAND,
            "WdfIoQueueCreate (PendingAuthQueue) failed with %!STATUS!", status);
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
    size_t                              transferred = 0;
    PCONTROL_DEVICE_CONTEXT             pControlCtx;
    PHIDGUARDIAN_SET_CREATE_REQUEST     pSetCreateRequest;

    pControlCtx = ControlDeviceGetContext(WdfIoQueueGetDevice(Queue));

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(transferred);

    switch (IoControlCode)
    {
#pragma region IOCTL_HIDGUARDIAN_GET_CREATE_REQUEST

    case IOCTL_HIDGUARDIAN_GET_CREATE_REQUEST:

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_SIDEBAND, "IOCTL_HIDGUARDIAN_GET_CREATE_REQUEST");

        status = WdfRequestForwardToIoQueue(Request, pControlCtx->InvertedCallQueue);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_SIDEBAND, 
                "WdfRequestForwardToIoQueue failed with status %!STATUS!", status);
            WdfRequestComplete(Request, status);
            return;
        }

        return;

        /*
        status = WdfRequestRetrieveOutputBuffer(
            Request,
            sizeof(HIDGUARDIAN_GET_CREATE_REQUEST),
            (void*)&pGetCreateRequest,
            &bufferLength);

        if (NT_SUCCESS(status) && OutputBufferLength == sizeof(HIDGUARDIAN_GET_CREATE_REQUEST))
        {
            transferred = OutputBufferLength;
            //RtlCopyMemory(&pGetHostAddr->Host, &pDeviceContext->HostAddress, sizeof(BD_ADDR));
        }
        */

        break;

#pragma endregion

#pragma region IOCTL_HIDGUARDIAN_SET_CREATE_REQUEST

    case IOCTL_HIDGUARDIAN_SET_CREATE_REQUEST:

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_SIDEBAND, "IOCTL_HIDGUARDIAN_SET_CREATE_REQUEST");

        status = WdfRequestRetrieveInputBuffer(
            Request,
            sizeof(HIDGUARDIAN_SET_CREATE_REQUEST),
            (void*)&pSetCreateRequest,
            &bufferLength);

        if (NT_SUCCESS(status) && InputBufferLength == sizeof(HIDGUARDIAN_SET_CREATE_REQUEST))
        {

        }

        break;

#pragma endregion

    default:
        break;
    }

    WdfRequestComplete(Request, status);
}
#pragma warning(pop) // enable 28118 again

_Use_decl_annotations_
VOID
HidGuardianSidebandFileCleanup(
    WDFFILEOBJECT  FileObject
)
{
    UNREFERENCED_PARAMETER(FileObject);

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_SIDEBAND,
        "HidGuardianSidebandFileCleanup called");
}

