#pragma once

#include "ControlDevice.h"
#include "HidCerberus.h"

#define POCO_NO_UNWINDOWS
#include <Poco/Util/ServerApplication.h>
#include <Poco/TaskManager.h>
#include <Poco/AutoPtr.h>
#include <Poco/Task.h>
#include <Poco/ThreadPool.h>
#include <Poco/SharedPtr.h>

using Poco::TaskManager;
using Poco::AutoPtr;
using Poco::ThreadPool;
using Poco::SharedPtr;

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

class ZerberusService :
    public Poco::Task
{
    ThreadPool _threadPool;
    SharedPtr<TaskManager> _taskManager;
    AutoPtr<ControlDevice> _controlDevice;
    PHC_HANDLE _hcHandle;

    void onDeviceArrived(const void* pSender, std::string& name);
    void onDeviceRemoved(const void* pSender, std::string& name);

public:
    ZerberusService(PHC_HANDLE handle);
    ~ZerberusService() {};

    void runTask() override;
};

