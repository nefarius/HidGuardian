#include "ServiceEnumerator.h"
#include <tlhelp32.h>
#include <locale>
#include <codecvt>

#include <Poco/String.h>

using Poco::icompare;


ServiceEnumerator::ServiceEnumerator()
{
}


ServiceEnumerator::~ServiceEnumerator()
{
}

DWORD ServiceEnumerator::processIdFromServiceName(std::string serviceName)
{
    const auto control = OpenSCManagerA(nullptr, nullptr, GENERIC_READ);

    if (control == nullptr)
        throw std::runtime_error("Service manager handle invalid");

    const auto service = OpenServiceA(control, serviceName.c_str(), SERVICE_QUERY_STATUS);

    if (service == nullptr)
    {
        CloseServiceHandle(control);
        throw std::runtime_error(std::string("Service handle invalid: ") + serviceName);
    }

    SERVICE_STATUS_PROCESS svcProc;
    RtlZeroMemory(&svcProc, sizeof(SERVICE_STATUS_PROCESS));

    DWORD bytesRequired;

    const auto ret = QueryServiceStatusEx(
        service,
        SC_STATUS_PROCESS_INFO,
        reinterpret_cast<LPBYTE>(&svcProc),
        sizeof(SERVICE_STATUS_PROCESS),
        &bytesRequired);

    if (!ret) {
        CloseServiceHandle(control);
        CloseServiceHandle(service);
        throw std::runtime_error("Failed to query service status");
    }

    CloseServiceHandle(control);
    CloseServiceHandle(service);

    return svcProc.dwProcessId;
}

DWORD ServiceEnumerator::processIdFromProcessName(std::string processName)
{
    DWORD pid = 0;
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

    if (Process32First(snapshot, &entry) == TRUE)
    {
        while (Process32Next(snapshot, &entry) == TRUE)
        {
            using convert_type = std::codecvt_utf8<wchar_t>;
            std::wstring_convert<convert_type, wchar_t> converter;
            const std::string exeFile(converter.to_bytes(entry.szExeFile));

            if (icompare(exeFile, processName) == 0)
            {
                pid = entry.th32ProcessID;
                break;
            }
        }
    }

    CloseHandle(snapshot);

    return pid;
}
