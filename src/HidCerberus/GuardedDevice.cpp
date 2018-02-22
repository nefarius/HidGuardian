#include "GuardedDevice.h"
#include <HidGuardian.h>
#include <devioctl.h>

#include <Poco/Logger.h>

using Poco::Logger;


GuardedDevice::GuardedDevice(const std::string & devicePath, const LayeredConfiguration& config, const Session& session)
    : _devicePath(devicePath), _config(config), _session(session)
{    
    DWORD bytesReturned = 0;
    OVERLAPPED lOverlapped = { 0 };

    //
    // Open device
    // 
    _deviceHandle = CreateFileA(_devicePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED,
        nullptr);

    //
    // Check for errors
    // 
    if (_deviceHandle == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            throw std::runtime_error("Couldn't open the desired device, make sure the provided path is correct.");
        }

        if (GetLastError() == ERROR_ACCESS_DENIED) {
            throw std::runtime_error("Couldn't access control device, please make sure the device isn't already guarded.");
        }
    }

    lOverlapped.hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    //
    // Check if the device responds to a HidGuardian-provided IOCTL
    // 
    DeviceIoControl(
        _deviceHandle,
        IOCTL_HIDGUARDIAN_IS_ACTIVE,
        nullptr,
        0,
        nullptr,
        0,
        &bytesReturned,
        &lOverlapped
    );

    if (GetOverlappedResult(_deviceHandle, &lOverlapped, &bytesReturned, TRUE) == 0)
    {
        CloseHandle(lOverlapped.hEvent);
        throw std::runtime_error("The device doesn't have HidGuardian attached.");
    }

    //
    // Announce your process as the Cerberus
    // 
    DeviceIoControl(
        _deviceHandle,
        IOCTL_HIDGUARDIAN_REGISTER_CERBERUS,
        nullptr,
        0,
        nullptr,
        0,
        &bytesReturned,
        &lOverlapped
    );

    if (GetOverlappedResult(_deviceHandle, &lOverlapped, &bytesReturned, TRUE) == 0)
    {
        CloseHandle(lOverlapped.hEvent);
        throw std::runtime_error("Couldn't register Cerberus to driver.");
    }

    CloseHandle(lOverlapped.hEvent);

    _taskManager.start(new PermissionRequestWorker(_deviceHandle, _session));
}

GuardedDevice::~GuardedDevice()
{
    _taskManager.cancelAll();
    _taskManager.joinAll();

    if (_deviceHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(_deviceHandle);
    }
}

