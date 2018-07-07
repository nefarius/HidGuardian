#pragma once

#include "HidCerberus.h"

#define POCO_NO_UNWINDOWS
#include <Poco/SharedPtr.h>
#include <Poco/TaskManager.h>

typedef struct _HC_HANDLE_T
{
    Poco::SharedPtr<Poco::TaskManager> MainManager;

    PFN_HC_PROCESS_ACCESS_REQUEST EvtProcessAccessRequest;

} HC_HANDLE;
