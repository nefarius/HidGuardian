#include "PermissionRequestWorker.h"
#include "HidGuardian.h"

#include <locale>
#include <codecvt>
#include <sstream>

#include <Psapi.h>

#include <Poco/Logger.h>
#include <Poco/Data/Session.h>
#include <Poco/Buffer.h>
#include <Poco/String.h>

using Poco::Logger;
using Poco::Data::Statement;
using namespace Poco::Data::Keywords;
using Poco::Buffer;
using Poco::icompare;


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

        HANDLE hProcess = OpenProcess(
            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
            FALSE,
            pHgGet->ProcessId);

        std::string imagePath;
        std::string moduleName;

        if (NULL != hProcess)
        {
            HMODULE hMod;
            DWORD cbNeeded;
            Buffer<wchar_t> szProcessName(1024);
            Buffer<wchar_t> lpFilename(1024);

            GetModuleFileNameEx(hProcess, NULL, lpFilename.begin(), lpFilename.size());

            using convert_type = std::codecvt_utf8<wchar_t>;
            std::wstring_convert<convert_type, wchar_t> converter;

            imagePath = converter.to_bytes(lpFilename.begin());

            logger.information("Process path: %s", imagePath);

            if (EnumProcessModules(hProcess, &hMod, sizeof(hMod),
                &cbNeeded))
            {
                GetModuleBaseName(hProcess, hMod, szProcessName.begin(), szProcessName.size());
                
                moduleName = converter.to_bytes(szProcessName.begin());

                logger.information("Process name: %s", moduleName);
            }

            CloseHandle(hProcess);
        }

        Statement select(_session);
        
        select << "SELECT IsAllowed, IsPermanent FROM AccessRules WHERE HardwareId IN ("
            << hwIds.str().substr(0, hwIds.str().size() - 2) << ") AND (ModuleName=? OR ImagePath=?)",
            into(hgSet.IsAllowed),
            into(hgSet.IsSticky),
            use(moduleName),
            use(imagePath),
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
