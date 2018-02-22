#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <vector>

#define POCO_NO_UNWINDOWS
#include <Poco/RefCountedObject.h>
#include <Poco/Data/Session.h>
#include <Poco/Task.h>
#include <Poco/Random.h>

using Poco::RefCountedObject;
using Poco::Data::Session;
using Poco::Task;
using Poco::Random;

class GuardedDevice : public Task
{
    std::string _devicePath;
    HANDLE _deviceHandle = INVALID_HANDLE_VALUE;
    Session _session;
    Random _rnd;
    const int _bufferSize = 1024;
public:
    GuardedDevice(std::string devicePath, const Session& session);

    void runTask() override;

protected:
    ~GuardedDevice();
};

