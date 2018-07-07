#pragma once

#include "CoreClrHost.h"
#include "ControlDevice.h"

#define POCO_NO_UNWINDOWS
#include <Poco/Util/ServerApplication.h>
#include <Poco/TaskManager.h>
#include <Poco/AutoPtr.h>

using Poco::TaskManager;
using Poco::AutoPtr;

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

class ZerberusService :
    public Poco::Util::ServerApplication
{
    TaskManager _taskManager;
    AutoPtr<CoreClrHost> _clrHost;
    AutoPtr<ControlDevice> _controlDevice;

    void onDeviceArrived(const void* pSender, std::string& name);
    void onDeviceRemoved(const void* pSender, std::string& name);

protected:
    int main(const std::vector<std::string>& args);
public:
    ZerberusService() {};
    ~ZerberusService() {};
};

