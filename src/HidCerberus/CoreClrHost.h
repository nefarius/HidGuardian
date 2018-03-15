#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "mscoree.h"

#include <Poco/Util/LayeredConfiguration.h>

using Poco::Util::LayeredConfiguration;

class CoreClrHost
{
    HMODULE _coreCLRModule;
    ICLRRuntimeHost2* _runtimeHost{};
    const LayeredConfiguration& _config;
    DWORD _domainId;

    static std::wstring toWide(std::string source);
public:
    CoreClrHost(const LayeredConfiguration& config);
    ~CoreClrHost();
};

