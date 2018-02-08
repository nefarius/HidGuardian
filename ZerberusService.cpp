#include "ZerberusService.h"
#include "PermissionRequestWorker.h"

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
#include <Poco/ThreadPool.h>
#include <Poco/SharedPtr.h>
#include <Poco/Data/Session.h>
#include <Poco/Data/SQLite/Connector.h>
#include <Poco/Data/SQLite/Utility.h>
#include <Poco/File.h>

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
using Poco::ThreadPool;
using Poco::SharedPtr;
using Poco::Data::Session;
using Poco::Data::SQLite::Connector;
using namespace Poco::Data::Keywords;


void ZerberusService::initialize(Application & self)
{
    loadConfiguration(); // load default configuration files
    Application::initialize(self);
}

int ZerberusService::main(const std::vector<std::string>& args)
{
    //
    // Prepare to log to file and optionally console window
    // 
    AutoPtr<FileChannel> pFileChannel(new FileChannel(Path::expand(config().getString("logging.path"))));
    AutoPtr<WindowsConsoleChannel> pCons(new WindowsConsoleChannel);
    AutoPtr<SplitterChannel> pSplitter(new SplitterChannel);
    pSplitter->addChannel(pFileChannel);

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
    if (config().getBool("logging.enabled", false))
    {
        Logger::root().setChannel(pAsync);

        if (config().getBool("logging.debug", false))
        {
            Logger::root().setLevel(Message::PRIO_DEBUG);
        }
    }

    auto& logger = Logger::get(std::string(typeid(this).name()) + std::string("::") + std::string(__func__));

    logger.information("Opening control device");

    HANDLE controlDevice = CreateFile(L"\\\\.\\HidGuardian",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED,
        nullptr);

    if (controlDevice == INVALID_HANDLE_VALUE) {
        logger.fatal("Couldn't open control device");
        return Application::EXIT_UNAVAILABLE;
    }

    logger.information("Control device opened");

    Poco::Data::SQLite::Connector::registerConnector();

    //
    // Get database path
    // 
    Poco::File dbFile(config().getString("database.path", "Zerberus.db"));
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

    logger.information("Spawning worker threads");

    auto threads = config().getInt("threadpool.count", 20);
    SharedPtr<ThreadPool> pPermPool(new ThreadPool(threads, threads));
    std::vector<SharedPtr<PermissionRequestWorker>> workers;

    for (size_t i = 0; i < threads; i++)
    {
        auto worker = new PermissionRequestWorker(controlDevice, session);
        workers.push_back(worker);
        pPermPool->start(*worker);
    }

    logger.information("Done, up and running");

    if (!config().getBool("application.runAsService", false))
    {
        logger.information("Press CTRL+C to terminate");
    }

    waitForTerminationRequest();

    CloseHandle(controlDevice);
    pPermPool->joinAll();
    logger.information("Process terminating");

    return Application::EXIT_OK;
}

void ZerberusService::defineOptions(OptionSet & options)
{
    ServerApplication::defineOptions(options);
}

void ZerberusService::handleOption(const std::string & name, const std::string & value)
{
    ServerApplication::handleOption(name, value);
}

