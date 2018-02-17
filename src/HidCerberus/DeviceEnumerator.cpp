#include "DeviceEnumerator.h"

#include <Windows.h>
#include <SetupAPI.h>
#pragma comment(lib, "setupapi.lib")

#include <hidsdi.h>
#pragma comment(lib, "hid.lib")

#include <locale>
#include <codecvt>

#include <Poco/Logger.h>

using Poco::Logger;


DeviceEnumerator::DeviceEnumerator()
{
    auto& logger = Logger::get(std::string(typeid(this).name()) + std::string("::") + std::string(__func__));

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData = { 0 };
    deviceInterfaceData.cbSize = sizeof(deviceInterfaceData);
    DWORD memberIndex = 0;
    DWORD requiredSize = 0;

    GUID classGuid;

    HidD_GetHidGuid(&classGuid);

    auto deviceInfoSet = SetupDiGetClassDevs(&classGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    // enumerate device instances
    while (SetupDiEnumDeviceInterfaces(deviceInfoSet, nullptr, &classGuid, memberIndex++, &deviceInterfaceData))
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

        //detailDataBuffer->DevicePath

        using convert_type = std::codecvt_utf8<wchar_t>;
        std::wstring_convert<convert_type, wchar_t> converter;
        std::string devicePath(converter.to_bytes(detailDataBuffer->DevicePath));

        logger.information("DevicePath: %s", devicePath);
        
        free(detailDataBuffer);
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
}


DeviceEnumerator::~DeviceEnumerator()
{
}
