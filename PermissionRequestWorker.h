#pragma once

#define POCO_NO_UNWINDOWS
#include <Poco/Runnable.h>
#include <Poco/Random.h>

using Poco::Random;

class PermissionRequestWorker : 
    public Poco::Runnable
{
    HANDLE _controlDevice;
    Random _rnd;

public:
    PermissionRequestWorker(HANDLE controlDevice);
    ~PermissionRequestWorker();

    void run() override;
};

