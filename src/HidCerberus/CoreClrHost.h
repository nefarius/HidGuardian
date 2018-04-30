#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "mscoree.h"
#include "HidCerberus.h"

#include <string>
#include <vector>

#define POCO_NO_UNWINDOWS
#include <Poco/Util/LayeredConfiguration.h>
#include <Poco/RefCountedObject.h>


using Poco::Util::LayeredConfiguration;

class CoreClrHost : public Poco::RefCountedObject
{
    HMODULE _coreCLRModule;
    ICLRRuntimeHost2* _runtimeHost;
    const LayeredConfiguration& _config;
    DWORD _domainId;
    std::vector<fpnClrVigilProcessAccessRequest> _accessRequestVigils;

    static std::wstring toWide(std::string source);
protected:
    ~CoreClrHost();

public:
    CoreClrHost(const LayeredConfiguration& config);

    void loadVigil(std::string assemblyName, std::string className, std::string methodName);
    void processVigil(
        PCWSTR szHwIDs,
        PCSTR deviceId,
        PCSTR instanceId,
        ULONG processId,
        PBOOL pIsAllowed,
        PBOOL pIsPermanent
    );
};

struct CoreClrVigil
{
    std::string name;
    std::string description;
    std::string assemblyPath;
    std::string assemblyName;
    std::string className;
    std::string methodName;
};
