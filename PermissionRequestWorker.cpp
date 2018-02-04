#include "PermissionRequestWorker.h"
#include "HidGuardian.h"

#include <Poco/Logger.h>

using Poco::Logger;


PermissionRequestWorker::PermissionRequestWorker(HANDLE controlDevice) : _controlDevice(controlDevice)
{
}


PermissionRequestWorker::~PermissionRequestWorker()
{
}

void PermissionRequestWorker::run()
{
    auto& logger = Logger::get(std::string(typeid(this).name()) + std::string("::") + std::string(__func__));

    //
    // DeviceIoControl stuff
    // 
    DWORD bytesReturned = 0;
    OVERLAPPED lOverlapped = { 0 };
    lOverlapped.hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    ULONG pHgGetSize = sizeof(HIDGUARDIAN_GET_CREATE_REQUEST) + 1024;
    PHIDGUARDIAN_GET_CREATE_REQUEST pHgGet;
    HIDGUARDIAN_SET_CREATE_REQUEST hgSet;

    pHgGet = (PHIDGUARDIAN_GET_CREATE_REQUEST)malloc(pHgGetSize);

    while (true)
    {
        ZeroMemory(&hgSet, sizeof(HIDGUARDIAN_SET_CREATE_REQUEST));
        ZeroMemory(pHgGet, pHgGetSize);
        pHgGet->Size = pHgGetSize;

        auto reqId = _rnd.next();

        pHgGet->RequestId = reqId;

        DeviceIoControl(
            _controlDevice,
            IOCTL_HIDGUARDIAN_GET_CREATE_REQUEST,
            pHgGet,
            pHgGetSize,
            pHgGet,
            pHgGetSize,
            &bytesReturned,
            &lOverlapped
        );

        if (GetOverlappedResult(_controlDevice, &lOverlapped, &bytesReturned, TRUE) != 0) 
        {
            logger.information("GET request answered");
        }
        else 
        {
            logger.error("GET request failed");
            break;
        }
    }

    //DeviceIoControl(_controlDevice, IOCTL_HIDGUARDIAN_GET_CREATE_REQUEST, nullptr, 0, nullptr, 0, &bytesReturned, nullptr);

    free(pHgGet);
    CloseHandle(lOverlapped.hEvent);
}
