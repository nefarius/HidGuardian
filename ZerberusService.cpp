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
    std::string logfile("%PROGRAMDATA%\\Zerberus.log");

    AutoPtr<FileChannel> pFileChannel(new FileChannel(Path::expand(logfile)));
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
        return -1;
    }

    std::vector<SharedPtr<PermissionRequestWorker>> workers;

    for (size_t i = 0; i < 15; i++)
    {
        auto worker = new PermissionRequestWorker(controlDevice);
        workers.push_back(worker);
        ThreadPool::defaultPool().start(*worker);
    }

    waitForTerminationRequest();

    CloseHandle(controlDevice);

    return Application::EXIT_OK;
}

ZerberusService::ZerberusService()
{
}


ZerberusService::~ZerberusService()
{
}
