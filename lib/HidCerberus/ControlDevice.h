#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>

#define POCO_NO_UNWINDOWS
#include <Poco/RefCountedObject.h>

using Poco::RefCountedObject;

class ControlDevice : public RefCountedObject
{
    std::string _devicePath;
    HANDLE _deviceHandle = INVALID_HANDLE_VALUE;
public:
    ControlDevice(std::string devicePath);

protected:
    ~ControlDevice();
};

