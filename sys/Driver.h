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


#define INITGUID

#include <ntddk.h>
#include <wdf.h>

#include "HidGuardian.h"
#include "PidList.h"
#include "Sideband.h"
#include "device.h"
#include "queue.h"
#include "Guardian.h"
#include "trace.h"

#define DRIVERNAME "HidGuardian: "

WDFCOLLECTION   FilterDeviceCollection;
WDFWAITLOCK     FilterDeviceCollectionLock;
WDFDEVICE       ControlDevice;

EXTERN_C_START

//
// WDFDRIVER Events
//

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD HidGuardianEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP HidGuardianEvtDriverContextCleanup;

EXTERN_C_END
