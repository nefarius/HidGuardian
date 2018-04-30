#include "ZerberusService.h"
#include "DeviceEnumerator.h"
#include "GuardedDevice.h"
#include "DeviceListener.h"
#include "CoreClrHost.h"

#include <fstream>

#include <initguid.h>
#include "HidGuardian.h"

#define POCO_NO_UNWINDOWS
#include <Poco/Util/Option.h>
#include <Poco/Util/OptionSet.h>
#include <Poco/AutoPtr.h>
#include <Poco/Logger.h>
#include <Poco/Message.h>
#include <Poco/FileChannel.h>
#include <Poco/PatternFormatter.h>
#include <Poco/FormattingChannel.h>
#include <Poco/AsyncChannel.h>
#include <Poco/Path.h>
#include <Poco/WindowsConsoleChannel.h>
#include <Poco/SplitterChannel.h>
#include <Poco/SharedPtr.h>
#include <Poco/Data/SQLite/Utility.h>
#include <Poco/Buffer.h>
#include <Poco/Util/RegExpValidator.h>
#include <Poco/Util/HelpFormatter.h>
#include <Poco/ThreadPool.h>
#include <Poco/BasicEvent.h>
#include <Poco/Delegate.h>
#include <Poco/DOM/DOMParser.h>
#include <Poco/DOM/Document.h>
#include <Poco/DOM/NodeIterator.h>
#include <Poco/DOM/NodeFilter.h>
#include <Poco/SAX/InputSource.h>
#include <Poco/DOM/NodeList.h>
#include <Poco/DOM/NamedNodeMap.h>
#include <Poco/Exception.h>

using Poco::AutoPtr;
using Poco::Logger;
using Poco::Message;
using Poco::FileChannel;
using Poco::PatternFormatter;
using Poco::FormattingChannel;
using Poco::AsyncChannel;
using Poco::Path;
using Poco::WindowsConsoleChannel;
using Poco::SplitterChannel;
using Poco::SharedPtr;
using Poco::Buffer;
using Poco::Util::RegExpValidator;
using Poco::Util::HelpFormatter;
using Poco::ThreadPool;
using Poco::BasicEvent;
using Poco::Delegate;
using Poco::AutoPtr;



void ZerberusService::enumerateDeviceInterface(const std::string& name, const std::string& value)
{
    auto interfaceGuid = DeviceEnumerator::stringToGuid(value);

    auto devices = DeviceEnumerator::enumerateDeviceInterface(&interfaceGuid);

    for (auto& device : devices)
    {
        std::cout << "Device path: " << device << "\n";
    }

    stopOptionsProcessing();
}

void ZerberusService::help(const std::string & name, const std::string & value)
{
    displayHelp();
    stopOptionsProcessing();
}

void ZerberusService::displayHelp() const
{
    HelpFormatter helpFormatter(options());
    helpFormatter.setCommand(commandName());
    helpFormatter.setUsage("[options]");
    helpFormatter.setHeader("HidGuardian management and control application.");
    helpFormatter.format(std::cout);
}

void ZerberusService::onDeviceArrived(const void* pSender, std::string& name)
{
    auto& logger = Logger::get(std::string(typeid(this).name()) + std::string("::") + std::string(__func__));

    logger.information("New device arrived: %s", name);

    try
    {
        _taskManager.start(new GuardedDevice(name, _clrHost));
    }
    catch (const std::exception& ex)
    {
        logger.fatal("Fatal error: %s", std::string(ex.what()));
    }
}

void ZerberusService::onDeviceRemoved(const void * pSender, std::string & name)
{
    auto& logger = Logger::get(std::string(typeid(this).name()) + std::string("::") + std::string(__func__));

    logger.information("Device %s removed", name);
}

void ZerberusService::initialize(Application & self)
{
    loadConfiguration(); // load default configuration files
    Application::initialize(self);
}

int ZerberusService::main(const std::vector<std::string>& args)
{
    //
    // TODO: maybe there's a better way, consult POCO docs
    // 
    if (config().has("core.oneShot")) {
        return Application::EXIT_OK;
    }

    //
    // Prepare to log to file and optionally console window
    // 

    auto logFilePath = config().getString("core.logging.path", "");

    AutoPtr<SplitterChannel> pSplitter(new SplitterChannel);

    //
    // TODO: also check for path validity
    // 
    if (config().getBool("core.logging[@toFile]", false) && !logFilePath.empty())
    {
        AutoPtr<FileChannel> pFileChannel(new FileChannel(Path::expand(logFilePath)));
        pSplitter->addChannel(pFileChannel);
    }

    //
    // Print to console if run interactively
    // 
    if (!config().getBool("application.runAsService", false))
    {
        AutoPtr<WindowsConsoleChannel> pCons(new WindowsConsoleChannel);
        pSplitter->addChannel(pCons);
    }

    //
    // Prepare logging formatting
    // 
    AutoPtr<PatternFormatter> pPF(new PatternFormatter(config().getString("core.logging.pattern", "%Y-%m-%d %H:%M:%S.%i %s [%p]: %t")));
    AutoPtr<FormattingChannel> pFC(new FormattingChannel(pPF, pSplitter));
    AutoPtr<AsyncChannel> pAsync(new AsyncChannel(pFC));

    //
    // Do we even log?
    // 
    if (config().getBool("core.logging[@enabled]", true))
    {
        Logger::root().setChannel(pAsync);

        if (config().getBool("core.logging[@debug]", false))
        {
            Logger::root().setLevel(Message::PRIO_DEBUG);
        }
    }

    auto& logger = Logger::get(std::string(typeid(this).name()) + std::string("::") + std::string(__func__));

    //
    // Get path to ourself
    // 
    Buffer<char> myPathBuf(MAX_PATH + 1);
    GetModuleFileNameA(
        reinterpret_cast<HINSTANCE>(&__ImageBase),
        myPathBuf.begin(),
        static_cast<DWORD>(myPathBuf.size())
    );
    Path myPath(myPathBuf.begin());
    Path myDir(myPath.parent());

    //
    // Log build version of ourself
    // 
    Buffer<char> versionValueBuffer(2048);
    const DWORD verBufferSize = GetFileVersionInfoSizeA(myPathBuf.begin(), nullptr);
    if (verBufferSize > 0 && verBufferSize <= versionValueBuffer.size())
    {
        //  get the version block from the file
        if (TRUE == GetFileVersionInfoA(
            myPathBuf.begin(),
            NULL,
            verBufferSize,
            versionValueBuffer.begin()
        ))
        {
            UINT length;
            VS_FIXEDFILEINFO *verInfo = nullptr;

            //  Query the version information for neutral language
            if (TRUE == VerQueryValueA(
                versionValueBuffer.begin(),
                "\\",
                reinterpret_cast<LPVOID*>(&verInfo),
                &length))
            {
                //  Pull the version values.
                logger.information("Starting up (version %lu.%lu.%lu.%lu)",
                    static_cast<ULONG>(HIWORD(verInfo->dwProductVersionMS)),
                    static_cast<ULONG>(LOWORD(verInfo->dwProductVersionMS)),
                    static_cast<ULONG>(HIWORD(verInfo->dwProductVersionLS)),
                    static_cast<ULONG>(LOWORD(verInfo->dwProductVersionLS)));
            }
        }
    }

    logger.information("Current directory: %s", myDir.toString());

    std::vector<CoreClrVigil> vigils;
    std::vector<std::string> assemblyPaths;

    //
    // Begin parsing Vigils.xml
    // 
    std::ifstream in("Vigils.xml");
    Poco::XML::InputSource src(in);
    Poco::XML::DOMParser parser;
    AutoPtr<Poco::XML::Document> pDoc = parser.parse(&src);
    Poco::XML::NodeIterator root(pDoc, Poco::XML::NodeFilter::SHOW_ELEMENT);
    Poco::XML::Node* coreclr = root.root()->getNodeByPath("//vigils/coreclr");

    //
    // Loop through Vigil definitions
    // 
    logger.information("Enumerating .NET Core Vigils");
    const auto vigilNodes = coreclr->childNodes();
    for (ULONG i = 0; i < vigilNodes->length(); i++)
    {
        const auto item = vigilNodes->item(i);
        const auto type = item->nodeType();
        if (type == Poco::XML::Node::ELEMENT_NODE)
        {
            const auto attr = item->attributes();
            const auto name = attr->getNamedItem("name")->getNodeValue();

            logger.information("Discovered Vigil %s", name);

            CoreClrVigil vigil;
            vigil.name = name;
            vigil.description = item->getNodeByPath("//description")->innerText();

            Path assemblyPath(item->getNodeByPath("//path")->innerText());
            if (assemblyPath.isRelative())
            {
                Path assemblyPathAbsolute(myDir, assemblyPath);
                vigil.assemblyPath = assemblyPathAbsolute.toString();
                logger.warning("Relative paths not tolerated by CoreCLR, expanding to: %s", vigil.assemblyPath);
            }
            else
            {
                vigil.assemblyPath = assemblyPath.toString();
            }

            vigil.assemblyName = item->getNodeByPath("//assembly")->innerText();
            vigil.className = item->getNodeByPath("//class")->innerText();
            vigil.methodName = item->getNodeByPath("//method")->innerText();

            vigils.push_back(vigil);
            assemblyPaths.push_back(vigil.assemblyPath);
        }
    }
    vigilNodes->release();
    logger.information("Found %lu .NET Core Vigils", static_cast<ULONG>(vigils.size()));

    //
    // Concat & implode assembly directories
    // 
    if (config().has("dotnet.APP_PATHS"))
    {
        assemblyPaths.push_back(config().getString("dotnet.APP_PATHS"));
    }
    std::stringstream assemblyPathStream;
    std::copy(assemblyPaths.begin(), assemblyPaths.end(), std::ostream_iterator<std::string>(assemblyPathStream, ";"));
    config().setString("dotnet.APP_PATHS", assemblyPathStream.str());

    try
    {
        //
        // Initialize CoreCLR host
        // 
        _clrHost = new CoreClrHost(config());
    }
    catch (Poco::Exception& ex)
    {
        logger.fatal("Couldn't load Core CLR: %s", ex.displayText());

        return Application::EXIT_OSFILE;
    }

    //
    // Load discovered Vigils
    // 
    for (auto& vigil : vigils)
    {
        logger.information("Loading .NET Core Vigil %s", vigil.name);

        try
        {
            _clrHost->loadVigil(
                vigil.assemblyName,
                vigil.className,
                vigil.methodName
            );
        }
        catch (Poco::Exception& ex)
        {
            logger.fatal("Couldn't load .NET Core Vigil: %s", ex.displayText());
        }
    }

    //
    // Access control device (establish connection to driver)
    // 
    try
    {
        _controlDevice = new ControlDevice(CONTROL_DEVICE_PATH);
    }
    catch (Poco::Exception& ex)
    {
        logger.fatal("Couldn't create control device: %s", ex.displayText());

        return Application::EXIT_OSFILE;
    }

    //
    // Enumerate existing devices and start guarding them
    // 
    for (auto device : DeviceEnumerator::enumerateDeviceInterface(const_cast<LPGUID>(&GUID_DEVINTERFACE_HIDGUARDIAN)))
    {
        onDeviceArrived(this, device);
    }

    logger.information("Starting listening for new devices");

    //
    // DeviceListener will raise events on device arrival or removal
    // 
    AutoPtr<DeviceListener> dl(new DeviceListener({ GUID_DEVINTERFACE_HIDGUARDIAN }));

    //
    // Listen for arriving devices
    // 
    dl->deviceArrived += Poco::delegate(this, &ZerberusService::onDeviceArrived);

    //
    // Listen for device removal (currently just logs the appearance)
    // 
    dl->deviceRemoved += Poco::delegate(this, &ZerberusService::onDeviceRemoved);

    //
    // Start listening
    // 
    _taskManager.start(dl);

    logger.information("Done, up and running");

    //
    // Print additional information if run interactively
    // 
    if (!config().getBool("application.runAsService", false))
    {
        logger.information("Press CTRL+C to terminate");
    }

    //
    // Block until user or service manager requests a halt
    // 
    waitForTerminationRequest();

    logger.information("Process terminating");

    //
    // Request graceful shutdown from all background tasks
    // 
    _taskManager.cancelAll();
    logger.debug("Requested all tasks to cancel");

    //
    // Wait for all background tasks to shut down
    // 
    logger.debug("Waiting for tasks to stop");
    _taskManager.joinAll();
    logger.debug("Tasks stopped");

    _clrHost->release();

    return Application::EXIT_OK;
}

void ZerberusService::defineOptions(Poco::Util::OptionSet & options)
{
    ServerApplication::defineOptions(options);

    options.addOption(
        Poco::Util::Option("help", "h", "Display this help.")
        .required(false)
        .repeatable(false)
        .binding("core.oneShot")
        .callback(Poco::Util::OptionCallback<ZerberusService>(this, &ZerberusService::help)));

    options.addOption(
        Poco::Util::Option("enumerateDeviceInterface", "e", "Returns a list of instance paths of devices with the specified interface GUID.")
        .required(false)
        .repeatable(true)
        .argument("GUID")
        .binding("core.oneShot")
        .callback(Poco::Util::OptionCallback<ZerberusService>(this, &ZerberusService::enumerateDeviceInterface)));
}

void ZerberusService::handleOption(const std::string & name, const std::string & value)
{
    ServerApplication::handleOption(name, value);
}

