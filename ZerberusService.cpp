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


int ZerberusService::main(const std::vector<std::string>& args)
{    
    AutoPtr<FileChannel> pFileChannel(new FileChannel(Path::expand(_logfile)));
    AutoPtr<WindowsConsoleChannel> pCons(new WindowsConsoleChannel);
    AutoPtr<SplitterChannel> pSplitter(new SplitterChannel);

    pSplitter->addChannel(pFileChannel);
    pSplitter->addChannel(pCons);

    AutoPtr<PatternFormatter> pPF(new PatternFormatter("%Y-%m-%d %H:%M:%S.%i %s [%p]: %t"));
    AutoPtr<FormattingChannel> pFC(new FormattingChannel(pPF, pSplitter));
    AutoPtr<AsyncChannel> pAsync(new AsyncChannel(pFC));

    Logger::root().setChannel(pAsync);

    auto& logger = Logger::get(std::string(typeid(this).name()) + std::string("::") + std::string(__func__));

    logger.information("Hello there");

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
        
    SharedPtr<ThreadPool> pPermPool(new ThreadPool(_threadCount, _threadCount));

    std::vector<SharedPtr<PermissionRequestWorker>> workers;

    for (size_t i = 0; i < _threadCount; i++)
    {
        auto worker = new PermissionRequestWorker(controlDevice);
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

    options.addOption(
        Option("logfile", "l", "Path to logfile")
        .required(true)
        .repeatable(false)
        .argument("<logfile>"));
    options.addOption(
        Option("threads", "t", "Count of worker threads")
        .required(false)
        .repeatable(false)
        .argument("<threads>"));
}

void ZerberusService::handleOption(const std::string & name, const std::string & value)
{
    ServerApplication::handleOption(name, value);

    if (name == "logfile") _logfile = value;
    if (name == "threads") _threadCount = stoi(value);
}

