#pragma once

#define POCO_NO_UNWINDOWS
#include <Poco/Runnable.h>
#include <Poco/Random.h>
#include <Poco/Data/Session.h>
#include <Poco/RefCountedObject.h>

using Poco::Random;
using Poco::Data::Session;
using Poco::RefCountedObject;

class PermissionRequestWorker : 
    public Poco::Runnable, public RefCountedObject
{
    HANDLE _controlDevice;
    Random _rnd;
    Session _session;
    const int _bufferSize = 1024;

public:
    PermissionRequestWorker(HANDLE controlDevice, const Session& dbSession) 
        : _controlDevice(controlDevice), _session(dbSession)
    {
    }
    
    void run() override;

protected:
    ~PermissionRequestWorker() {};
};

