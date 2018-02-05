#include "PermissionRequestWorker.h"
#include "HidGuardian.h"

#include <Poco/Logger.h>
#include <Poco/Data/Session.h>

using Poco::Logger;
using Poco::Data::Statement;
using namespace Poco::Data::Keywords;


PermissionRequestWorker::PermissionRequestWorker(HANDLE controlDevice, const Session& dbSession) : _controlDevice(controlDevice), _session(dbSession)
{
}


PermissionRequestWorker::~PermissionRequestWorker()
{
}

void PermissionRequestWorker::run()
{
    auto& logger = Logger::get(std::string(typeid(this).name()) + std::string("::") + std::string(__func__));

    logger.information("Worker running");

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

        if (GetOverlappedResult(_controlDevice, &lOverlapped, &bytesReturned, TRUE) == 0)
        {
            logger.error("GET request failed");
            break;
        }

        logger.information("GET request answered");

        logger.information("RequestId: %lu", pHgGet->RequestId);
        logger.information("DeviceIndex: %lu", pHgGet->DeviceIndex);

        hgSet.RequestId = pHgGet->RequestId;
        hgSet.DeviceIndex = pHgGet->DeviceIndex;

        Statement select(_session);
        
        std::string demoHwId("HID_DEVICE");

        select << "SELECT IsAllowed, IsPermanent FROM AccessRules WHERE HardwareId=?",
            into(hgSet.IsAllowed),
            into(hgSet.IsSticky),
            use(demoHwId),
            now;

        DeviceIoControl(
            _controlDevice,
            IOCTL_HIDGUARDIAN_SET_CREATE_REQUEST,
            &hgSet,
            sizeof(HIDGUARDIAN_SET_CREATE_REQUEST),
            nullptr,
            0,
            &bytesReturned,
            &lOverlapped
        );

        if (GetOverlappedResult(_controlDevice, &lOverlapped, &bytesReturned, TRUE) == 0)
        {
            logger.error("SET request failed");
            break;
        }
    }

    //DeviceIoControl(_controlDevice, IOCTL_HIDGUARDIAN_GET_CREATE_REQUEST, nullptr, 0, nullptr, 0, &bytesReturned, nullptr);

    free(pHgGet);
    CloseHandle(lOverlapped.hEvent);
}
