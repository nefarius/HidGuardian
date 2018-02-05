#include "PermissionRequestWorker.h"
#include "HidGuardian.h"

#include <Poco/Logger.h>
#include <Poco/Data/Session.h>

#include <locale>
#include <codecvt>
#include <sstream>

using Poco::Logger;
using Poco::Data::Statement;
using namespace Poco::Data::Keywords;


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

        std::stringstream hwIds;

        for (PCWSTR szIter = pHgGet->HardwareIds; *szIter; szIter += wcslen(szIter) + 1)
        {
            using convert_type = std::codecvt_utf8<wchar_t>;
            std::wstring_convert<convert_type, wchar_t> converter;

            hwIds << "\"" << converter.to_bytes(szIter) << "\", ";
        }

        Statement select(_session);

        select << "SELECT IsAllowed, IsPermanent FROM AccessRules WHERE HardwareId IN (" 
            << hwIds.str().substr(0, hwIds.str().size() - 2) << ")",
            into(hgSet.IsAllowed), 
            into(hgSet.IsSticky), 
            now;

        logger.information("IsAllowed: %b", (bool)hgSet.IsAllowed);
        logger.information("IsSticky: %b", (bool)hgSet.IsSticky);

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

    free(pHgGet);
    CloseHandle(lOverlapped.hEvent);
}
