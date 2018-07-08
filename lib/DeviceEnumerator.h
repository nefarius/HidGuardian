#pragma once

#include <Windows.h>
#include <string>
#include <vector>

#define POCO_NO_UNWINDOWS
#include <Poco/RefCountedObject.h>

using Poco::RefCountedObject;

class DeviceEnumerator : public RefCountedObject
{
public:
    DeviceEnumerator();
    
    static GUID stringToGuid(const std::string& str);
    static std::string guidToString(GUID guid);

    static bool classGuidFromName(const std::string& className, LPGUID classGuid);
    static std::vector<std::string> enumerateDeviceInterface(LPGUID interfaceGuid);

protected:
    ~DeviceEnumerator();
};

