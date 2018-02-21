#pragma once

#include "PermissionRequestWorker.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <vector>

#define POCO_NO_UNWINDOWS
#include <Poco/RefCountedObject.h>
#include <Poco/Util/LayeredConfiguration.h>
#include <Poco/Data/Session.h>
#include <Poco/TaskManager.h>

using Poco::RefCountedObject;
using Poco::Util::LayeredConfiguration;
using Poco::Data::Session;
using Poco::TaskManager;

class GuardedDevice : public RefCountedObject
{
    std::string _devicePath;
    HANDLE _deviceHandle = INVALID_HANDLE_VALUE;
    const LayeredConfiguration& _config;
    const Session& _session;
    TaskManager _taskManager;
public:
    GuardedDevice(const std::string& devicePath, const LayeredConfiguration& config, const Session& session);

protected:
    ~GuardedDevice();
};

