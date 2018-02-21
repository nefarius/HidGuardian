#pragma once

#define POCO_NO_UNWINDOWS
#include <Poco/Task.h>
#include <Poco/Random.h>
#include <Poco/Data/Session.h>

using Poco::Random;
using Poco::Data::Session;

class PermissionRequestWorker :
    public Poco::Task
{
    HANDLE _deviceHandle;
    Random _rnd;
    Session _session;
    const int _bufferSize = 1024;

public:
    PermissionRequestWorker(HANDLE deviceHandle, const Session& dbSession)
        : _deviceHandle(deviceHandle), _session(dbSession), Task("PermissionRequestWorker")
    {
    }

    void runTask() override;

protected:
    ~PermissionRequestWorker() {}
};

