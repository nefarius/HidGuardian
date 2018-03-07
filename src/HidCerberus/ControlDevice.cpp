#include "ControlDevice.h"
#include <utility>

#include <Poco/Logger.h>
#include <devioctl.h>
#include <HidGuardian.h>
#include "ServiceEnumerator.h"


ControlDevice::ControlDevice(std::string devicePath) : _devicePath(std::move(devicePath))
{
    auto& logger = Poco::Logger::get(std::string(typeid(this).name()) + std::string("::") + std::string(__func__));

    logger.debug("Trying to open control device %s", devicePath);

    //
    // Open sideband control device
    // 
    _deviceHandle = CreateFileA(_devicePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH |
        FILE_FLAG_OVERLAPPED,
        nullptr);

    //
    // Validate handle
    // 
    if (_deviceHandle == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
        {
            throw std::runtime_error("Couldn't open the desired device, make sure the provided path is correct.");
        }

        if (GetLastError() == ERROR_ACCESS_DENIED)
        {
            throw std::runtime_error("Couldn't access device, please make sure the device isn't already guarded.");
        }

        throw std::runtime_error("Couldn't access device, unknown error.");
    }

    logger.debug("Device opened");

    DWORD bytesReturned = 0;
    OVERLAPPED lOverlapped = { 0 };
    lOverlapped.hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

#pragma region Whitelist processes

    //
    // Processes to always whitelist to ensure system stability
    // 
    std::vector<std::string> processNames = {
        "dwm.exe"
    };

    const auto submitProcessesSize = sizeof(HIDGUARDIAN_SUBMIT_SYSTEM_PIDS) + (sizeof(ULONG) * processNames.size());
    const auto pSubmitProcesses = static_cast<PHIDGUARDIAN_SUBMIT_SYSTEM_PIDS>(malloc(submitProcessesSize));
    pSubmitProcesses->Size = submitProcessesSize;

    for (unsigned int i = 0; i < processNames.size(); i++)
    {
        const auto name = processNames[i];
        pSubmitProcesses->ProcessIds[i] = ServiceEnumerator::processIdFromProcessName(name);
        logger.debug("Process %s has PID: %lu", name, pSubmitProcesses->ProcessIds[i]);
    }

    DeviceIoControl(
        _deviceHandle,
        IOCTL_HIDGUARDIAN_SUBMIT_SYSTEM_PIDS,
        pSubmitProcesses,
        submitProcessesSize,
        nullptr,
        0,
        &bytesReturned,
        &lOverlapped
    );

    if (GetOverlappedResult(_deviceHandle, &lOverlapped, &bytesReturned, TRUE) == 0)
        throw std::runtime_error("Failed to submit process IDs");

#pragma endregion

#pragma region Whitelist services

    //
    // Windows services to always whitelist to ensure system stability
    // 
    // TODO: can probably be shortened, do more testing
    // 
    std::vector<std::string> serviceNames = {
        "BrokerInfrastructure",
        "DcomLaunch",
        "DeviceInstall",
        "LSM",
        "PlugPlay",
        "Power",
        "SystemEventsBroker",
        "Appinfo",
        "BITS",
        "Browser",
        "DoSvc",
        "iphlpsvc",
        "LanmanServer",
        "lfsvc",
        "ProfSvc",
        "Schedule",
        "SENS",
        "ShellHWDetection",
        "Themes",
        "TokenBroker",
        "UserManager",
        "Winmgmt",
        "wlidsvc",
        "WpnService",
        "wuauserv"
    };

    const auto submitServicesSize = sizeof(HIDGUARDIAN_SUBMIT_SYSTEM_PIDS) + (sizeof(ULONG) * serviceNames.size());
    const auto pSubmitServices = static_cast<PHIDGUARDIAN_SUBMIT_SYSTEM_PIDS>(malloc(submitServicesSize));
    pSubmitServices->Size = submitServicesSize;

    for (unsigned int i = 0; i < serviceNames.size(); i++)
    {
        const auto name = serviceNames[i];
        pSubmitServices->ProcessIds[i] = ServiceEnumerator::processIdFromServiceName(name);
        logger.debug("Service %s has PID: %lu", name, pSubmitServices->ProcessIds[i]);
    }

    DeviceIoControl(
        _deviceHandle,
        IOCTL_HIDGUARDIAN_SUBMIT_SYSTEM_PIDS,
        pSubmitServices,
        submitServicesSize,
        nullptr,
        0,
        &bytesReturned,
        &lOverlapped
    );

    if (GetOverlappedResult(_deviceHandle, &lOverlapped, &bytesReturned, TRUE) == 0)
        throw std::runtime_error("Failed to submit service IDs");

#pragma endregion

    CloseHandle(lOverlapped.hEvent);
}


ControlDevice::~ControlDevice()
{
    if (_deviceHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(_deviceHandle);
    }
}
