#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string>
#include <vector>

#include "HidCerberus.h"

#define POCO_NO_UNWINDOWS
#include <Poco/Task.h>
#include <Poco/AutoPtr.h>

using Poco::Task;
using Poco::AutoPtr;

class GuardedDevice : public Task
{
    std::string _devicePath;
    HANDLE _deviceHandle = INVALID_HANDLE_VALUE;
    const int _bufferSize = 1024;
    PHC_HANDLE _hcHandle;

public:
    GuardedDevice(std::string devicePath, PHC_HANDLE handle);

    void runTask() override;

    void submitAccessRequestResult(ULONG Id, BOOL IsAllowed, BOOL IsPermanent);

protected:
    ~GuardedDevice();
};

typedef void (GuardedDevice::*fpnSubmitAccessRequestResult)(ULONG Id, BOOL IsAllowed, BOOL IsPermanent);

