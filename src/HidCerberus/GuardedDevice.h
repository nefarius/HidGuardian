#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>

class GuardedDevice
{
    std::string _devicePath;
    HANDLE _deviceHandle;
public:
    GuardedDevice(const std::string& devicePath);
    ~GuardedDevice();

    void open();
};

