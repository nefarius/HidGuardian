/*
* Windows kernel-mode driver for controlling access to various input devices.
* Copyright (C) 2016-2018  Benjamin Höglinger-Stelzer
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "Driver.h"
#include "Guardian.tmh"

#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, AmIAffected)
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
    BOOLEAN                 affected = TRUE;
    PCWSTR                  szIter = NULL;

    DECLARE_CONST_UNICODE_STRING(valueExemptedMultiSz, REG_MULTI_SZ_EXCEMPTED_DEVICES);
    DECLARE_UNICODE_STRING_SIZE(currentHardwareID, MAX_HARDWARE_ID_SIZE);
    DECLARE_UNICODE_STRING_SIZE(myHardwareID, MAX_HARDWARE_ID_SIZE);


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

    WdfRegistryClose(keyParams);
    WdfObjectDelete(col);

    //
    // If Hardware ID wasn't found (or Force is disabled), report failure so the filter gets unloaded
    // 
    return (affected) ? STATUS_SUCCESS : STATUS_DEVICE_FEATURE_NOT_SUPPORTED;
}

BOOLEAN AmIMaster(PDEVICE_CONTEXT DeviceContext)
{
    PCWSTR      szIter = NULL;
    NTSTATUS    status;

    DECLARE_CONST_UNICODE_STRING(masterHardwareId, HIDGUARDIAN_HARDWARE_ID);
    DECLARE_UNICODE_STRING_SIZE(myHardwareID, MAX_HARDWARE_ID_SIZE);

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
            return FALSE;
        }

        //
        // Compare to hardware ID of master device
        // 
        if (RtlEqualUnicodeString(&myHardwareID, &masterHardwareId, TRUE)) {
            return TRUE;
        }
    }

    return FALSE;
}
