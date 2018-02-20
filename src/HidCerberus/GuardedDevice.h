#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>

#define POCO_NO_UNWINDOWS
#include <Poco/RefCountedObject.h>

using Poco::RefCountedObject;

class GuardedDevice : public RefCountedObject
{
    std::string _devicePath;
    HANDLE _deviceHandle;
public:
    GuardedDevice(const std::string& devicePath);
    
    void open();

protected:
    ~GuardedDevice();
};

