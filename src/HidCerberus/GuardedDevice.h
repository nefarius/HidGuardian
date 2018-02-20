#pragma once

#include "PermissionRequestWorker.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <vector>

#define POCO_NO_UNWINDOWS
#include <Poco/RefCountedObject.h>
#include <Poco/Util/LayeredConfiguration.h>
#include <Poco/SharedPtr.h>
#include <Poco/ThreadPool.h>
#include <Poco/AutoPtr.h>
#include <Poco/Data/Session.h>

using Poco::RefCountedObject;
using Poco::Util::LayeredConfiguration;
using Poco::SharedPtr;
using Poco::ThreadPool;
using Poco::AutoPtr;
using Poco::Data::Session;

class GuardedDevice : public RefCountedObject
{
    std::string _devicePath;
    HANDLE _deviceHandle = INVALID_HANDLE_VALUE;
    const LayeredConfiguration& _config;
    int _workerCount;
    SharedPtr<ThreadPool> _workerPool;
    std::vector<AutoPtr<PermissionRequestWorker>> _workers;
    const Session& _session;
public:
    GuardedDevice(const std::string& devicePath, const LayeredConfiguration& config, const Session& session);
    
    void open();
    void guard();

protected:
    ~GuardedDevice();
};

