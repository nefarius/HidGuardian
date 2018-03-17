#pragma once

#include "CoreClrHost.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <vector>

#define POCO_NO_UNWINDOWS
#include <Poco/Task.h>
#include <Poco/Random.h>
#include <Poco/AutoPtr.h>

using Poco::Task;
using Poco::Random;
using Poco::AutoPtr;

class GuardedDevice : public Task
{
    std::string _devicePath;
    HANDLE _deviceHandle = INVALID_HANDLE_VALUE;
    Random _rnd;
    const int _bufferSize = 1024;
    AutoPtr<CoreClrHost> _clrHost;
public:
    GuardedDevice(std::string devicePath, AutoPtr<CoreClrHost> clrHost);

    void runTask() override;

protected:
    ~GuardedDevice();
};

