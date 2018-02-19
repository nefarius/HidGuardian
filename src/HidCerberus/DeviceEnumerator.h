#pragma once

#include <Windows.h>
#include <string>
#include <vector>

class DeviceEnumerator
{
public:
    DeviceEnumerator();
    ~DeviceEnumerator();

    static GUID stringToGuid(const std::string& str);
    static std::string guidToString(GUID guid);

    static bool classGuidFromName(const std::string& className, LPGUID classGuid);
    static std::vector<std::string> enumerateDeviceInterface(LPGUID interfaceGuid);
};

