#include "ControlDevice.h"
#include <utility>

#include <Poco/Logger.h>
#include <HidGuardian.h>


ControlDevice::ControlDevice(std::string devicePath) : _devicePath(std::move(devicePath))
{
    auto& logger = Poco::Logger::get(std::string(typeid(this).name()) + std::string("::") + std::string(__func__));

    logger.debug("Trying to open control device %s", devicePath);

    _deviceHandle = CreateFileA(_devicePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED,
        nullptr);

    if (_deviceHandle == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            throw std::runtime_error("Couldn't open the desired device, make sure the provided path is correct.");
        }

        if (GetLastError() == ERROR_ACCESS_DENIED) {
            throw std::runtime_error("Couldn't access device, please make sure the device isn't already guarded.");
        }

        throw std::runtime_error("Couldn't access device, unknown error.");
    }

    logger.debug("Device opened");

    DWORD bytesReturned = 0;
    OVERLAPPED lOverlapped = { 0 };
    lOverlapped.hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    
    

    CloseHandle(lOverlapped.hEvent);
}


ControlDevice::~ControlDevice()
{
    if (_deviceHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(_deviceHandle);
    }
}
