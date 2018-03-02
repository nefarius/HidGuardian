#include "DeviceEnumerator.h"

#include <Windows.h>
#include <initguid.h>
#include <SetupAPI.h>

#include <locale>
#include <codecvt>

#include <Poco/Logger.h>
#include <Poco/Buffer.h>

using Poco::Logger;
using Poco::Buffer;


DeviceEnumerator::DeviceEnumerator()
{
}


DeviceEnumerator::~DeviceEnumerator()
{
}

/**
 * \fn  GUID DeviceEnumerator::stringToGuid(const std::string & str)
 *
 * \brief   String to unique identifier.
 *
 * \author  Benjamin "Nefarius" Höglinger
 * \date    02.03.2018
 *
 * \param   str The string.
 *
 * \return  A GUID.
 */
GUID DeviceEnumerator::stringToGuid(const std::string & str)
{
    GUID guid;
    sscanf(str.c_str(),
        "{%8x-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx}",
        &guid.Data1, &guid.Data2, &guid.Data3,
        &guid.Data4[0], &guid.Data4[1], &guid.Data4[2], &guid.Data4[3],
        &guid.Data4[4], &guid.Data4[5], &guid.Data4[6], &guid.Data4[7]);

    return guid;
}

/**
 * \fn  std::string DeviceEnumerator::guidToString(GUID guid)
 *
 * \brief   Unique identifier to string.
 *
 * \author  Benjamin "Nefarius" Höglinger
 * \date    02.03.2018
 *
 * \param   guid    Unique identifier.
 *
 * \return  A std::string.
 */
std::string DeviceEnumerator::guidToString(GUID guid)
{
    char guid_cstr[39];
    snprintf(guid_cstr, sizeof(guid_cstr),
        "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

    return std::string(guid_cstr);
}

/**
 * \fn  bool DeviceEnumerator::classGuidFromName(const std::string& className, LPGUID classGuid)
 *
 * \brief   Class unique identifier from name.
 *
 * \author  Benjamin "Nefarius" Höglinger
 * \date    02.03.2018
 *
 * \param   className   Name of the class.
 * \param   classGuid   Unique identifier for the class.
 *
 * \return  True if it succeeds, false if it fails.
 */
bool DeviceEnumerator::classGuidFromName(const std::string& className, LPGUID classGuid)
{
    DWORD requiredSize = 0;

    Buffer<char> machineName(1024);
    requiredSize = static_cast<DWORD>(machineName.size());

    GetComputerNameA(machineName.begin(), &requiredSize);

    return SetupDiClassGuidsFromNameExA(className.c_str(), classGuid, 1, &requiredSize, machineName.begin(), nullptr);
}

/**
 * \fn  std::vector<std::string> DeviceEnumerator::enumerateDeviceInterface(LPGUID interfaceGuid)
 *
 * \brief   Enumerate device interface.
 *
 * \author  Benjamin "Nefarius" Höglinger
 * \date    02.03.2018
 *
 * \param   interfaceGuid   Unique identifier for the interface.
 *
 * \return  A std::vector&lt;std::string&gt;
 */
std::vector<std::string> DeviceEnumerator::enumerateDeviceInterface(LPGUID interfaceGuid)
{
    SP_DEVICE_INTERFACE_DATA deviceInterfaceData = { 0 };
    deviceInterfaceData.cbSize = sizeof(deviceInterfaceData);
    DWORD memberIndex = 0;
    DWORD requiredSize = 0;

    std::vector<std::string> devices;

    auto deviceInfoSet = SetupDiGetClassDevs(interfaceGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    // enumerate device instances
    while (SetupDiEnumDeviceInterfaces(deviceInfoSet, nullptr, interfaceGuid, memberIndex++, &deviceInterfaceData))
    {
        // get required target buffer size
        SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, nullptr, 0, &requiredSize, nullptr);

        // allocate target buffer
        auto detailDataBuffer = static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(malloc(requiredSize));
        detailDataBuffer->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        // get detail buffer
        if (!SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, detailDataBuffer, requiredSize, &requiredSize, nullptr))
        {
            SetupDiDestroyDeviceInfoList(deviceInfoSet);
            free(detailDataBuffer);
            continue;
        }

        using convert_type = std::codecvt_utf8<wchar_t>;
        std::wstring_convert<convert_type, wchar_t> converter;
        std::string devicePath(converter.to_bytes(detailDataBuffer->DevicePath));

        devices.push_back(devicePath);

        free(detailDataBuffer);
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    return devices;
}
