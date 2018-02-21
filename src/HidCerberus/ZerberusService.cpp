#include "ZerberusService.h"
#include "PermissionRequestWorker.h"
#include "DeviceEnumerator.h"
#include <devioctl.h>
#include "HidGuardian.h"
#include "GuardedDevice.h"

#define POCO_NO_UNWINDOWS
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
#include <Poco/Data/Session.h>
#include <Poco/Data/SQLite/Connector.h>
#include <Poco/Data/SQLite/Utility.h>
#include <Poco/File.h>
#include <Poco/Buffer.h>
#include <Poco/Util/RegExpValidator.h>
#include <Poco/Util/HelpFormatter.h>

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
using Poco::Data::Session;
using Poco::Data::SQLite::Connector;
using namespace Poco::Data::Keywords;
using Poco::Buffer;
using Poco::Util::RegExpValidator;
using Poco::Util::HelpFormatter;


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

void ZerberusService::displayHelp()
{
    HelpFormatter helpFormatter(options());
    helpFormatter.setCommand(commandName());
    helpFormatter.setUsage("[options]");
    helpFormatter.setHeader("HidGuardian management and control application.");
    helpFormatter.format(std::cout);
}

void ZerberusService::initialize(Application & self)
{
    loadConfiguration(); // load default configuration files
    Application::initialize(self);
}

int ZerberusService::main(const std::vector<std::string>& args)
{
    if (config().has("core.oneShot")) {
        return Application::EXIT_OK;
    }

    //
    // Prepare to log to file and optionally console window
    // 

    auto logFilePath = config().getString("logging.path", "");

    AutoPtr<WindowsConsoleChannel> pCons(new WindowsConsoleChannel);
    AutoPtr<SplitterChannel> pSplitter(new SplitterChannel);

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
    
    Poco::Data::SQLite::Connector::registerConnector();

    //
    // Get database path
    // 
    Poco::File dbFile(config().getString("database.path", "HidCerberus.db"));
    logger.information("Loading database [%s]", dbFile.path());

    //
    // Create database if not found at the provided path
    // 
    if (!dbFile.exists())
    {
        logger.information("Database doesn't exist, creating empty one");

        Session freshDb("SQLite", ":memory:");

        freshDb << R"(CREATE TABLE `AccessRules` (
	                    `HardwareId`    TEXT NOT NULL,
	                    `IsAllowed`     INTEGER NOT NULL,
	                    `IsPermanent`   INTEGER NOT NULL,
	                    `ModuleName`    TEXT,
	                    `ImagePath`     TEXT);)", now;

        Poco::Data::SQLite::Utility::memoryToFile(dbFile.path(), freshDb);
    }

    //
    // Create in-memory database, initialize from file
    // 
    Session session("SQLite", ":memory:");
    Poco::Data::SQLite::Utility::fileToMemory(session, dbFile.path());

    logger.information("Database loaded");

    auto devicePath = config().getString("args.devicePath");

    logger.information("Opening device %s", devicePath);

    AutoPtr<GuardedDevice> dev;

    try
    {
        dev = new GuardedDevice(devicePath, config(), session);
    }
    catch (const std::exception& ex)
    {
        logger.fatal("Fatal error: %s", std::string(ex.what()));
        return Application::EXIT_UNAVAILABLE;
    }    

    logger.information("Done, up and running");

    if (!config().getBool("application.runAsService", false))
    {
        logger.information("Press CTRL+C to terminate");
    }

    waitForTerminationRequest();

    logger.information("Process terminating");

    return Application::EXIT_OK;
}

void ZerberusService::defineOptions(OptionSet & options)
{
    ServerApplication::defineOptions(options);

    options.addOption(
        Option("help", "h", "Display this help.")
        .required(false)
        .repeatable(false)
        .binding("core.oneShot")
        .callback(OptionCallback<ZerberusService>(this, &ZerberusService::help)));

    options.addOption(
        Option("enumerateDeviceInterface", "e", "Returns a list of instance paths of devices with the specified interface GUID.")
        .required(false)
        .repeatable(true)
        .argument("GUID")
        .binding("core.oneShot")
        .callback(OptionCallback<ZerberusService>(this, &ZerberusService::enumerateDeviceInterface)));

    options.addOption(
        Option("devicePath", "d", "The path of the device to guard.")
        .required(true)
        .repeatable(false)
        .argument("path")
        .binding("args.devicePath"));
}

void ZerberusService::handleOption(const std::string & name, const std::string & value)
{
    ServerApplication::handleOption(name, value);
}

