#include "GuardedDevice.h"
#include <HidGuardian.h>
#include <devioctl.h>


GuardedDevice::GuardedDevice(const std::string & devicePath) : _devicePath(devicePath)
{
}

GuardedDevice::~GuardedDevice()
{
}

void GuardedDevice::open()
{
    DWORD bytesReturned = 0;
    OVERLAPPED lOverlapped = { 0 };
    
    _deviceHandle = CreateFileA(_devicePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED,
        nullptr);

    //
    // Log error state
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

    CloseHandle(lOverlapped.hEvent);
}
