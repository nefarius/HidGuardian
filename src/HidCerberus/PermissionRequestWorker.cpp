#include "PermissionRequestWorker.h"

#include <devioctl.h>
#include "HidGuardian.h"

//
// STL
// 
#include <locale>
#include <codecvt>
#include <sstream>

//
// Windows
// 
#include <Psapi.h>

//
// POCO
// 
#include <Poco/Logger.h>
#include <Poco/Data/Session.h>
#include <Poco/Buffer.h>
#include <Poco/String.h>

using Poco::Logger;
using Poco::Data::Statement;
using namespace Poco::Data::Keywords;
using Poco::Buffer;
using Poco::icompare;


void PermissionRequestWorker::runTask()
{
    auto& logger = Logger::get(std::string(typeid(this).name()) + std::string("::") + std::string(__func__));

    logger.debug("Worker running");
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
            //
            // This condition is met if the drivers queue is empty
            // 
            if (GetLastError() == ERROR_NO_MORE_ITEMS) {
                Sleep(1000);
                continue;
            }

            logger.error("Request (ID: %lu) failed: %lu", pHgGet->RequestId, (ULONG)GetLastError());
            break;
        }

        if (logger.is(Poco::Message::PRIO_DEBUG)) {
            logger.debug("Request (ID: %lu) completed", pHgGet->RequestId);
            logger.debug("PID: %lu", pHgGet->ProcessId);
        }

        hgSet.RequestId = pHgGet->RequestId;

#pragma region Extract Hardware IDs

        std::vector<std::string> hardwareIds;

        //
        // Split up multi-value string to individual objects
        // 
        for (PCWSTR szIter = pHgGet->HardwareIds; *szIter; szIter += wcslen(szIter) + 1)
        {
            using convert_type = std::codecvt_utf8<wchar_t>;
            std::wstring_convert<convert_type, wchar_t> converter;
            const std::string id(converter.to_bytes(szIter));

            if (logger.is(Poco::Message::PRIO_DEBUG)) {
                logger.debug("Hardware ID: %s", id);
            }

            hardwareIds.push_back(id);
        }

#pragma endregion

#pragma region Get process details

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
            Buffer<wchar_t> szProcessName(_bufferSize);
            Buffer<wchar_t> lpFilename(_bufferSize);

            GetModuleFileNameEx(hProcess, NULL, lpFilename.begin(), (DWORD)lpFilename.size());

            using convert_type = std::codecvt_utf8<wchar_t>;
            std::wstring_convert<convert_type, wchar_t> converter;

            imagePath = converter.to_bytes(lpFilename.begin());

            if (EnumProcessModules(hProcess, &hMod, sizeof(hMod),
                &cbNeeded))
            {
                GetModuleBaseName(hProcess, hMod, szProcessName.begin(), (DWORD)szProcessName.size());

                moduleName = converter.to_bytes(szProcessName.begin());
            }

            CloseHandle(hProcess);
        }

#pragma endregion

#pragma region Database query

        Statement select(_session);

        select << "SELECT IsAllowed, IsPermanent FROM AccessRules WHERE HardwareId IN (";

        //
        // Convert list of Hardware IDs to statement
        // 
        const auto separator = ", ";
        const auto* sep = "";
        for (const auto& item : hardwareIds) {
            select << sep << '"' << item << '"';
            sep = separator;
        }

        select << ") AND (ModuleName=? OR ImagePath=?)",
            into(hgSet.IsAllowed),
            into(hgSet.IsSticky),
            use(moduleName),
            use(imagePath),
            now;

#pragma endregion

        if (logger.is(Poco::Message::PRIO_DEBUG)) {
            logger.debug("Process path: %s", imagePath);
            logger.debug("Process name: %s", moduleName);
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
            logger.error("Permission request %lu failed", pHgGet->RequestId);
            break;
        }

        if (logger.is(Poco::Message::PRIO_DEBUG))
            logger.debug("Permission request %lu finished successfully", pHgGet->RequestId);
    }

    free(pHgGet);
    CloseHandle(lOverlapped.hEvent);
}
