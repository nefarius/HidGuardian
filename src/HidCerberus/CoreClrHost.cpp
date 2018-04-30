#include "CoreClrHost.h"

//
// STL
// 
#include <string>
#include <sstream>
#include <iterator>
#include <locale>
#include <codecvt>

//
// POCO
// 
#include <Poco/Glob.h>
#include <Poco/Path.h>
#include <Poco/Logger.h>
#include <Poco/Util/WinRegistryKey.h>
#include <Poco/Environment.h>
#include <Poco/Exception.h>

using Poco::Glob;
using Poco::Path;
using Poco::Util::WinRegistryKey;
using Poco::Environment;


std::wstring CoreClrHost::toWide(std::string source)
{
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;
    return converter.from_bytes(source);
}

CoreClrHost::CoreClrHost(const LayeredConfiguration& config) : _runtimeHost(nullptr), _config(config), _domainId(0)
{
    //
    // Build registry key to detect installed host version
    // 
    std::ostringstream dotnetCoreVersionKey;
    dotnetCoreVersionKey << R"(HKEY_LOCAL_MACHINE\SOFTWARE\dotnet\Setup\InstalledVersions\)";
    if (Environment::arch() == POCO_ARCH_AMD64)
        dotnetCoreVersionKey << R"(x64\)";
    //  TODO: does this work in 32-Bit?
    if (Environment::arch() == POCO_ARCH_IA32)
        dotnetCoreVersionKey << R"(x86\)";
    dotnetCoreVersionKey << "sharedhost";

    WinRegistryKey dotnetCoreVersion(dotnetCoreVersionKey.str(), true);
    const auto hostVersion = dotnetCoreVersion.getString("Version");

    //
    // Build path to CoreCLR host path
    // 
    Path coreRootDetected(Path::expand(R"(%programfiles%\dotnet\shared\Microsoft.NETCore.App)"), hostVersion);
    const Path coreRoot(_config.getString("dotnet.CORE_ROOT", coreRootDetected.toString()));

    //
    // CoreCLR bootstrapper library
    // 
    Path coreClrDll(coreRoot, "coreclr.dll");

    //
    // Add all DLLs from the host directory to TPA path if not overridden by config
    // 
    const Path coreAssemblies(coreRoot, "*.dll");
    std::set<std::string> tpaFiles;
    std::stringstream tpaStream;
    Glob::glob(coreAssemblies, tpaFiles);
    std::copy(tpaFiles.begin(), tpaFiles.end(), std::ostream_iterator<std::string>(tpaStream, ";"));

    //
    // Grab CoreCLR flags from configuration or use default/auto values
    // 
    std::wstring trustedPlatformAssemblies(toWide(_config.getString("dotnet.TRUSTED_PLATFORM_ASSEMBLIES", tpaStream.str())));
    std::wstring appPaths(toWide(_config.getString("dotnet.APP_PATHS", "")));
    std::wstring appNiPaths(toWide(_config.getString("dotnet.APP_NI_PATHS", "")));
    std::wstring nativeDllSearchDirectories(toWide(_config.getString("dotnet.NATIVE_DLL_SEARCH_DIRECTORIES", "")));
    std::wstring platformResourceRoots(toWide(_config.getString("dotnet.PLATFORM_RESOURCE_ROOTS", "")));
    std::wstring appDomainCompatSwitch(L"UseLatestBehaviorWhenTFMNotSpecified");

    _coreCLRModule = LoadLibraryExA(coreClrDll.toString().c_str(), nullptr, 0);
    if (!_coreCLRModule) {
        throw Poco::RuntimeException("CoreCLR.dll could not be found");
    }

    const auto pfnGetCLRRuntimeHost = reinterpret_cast<FnGetCLRRuntimeHost>(GetProcAddress(
        _coreCLRModule, "GetCLRRuntimeHost"));
    if (!pfnGetCLRRuntimeHost) {
        throw Poco::RuntimeException("GetCLRRuntimeHost not found");
    }

    auto hr = pfnGetCLRRuntimeHost(IID_ICLRRuntimeHost2, reinterpret_cast<IUnknown**>(&_runtimeHost));
    if (FAILED(hr)) {
        throw Poco::RuntimeException("Failed to get ICLRRuntimeHost2 instance");
    }

    hr = _runtimeHost->SetStartupFlags(
        static_cast<STARTUP_FLAGS>(
            // STARTUP_FLAGS::STARTUP_SERVER_GC |								// Use server GC
            // STARTUP_FLAGS::STARTUP_LOADER_OPTIMIZATION_MULTI_DOMAIN |		// Maximize domain-neutral loading
            // STARTUP_FLAGS::STARTUP_LOADER_OPTIMIZATION_MULTI_DOMAIN_HOST |	// Domain-neutral loading for strongly-named assemblies
            STARTUP_FLAGS::STARTUP_CONCURRENT_GC |						// Use concurrent GC
            STARTUP_FLAGS::STARTUP_SINGLE_APPDOMAIN |					// All code executes in the default AppDomain 
                                                                        // (required to use the runtimeHost->ExecuteAssembly helper function)
            STARTUP_FLAGS::STARTUP_LOADER_OPTIMIZATION_SINGLE_DOMAIN	// Prevents domain-neutral loading
            )
    );
    if (FAILED(hr)) {
        throw Poco::RuntimeException("Failed to set startup flags");
    }

    hr = _runtimeHost->Start();
    if (FAILED(hr)) {
        throw Poco::RuntimeException("Failed to start the runtime");
    }

    const auto appDomainFlags =
        // APPDOMAIN_FORCE_TRIVIAL_WAIT_OPERATIONS |		// Do not pump messages during wait
        // APPDOMAIN_SECURITY_SANDBOXED |					// Causes assemblies not from the TPA list to be loaded as partially trusted
        APPDOMAIN_ENABLE_PLATFORM_SPECIFIC_APPS |			// Enable platform-specific assemblies to run
        APPDOMAIN_ENABLE_PINVOKE_AND_CLASSIC_COMINTEROP |	// Allow PInvoking from non-TPA assemblies
        APPDOMAIN_DISABLE_TRANSPARENCY_ENFORCEMENT;			// Entirely disables transparency checks 

    // Setup key/value pairs for AppDomain  properties
    const wchar_t* propertyKeys[] = {
        L"TRUSTED_PLATFORM_ASSEMBLIES",
        L"APP_PATHS",
        L"APP_NI_PATHS",
        L"NATIVE_DLL_SEARCH_DIRECTORIES",
        L"PLATFORM_RESOURCE_ROOTS",
        L"AppDomainCompatSwitch"
    };

    // Property values which were constructed in step 5
    const wchar_t* propertyValues[] = {
        trustedPlatformAssemblies.c_str(),
        appPaths.c_str(),
        appNiPaths.c_str(),
        nativeDllSearchDirectories.c_str(),
        platformResourceRoots.c_str(),
        appDomainCompatSwitch.c_str()
    };

    hr = _runtimeHost->CreateAppDomainWithManager(
        L"Cerberus CLR Host Domain",    // Friendly AD name
        appDomainFlags,
        nullptr,						// Optional AppDomain manager assembly name
        nullptr,						// Optional AppDomain manager type (including namespace)
        sizeof(propertyKeys) / sizeof(wchar_t*),
        propertyKeys,
        propertyValues,
        &_domainId);

    if (FAILED(hr))
    {
        throw Poco::RuntimeException("Failed to create AppDomain");
    }
}

CoreClrHost::~CoreClrHost()
{
    auto& logger = Poco::Logger::get(std::string(typeid(this).name()) + std::string("::") + std::string(__func__));

    logger.debug("Shutting down CoreCLR host");

    _accessRequestVigils.clear();

    if (_runtimeHost)
    {
        _runtimeHost->UnloadAppDomain(_domainId, true);
        _runtimeHost->Stop();
        _runtimeHost->Release();
    }

    if (_coreCLRModule)
    {
        FreeLibrary(_coreCLRModule);
    }
}

void CoreClrHost::loadVigil(std::string assemblyName, std::string className, std::string methodName)
{
    auto assembly = toWide(assemblyName);
    auto classnam = toWide(className);
    auto methodnm = toWide(methodName);

    fpnClrVigilProcessAccessRequest fpnVPAR;

    const auto hr = _runtimeHost->CreateDelegate(
        _domainId,
        assembly.c_str(),
        classnam.c_str(),
        methodnm.c_str(),
        reinterpret_cast<INT_PTR*>(&fpnVPAR));

    if (FAILED(hr))
    {
        throw Poco::RuntimeException("Delegate creation failed");
    }

    _accessRequestVigils.push_back(fpnVPAR);
}

void CoreClrHost::processVigil(
    PCWSTR szHwIDs, 
    PCSTR deviceId,
    PCSTR instanceId,
    ULONG processId, 
    PBOOL pIsAllowed, 
    PBOOL pIsPermanent
)
{
    BOOL isAllowed = FALSE;
    BOOL isPermanent = FALSE;

    //
    // Split up multi-value string and call processing method
    // 
    for (PCWSTR szIter = szHwIDs; *szIter; szIter += wcslen(szIter) + 1)
    {
        for (auto& vigil : _accessRequestVigils)
        {
            using convert_type = std::codecvt_utf8<wchar_t>;
            std::wstring_convert<convert_type, wchar_t> converter;
            const std::string id(converter.to_bytes(szIter));

            const auto ret = vigil(
                id.c_str(), 
                deviceId, 
                instanceId,
                processId, 
                &isAllowed, 
                &isPermanent
            );

            if (ret)
            {
                *pIsAllowed = isAllowed;
                *pIsPermanent = isPermanent;
            }
        }
    }
}
