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


#pragma once

#include "driver.h"


typedef struct _CONTROL_DEVICE_CONTEXT
{
    //
    // Process ID of Cerberus (if connected)
    // 
    ULONG           CerberusPid;

    //
    // State of Cerberus connection
    // 
    BOOLEAN         IsCerberusConnected;

    //
    // List if privileged processes who will never get blocked
    //
    PPID_LIST_NODE  SystemPidList;

    WDFQUEUE        DeviceArrivalNotificationQueue;

} CONTROL_DEVICE_CONTEXT, *PCONTROL_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CONTROL_DEVICE_CONTEXT, ControlDeviceGetContext)


EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL HidGuardianSidebandIoDeviceControl;
EVT_WDF_DEVICE_FILE_CREATE HidGuardianSidebandDeviceFileCreate;
EVT_WDF_FILE_CLEANUP HidGuardianSidebandFileCleanup;

_Must_inspect_result_
_Success_(return == STATUS_SUCCESS)
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
HidGuardianCreateControlDevice(
    WDFDEVICE Device
);

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
HidGuardianDeleteControlDevice(
    WDFDEVICE Device
);
