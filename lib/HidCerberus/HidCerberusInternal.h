#pragma once

#include "HidCerberus.h"
#include "GuardedDevice.h"

#define POCO_NO_UNWINDOWS
#include <Poco/SharedPtr.h>
#include <Poco/TaskManager.h>

#include <atomic>

typedef struct _HC_HANDLE_T
{
    Poco::SharedPtr<Poco::TaskManager> MainManager;

    PFN_HC_PROCESS_ACCESS_REQUEST EvtProcessAccessRequest;

} HC_HANDLE;

typedef struct _HC_ARE_HANDLE_T
{
	ULONG RequestId;

	GuardedDevice* ParentDevice;

    std::atomic<bool> IsHandled;

} HC_ARE_HANDLE;
