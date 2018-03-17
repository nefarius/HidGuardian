#include "ZerberusService.h"
#include "DeviceEnumerator.h"
#include "GuardedDevice.h"
#include "DeviceListener.h"

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
#include "ControlDevice.h"
#include "CoreClrHost.h"

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

    _clrHost = new CoreClrHost(config());

    _clrHost->loadVigil(
        "Test",
        "Test.Demo", 
        "ProcessAccessRequest"
    );

    //
    // Prepare to log to file and optionally console window
    // 

    auto logFilePath = config().getString("logging.path", "");

    AutoPtr<WindowsConsoleChannel> pCons(new WindowsConsoleChannel);
    AutoPtr<SplitterChannel> pSplitter(new SplitterChannel);

    //
    // TODO: also check for path validity
    // 
    if (!logFilePath.empty())
    {
        AutoPtr<FileChannel> pFileChannel(new FileChannel(Path::expand(logFilePath)));
        pSplitter->addChannel(pFileChannel);
    }

    //
    // Print to console if run interactively
    // 
    if (!config().getBool("application.runAsService", false))
    {
        pSplitter->addChannel(pCons);
    }

    //
    // Prepare logging formatting
    // 
    AutoPtr<PatternFormatter> pPF(new PatternFormatter(config().getString("logging.pattern", "%Y-%m-%d %H:%M:%S.%i %s [%p]: %t")));
    AutoPtr<FormattingChannel> pFC(new FormattingChannel(pPF, pSplitter));
    AutoPtr<AsyncChannel> pAsync(new AsyncChannel(pFC));

    //
    // Do we even log?
    // 
    if (config().getBool("logging.enabled", true))
    {
        Logger::root().setChannel(pAsync);

        if (config().getBool("logging.debug", false))
        {
            Logger::root().setLevel(Message::PRIO_DEBUG);
        }
    }

    auto& logger = Logger::get(std::string(typeid(this).name()) + std::string("::") + std::string(__func__));


    AutoPtr<ControlDevice> cd;

    try
    {
        cd = new ControlDevice(CONTROL_DEVICE_PATH);
    }
    catch(std::exception& ex)
    {
        logger.fatal("Couldn't create control device: %s", std::string(ex.what()));

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
    // Listen fordevice removal (currently just loggs the occurance)
    // 
    dl->deviceRemoved += Poco::delegate(this, &ZerberusService::onDeviceRemoved);

    //
    // Start listening
    // 
    _taskManager.start(dl);

    logger.information("Done, up and running");

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

    //
    // Wait for all background tasks to shut down
    // 
    _taskManager.joinAll();

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

