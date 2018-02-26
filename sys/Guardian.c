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


#include "Driver.h"
#include "Guardian.tmh"

#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, AmIAffected)
#pragma alloc_text (PAGE, GetDefaultAction)
#endif

//
// Checks if the current device should be intercepted or not.
// 
NTSTATUS AmIAffected(PDEVICE_CONTEXT DeviceContext)
{
    WDF_OBJECT_ATTRIBUTES   stringAttributes;
    WDFCOLLECTION           col;
    NTSTATUS                status;
    ULONG                   i;
    WDFKEY                  keyParams;
    BOOLEAN                 affected = FALSE;
    ULONG                   force = 0;
    PCWSTR                  szIter = NULL;

    DECLARE_CONST_UNICODE_STRING(valueAffectedMultiSz, L"AffectedDevices");
    DECLARE_CONST_UNICODE_STRING(valueForceUl, L"Force");
    DECLARE_CONST_UNICODE_STRING(valueExemptedMultiSz, L"ExemptedDevices");
    DECLARE_UNICODE_STRING_SIZE(currentHardwareID, MAX_HARDWARE_ID_SIZE);
    DECLARE_UNICODE_STRING_SIZE(myHardwareID, MAX_HARDWARE_ID_SIZE);
    DECLARE_CONST_UNICODE_STRING(ctrlHardwareId, L"Root\\HidGuardian");


    PAGED_CODE();

    //
    // Create collection holding the Hardware IDs
    // 
    status = WdfCollectionCreate(
        NULL,
        &col
    );
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_GUARDIAN,
            "WdfCollectionCreate failed: %!STATUS!", status);
        return status;
    }

    //
    // Get the filter drivers Parameter key
    // 
    status = WdfDriverOpenParametersRegistryKey(WdfGetDriver(), STANDARD_RIGHTS_ALL, WDF_NO_OBJECT_ATTRIBUTES, &keyParams);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_GUARDIAN,
            "WdfDriverOpenParametersRegistryKey failed: %!STATUS!", status);
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&stringAttributes);
    stringAttributes.ParentObject = col;

    //
    // Get the multi-string value for exempted devices
    // 
    status = WdfRegistryQueryMultiString(
        keyParams,
        &valueExemptedMultiSz,
        &stringAttributes,
        col
    );
    if (NT_SUCCESS(status))
    {
        //
        // Walk through devices Hardware IDs
        // 
        for (szIter = DeviceContext->HardwareIDs; *szIter; szIter += wcslen(szIter) + 1)
        {
            //
            // Convert wide into Unicode string
            // 
            status = RtlUnicodeStringInit(&myHardwareID, szIter);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_GUARDIAN,
                    "RtlUnicodeStringInit failed: %!STATUS!", status);

                return status;
            }

            // 
            // Get exempted values
            // 
            for (i = 0; i < WdfCollectionGetCount(col); i++)
            {
                WdfStringGetUnicodeString(WdfCollectionGetItem(col, i), &currentHardwareID);

                TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_GUARDIAN,
                    "My ID %wZ vs current exempted ID %wZ\n", &myHardwareID, &currentHardwareID);

                affected = RtlEqualUnicodeString(&myHardwareID, &currentHardwareID, TRUE);
                TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_GUARDIAN,
                    "Are we exempted: %d\n", affected);

                if (affected)
                {
                    WdfRegistryClose(keyParams);
                    WdfObjectDelete(col);
                    return STATUS_DEVICE_FEATURE_NOT_SUPPORTED;
                }
            }
        }
    }

    //
    // Force loading on every class device
    // 
    status = WdfRegistryQueryULong(keyParams, &valueForceUl, &force);
    if (NT_SUCCESS(status) && force > 0) {
        WdfRegistryClose(keyParams);
        WdfObjectDelete(col);
        return STATUS_SUCCESS;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&stringAttributes);
    stringAttributes.ParentObject = col;

    //
    // Get the multi-string value for affected devices
    // 
    status = WdfRegistryQueryMultiString(
        keyParams,
        &valueAffectedMultiSz,
        &stringAttributes,
        col
    );
    if (NT_SUCCESS(status))
    {
        //
        // Walk through devices Hardware IDs
        // 
        for (szIter = DeviceContext->HardwareIDs; *szIter; szIter += wcslen(szIter) + 1)
        {
            //
            // Convert wide into Unicode string
            // 
            status = RtlUnicodeStringInit(&myHardwareID, szIter);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_GUARDIAN,
                    "RtlUnicodeStringInit failed: %!STATUS!", status);
                return status;
            }

            // 
            // Get affected values
            // 
            for (i = 0; i < WdfCollectionGetCount(col); i++)
            {
                WdfStringGetUnicodeString(WdfCollectionGetItem(col, i), &currentHardwareID);

                TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_GUARDIAN,
                    "My ID %wZ vs current affected ID %wZ\n", &myHardwareID, &currentHardwareID);

                affected = (
                    RtlEqualUnicodeString(&myHardwareID, &currentHardwareID, TRUE)
                    || RtlEqualUnicodeString(&myHardwareID, &ctrlHardwareId, TRUE)
                    );

                TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_GUARDIAN,
                    "Are we affected: %d\n", affected);

                if (affected) break;
            }

            if (affected) break;
        }
    }

    WdfRegistryClose(keyParams);
    WdfObjectDelete(col);

    //
    // If Hardware ID wasn't found (or Force is disabled), report failure so the filter gets unloaded
    // 
    return (affected) ? STATUS_SUCCESS : STATUS_DEVICE_FEATURE_NOT_SUPPORTED;
}

//
// Read the default action (allow or block) from registry.
// 
VOID GetDefaultAction(PDEVICE_CONTEXT DeviceContext)
{
    NTSTATUS status;
    WDFKEY                  keyParams;
    ULONG                   allowByDefault = 0;
    DECLARE_CONST_UNICODE_STRING(allowByDefaultUl, L"AllowByDefault");

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_GUARDIAN, "%!FUNC! Entry");

    //
    // Get the filter drivers Parameter key
    // 
    status = WdfDriverOpenParametersRegistryKey(WdfGetDriver(), STANDARD_RIGHTS_READ, WDF_NO_OBJECT_ATTRIBUTES, &keyParams);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_WARNING,
            TRACE_GUARDIAN,
            "WdfDriverOpenParametersRegistryKey failed: %!STATUS!", status);
        return;
    }

    //
    // Force loading on every class device
    // 
    status = WdfRegistryQueryULong(keyParams, &allowByDefaultUl, &allowByDefault);
    if (NT_SUCCESS(status) && allowByDefault > 0) {
        DeviceContext->AllowByDefault = TRUE;
    }

    WdfRegistryClose(keyParams);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_GUARDIAN, "%!FUNC! Exit");
}

