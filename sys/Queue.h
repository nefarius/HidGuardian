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

EXTERN_C_START

NTSTATUS
HidGuardianQueueInitialize(
    _In_ WDFDEVICE hDevice
    );

NTSTATUS
PendingAuthQueueInitialize(
    _In_ WDFDEVICE hDevice
);

NTSTATUS
PendingCreateRequestsQueueInitialize(
    _In_ WDFDEVICE hDevice
);

NTSTATUS
CreateRequestsQueueInitialize(
    _In_ WDFDEVICE hDevice
);

NTSTATUS
NotificationsQueueInitialize(
    _In_ WDFDEVICE hDevice
);

//
// Events from the IoQueue object
//
EVT_WDF_IO_QUEUE_IO_DEFAULT HidGuardianEvtIoDefault;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL HidGuardianEvtIoDeviceControl;
EVT_WDF_IO_QUEUE_IO_DEFAULT EvtWdfCreateRequestsQueueIoDefault;

EXTERN_C_END
