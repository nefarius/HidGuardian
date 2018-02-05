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


void ZerberusService::initialize(Application & self)
{
    loadConfiguration(); // load default configuration files
    Application::initialize(self);
}

int ZerberusService::main(const std::vector<std::string>& args)
{
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

    AutoPtr<PatternFormatter> pPF(new PatternFormatter(config().getString("logging.pattern", "%Y-%m-%d %H:%M:%S.%i %s [%p]: %t")));
    AutoPtr<FormattingChannel> pFC(new FormattingChannel(pPF, pSplitter));
    AutoPtr<AsyncChannel> pAsync(new AsyncChannel(pFC));

    Logger::root().setChannel(pAsync);

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

    auto threads = config().getInt("threadpool.count", 20);
    Poco::Data::SQLite::Connector::registerConnector();
    Session session("SQLite", config().getString("database.path", "Zerberus.db"));

    SharedPtr<ThreadPool> pPermPool(new ThreadPool(threads, threads));

    std::vector<SharedPtr<PermissionRequestWorker>> workers;

    for (size_t i = 0; i < threads; i++)
    {
        auto worker = new PermissionRequestWorker(controlDevice, session);
        workers.push_back(worker);
        pPermPool->start(*worker);
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

