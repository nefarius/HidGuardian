#pragma once

#define POCO_NO_UNWINDOWS
#include <Poco/Runnable.h>
#include <Poco/Random.h>
#include <Poco/Data/Session.h>

using Poco::Random;
using Poco::Data::Session;

class PermissionRequestWorker : 
    public Poco::Runnable
{
    HANDLE _controlDevice;
    Random _rnd;
    Session _session;

public:
    PermissionRequestWorker(HANDLE controlDevice, const Session& dbSession) 
        : _controlDevice(controlDevice), _session(dbSession)
    {
    }
    ~PermissionRequestWorker() {};

    void run() override;
};

