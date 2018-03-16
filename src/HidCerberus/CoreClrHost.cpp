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

using Poco::Glob;
using Poco::Path;


std::wstring CoreClrHost::toWide(std::string source)
{
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;
    return converter.from_bytes(source);
}

long CoreClrHost::CreateSafeArrayFromBSTRArray(
    BSTR* pBSTRArray,
    ULONG ulArraySize,
    SAFEARRAY** ppSafeArrayReceiver
)
{
    HRESULT hrRetTemp = S_OK;
    SAFEARRAY* pSAFEARRAYRet = nullptr;
    SAFEARRAYBOUND rgsabound[1];
    ULONG ulIndex = 0;
    const long lRet = 0;

    // Initialise receiver.
    if (ppSafeArrayReceiver)
    {
        *ppSafeArrayReceiver = nullptr;
    }

    if (pBSTRArray)
    {
        rgsabound[0].lLbound = 0;
        rgsabound[0].cElements = ulArraySize;

        pSAFEARRAYRet = static_cast<SAFEARRAY*>(SafeArrayCreate
        (
            static_cast<VARTYPE>(VT_BSTR),
            static_cast<unsigned int>(1),
            static_cast<SAFEARRAYBOUND*>(rgsabound)
        ));
    }

    for (ulIndex = 0; ulIndex < ulArraySize; ulIndex++)
    {
        long lIndexVector[1];

        lIndexVector[0] = ulIndex;

        // Since pSAFEARRAYRet is created as a SafeArray of VT_BSTR,
        // SafeArrayPutElement() will create a copy of each BSTR
        // inserted into the SafeArray.
        SafeArrayPutElement
        (
            static_cast<SAFEARRAY*>(pSAFEARRAYRet),
            static_cast<long*>(lIndexVector),
            static_cast<void*>(pBSTRArray[ulIndex])
        );
    }

    if (pSAFEARRAYRet)
    {
        *ppSafeArrayReceiver = pSAFEARRAYRet;
    }

    return lRet;
}

void CoreClrHost::safeArrayFromBSTRVector(SAFEARRAY ** ppSafeArrayReceiver, const std::vector<BSTR>& pBSTRArray)
{
    SAFEARRAY* pSAFEARRAYRet = nullptr;
    SAFEARRAYBOUND rgsabound[1];
    ULONG ulIndex = 0;

    // Initialise receiver.
    if (ppSafeArrayReceiver)
    {
        *ppSafeArrayReceiver = nullptr;
    }

    if (pBSTRArray.empty())
        return;

    rgsabound[0].lLbound = 0;
    rgsabound[0].cElements = pBSTRArray.size();

    pSAFEARRAYRet = static_cast<SAFEARRAY*>(SafeArrayCreate
    (
        static_cast<VARTYPE>(VT_BSTR),
        static_cast<unsigned int>(1),
        static_cast<SAFEARRAYBOUND*>(rgsabound)
    ));

    for (ulIndex = 0; ulIndex < pBSTRArray.size(); ulIndex++)
    {
        long lIndexVector[1];

        lIndexVector[0] = ulIndex;

        // Since pSAFEARRAYRet is created as a SafeArray of VT_BSTR,
        // SafeArrayPutElement() will create a copy of each BSTR
        // inserted into the SafeArray.
        SafeArrayPutElement
        (
            static_cast<SAFEARRAY*>(pSAFEARRAYRet),
            static_cast<long*>(lIndexVector),
            static_cast<void*>(pBSTRArray[ulIndex])
        );
    }

    if (pSAFEARRAYRet)
    {
        *ppSafeArrayReceiver = pSAFEARRAYRet;
    }
}

CoreClrHost::CoreClrHost(const LayeredConfiguration& config) : _config(config), _domainId(0)
{
    const Path coreRoot(_config.getString("dotnet.CORE_ROOT", R"(C:\Program Files\dotnet\shared\Microsoft.NETCore.App\2.0.6)"));
    Path coreClrDll(coreRoot, "coreclr.dll");

    const Path coreAssemblies(coreRoot, "*.dll");
    std::set<std::string> tpaFiles;
    std::stringstream tpaStream;
    Glob::glob(coreAssemblies, tpaFiles);
    std::copy(tpaFiles.begin(), tpaFiles.end(), std::ostream_iterator<std::string>(tpaStream, ";"));

    std::wstring trustedPlatformAssemblies(toWide(_config.getString("dotnet.TRUSTED_PLATFORM_ASSEMBLIES", tpaStream.str())));
    std::wstring appPaths(toWide(_config.getString("dotnet.APP_PATHS", R"(D:\Development\C\HidGuardian\x64\Debug)")));
    std::wstring appNiPaths(toWide(_config.getString("dotnet.APP_NI_PATHS", "")));
    std::wstring nativeDllSearchDirectories(toWide(_config.getString("dotnet.NATIVE_DLL_SEARCH_DIRECTORIES", "")));
    std::wstring platformResourceRoots(toWide(_config.getString("dotnet.PLATFORM_RESOURCE_ROOTS", "")));
    std::wstring appDomainCompatSwitch(L"UseLatestBehaviorWhenTFMNotSpecified");

    _coreCLRModule = LoadLibraryExA(coreClrDll.toString().c_str(), nullptr, 0);

    if (!_coreCLRModule)
    {
        throw std::runtime_error("CoreCLR.dll could not be found");
    }

    const auto pfnGetCLRRuntimeHost = reinterpret_cast<FnGetCLRRuntimeHost>(GetProcAddress(
        _coreCLRModule, "GetCLRRuntimeHost"));

    if (!pfnGetCLRRuntimeHost)
    {
        throw std::runtime_error("GetCLRRuntimeHost not found");
    }

    auto hr = pfnGetCLRRuntimeHost(IID_ICLRRuntimeHost2, reinterpret_cast<IUnknown**>(&_runtimeHost));

    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to get ICLRRuntimeHost2 instance");
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

    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to set startup flags");
    }

    hr = _runtimeHost->Start();

    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to start the runtime");
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
        throw std::runtime_error("Failed to create AppDomain");
    }
}


CoreClrHost::~CoreClrHost()
{
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
        throw std::runtime_error("Delegate creation failed");
    }

    _accessRequestVigils.push_back(fpnVPAR);
}

bool CoreClrHost::processVigil(PCWSTR szHwIDs, ULONG processId)
{
    SAFEARRAY* pSafeArrayOfBSTR = nullptr;
    std::vector<BSTR> hardwareIds;
    BOOL isAllowed = FALSE;
    BOOL isPermanent = FALSE;

    //
    // Split up multi-value string and convert to BSTR
    // 
    for (PCWSTR szIter = szHwIDs; *szIter; szIter += wcslen(szIter) + 1)
    {
        hardwareIds.push_back(SysAllocString(szIter));
    }

    safeArrayFromBSTRVector(&pSafeArrayOfBSTR, hardwareIds);

    for (auto& vigil : _accessRequestVigils)
    {
        auto ret = vigil(&pSafeArrayOfBSTR, &isAllowed, &isPermanent);
    }

    for (auto& id : hardwareIds)
    {
        SysFreeString(id);
    }

    return false;
}
