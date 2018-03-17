#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "mscoree.h"
#include "HidCerberus.h"

#include <string>
#include <vector>

#include <Poco/Util/LayeredConfiguration.h>


using Poco::Util::LayeredConfiguration;

class CoreClrHost
{
    HMODULE _coreCLRModule;
    ICLRRuntimeHost2* _runtimeHost{};
    const LayeredConfiguration& _config;
    DWORD _domainId;
    std::vector<fpnClrVigilProcessAccessRequest> _accessRequestVigils;

    static std::wstring toWide(std::string source);
public:
    CoreClrHost(const LayeredConfiguration& config);
    ~CoreClrHost();

    void loadVigil(std::string assemblyName, std::string className, std::string methodName);
    bool processVigil(PCWSTR szHwIDs, ULONG processId);
};

