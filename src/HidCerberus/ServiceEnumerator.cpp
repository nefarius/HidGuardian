#include "ServiceEnumerator.h"


ServiceEnumerator::ServiceEnumerator()
{
}


ServiceEnumerator::~ServiceEnumerator()
{
}

DWORD ServiceEnumerator::processIdFromServiceName(std::string serviceName)
{
    const auto control = OpenSCManager(nullptr, nullptr, GENERIC_READ);

    if (control == nullptr)
        throw std::runtime_error("Service manager handle invalid");

    const auto service = OpenServiceA(control, serviceName.c_str(), GENERIC_READ);

    if (service == nullptr)
    {
        CloseServiceHandle(control);
        throw std::runtime_error("Service handle invalid");
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

    if (!ret)
        throw std::runtime_error("Failed to query service status");

    return svcProc.dwProcessId;
}
