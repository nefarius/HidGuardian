#include "GuardedDevice.h"
#include <HidGuardian.h>
#include <devioctl.h>
#include "HidCerberusInternal.h"

//
// STL
// 
#include <locale>
#include <codecvt>
#include <utility>

//
// Windows
// 
#include <Psapi.h>

//
// POCO
// 
#include <Poco/Logger.h>
#include <Poco/Buffer.h>
#include <Poco/String.h>
#include <Poco/UnicodeConverter.h>

using Poco::Logger;
using Poco::Buffer;
using Poco::icompare;


GuardedDevice::GuardedDevice(std::string devicePath, PHC_HANDLE handle)
    : Task(devicePath), _devicePath(std::move(devicePath)), _hcHandle(handle)
{
    auto& logger = Logger::get(std::string(typeid(this).name()) + std::string("::") + std::string(__func__));

    logger.debug("Trying to open device %s", _devicePath);

    //
    // Open device
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
    // Check for errors
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
}

void GuardedDevice::runTask()
{
    auto& logger = Logger::get(std::string(typeid(this).name()) + std::string("::") + std::string(__func__));

    logger.debug("Worker running (%s)", _devicePath);
    _rnd.seed();

    //
    // DeviceIoControl stuff
    // 
    DWORD bytesReturned = 0;
    OVERLAPPED lOverlapped = { 0 };
    lOverlapped.hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    const ULONG pHgGetSize = sizeof(HIDGUARDIAN_GET_CREATE_REQUEST) + _bufferSize;
    PHIDGUARDIAN_GET_CREATE_REQUEST pHgGet;
    HIDGUARDIAN_SET_CREATE_REQUEST hgSet;

    pHgGet = static_cast<PHIDGUARDIAN_GET_CREATE_REQUEST>(malloc(pHgGetSize));

    while (!isCancelled())
    {
        ZeroMemory(&hgSet, sizeof(HIDGUARDIAN_SET_CREATE_REQUEST));
        ZeroMemory(pHgGet, pHgGetSize);
        pHgGet->Size = pHgGetSize;

        const auto reqId = _rnd.next();

        pHgGet->RequestId = reqId;

        if (logger.is(Poco::Message::PRIO_DEBUG))
            logger.debug("Looking for quests (ID: %lu)", pHgGet->RequestId);

        //
        // Query for pending create (open) requests
        // 
        DeviceIoControl(
            _deviceHandle,
            IOCTL_HIDGUARDIAN_GET_CREATE_REQUEST,
            pHgGet,
            pHgGetSize,
            pHgGet,
            pHgGetSize,
            &bytesReturned,
            &lOverlapped
        );

        if (GetOverlappedResult(_deviceHandle, &lOverlapped, &bytesReturned, TRUE) == 0)
        {
            const auto error = GetLastError();

            //
            // This condition is met if the drivers queue is empty
            // 
            if (error == ERROR_NO_MORE_ITEMS)
            {
                sleep(200);
                continue;
            }

            if (error == ERROR_DEV_NOT_EXIST)
            {
                logger.debug("Device got removed/powered down, terminating thread");
                break;
            }

            logger.error("Request (ID: %lu) failed: %lu", pHgGet->RequestId, (ULONG)error);
            break;
        }

        std::string deviceId;
        Poco::UnicodeConverter::convert(pHgGet->DeviceId, deviceId);

        std::string instanceId;
        Poco::UnicodeConverter::convert(pHgGet->InstanceId, instanceId);

        if (logger.is(Poco::Message::PRIO_DEBUG))
        {
            logger.debug("DeviceId = %s", deviceId);
            logger.debug("InstanceId = %s", instanceId);
            logger.debug("Request (ID: %lu) completed", pHgGet->RequestId);
            logger.debug("PID: %lu", pHgGet->ProcessId);
        }

        hgSet.RequestId = pHgGet->RequestId;
        hgSet.IsAllowed = TRUE; // important; use as a default

        if (logger.is(Poco::Message::PRIO_DEBUG)) {
            logger.debug("Start processing Vigil (ID: %lu)", pHgGet->RequestId);
        }

        BOOL isAllowed = FALSE;
        BOOL isPermanent = FALSE;

        //
        // Split up multi-value string and call processing method
        // 
        for (PCWSTR szIter = pHgGet->HardwareIds; *szIter; szIter += wcslen(szIter) + 1)
        {
            using convert_type = std::codecvt_utf8<wchar_t>;
            std::wstring_convert<convert_type, wchar_t> converter;
            const std::string id(converter.to_bytes(szIter));

            //
            // Invoke callback asking host for a decision
            // 
            const auto ret = _hcHandle->EvtProcessAccessRequest(
                id.c_str(),
                deviceId.c_str(),
                instanceId.c_str(),
                pHgGet->ProcessId,
                &isAllowed,
                &isPermanent
            );

            //
            // Only overwrite properties if a rule applies
            // 
            if (ret)
            {
                hgSet.IsAllowed = isAllowed;
                hgSet.IsSticky = isPermanent;
                break;
            }
        }

        if (logger.is(Poco::Message::PRIO_DEBUG)) {
            logger.debug("End processing Vigil (ID: %lu)", pHgGet->RequestId);
        }

        if (logger.is(Poco::Message::PRIO_DEBUG))
        {
            logger.debug("IsAllowed: %b", (bool)hgSet.IsAllowed);
            logger.debug("IsSticky: %b", (bool)hgSet.IsSticky);
            logger.debug("Sending permission request %lu", pHgGet->RequestId);
        }

        //
        // Submit result to driver
        // 
        DeviceIoControl(
            _deviceHandle,
            IOCTL_HIDGUARDIAN_SET_CREATE_REQUEST,
            &hgSet,
            sizeof(HIDGUARDIAN_SET_CREATE_REQUEST),
            nullptr,
            0,
            &bytesReturned,
            &lOverlapped
        );

        if (GetOverlappedResult(_deviceHandle, &lOverlapped, &bytesReturned, TRUE) == 0)
        {
            const auto error = GetLastError();

            if (error == ERROR_DEV_NOT_EXIST)
            {
                logger.debug("Device got removed/powered down, terminating thread");
                break;
            }

            logger.error("Permission request %lu failed", pHgGet->RequestId);
            break;
        }

        if (logger.is(Poco::Message::PRIO_DEBUG))
            logger.debug("Permission request %lu finished successfully", pHgGet->RequestId);
    }

    free(pHgGet);
    CloseHandle(lOverlapped.hEvent);

    logger.information("No more guarding");
}

GuardedDevice::~GuardedDevice()
{
    if (_deviceHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(_deviceHandle);
    }
}
