#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>

class ServiceEnumerator
{
    ServiceEnumerator() {}

public:
    static DWORD processIdFromServiceName(const std::string& serviceName);
    static DWORD processIdFromProcessName(const std::string& processname);
};

