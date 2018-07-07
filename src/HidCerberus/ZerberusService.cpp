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


void ZerberusService::onDeviceArrived(const void* pSender, std::string& name)
{
    auto& logger = Logger::get(std::string(typeid(this).name()) + std::string("::") + std::string(__func__));

    logger.information("New device arrived: %s", name);

    try
    {
        _taskManager.start(new GuardedDevice(name, _hcHandle));
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

ZerberusService::ZerberusService(const std::string & name, PHC_HANDLE handle): Task(name), _hcHandle(handle)
{
    AutoPtr<SplitterChannel> pSplitter(new SplitterChannel);
    AutoPtr<FileChannel> pFileChannel(new FileChannel("HidCerberus.log"));
    pSplitter->addChannel(pFileChannel);

    //
    // Prepare logging formatting
    // 
    AutoPtr<PatternFormatter> pPF(new PatternFormatter("%Y-%m-%d %H:%M:%S.%i %s [%p]: %t"));
    AutoPtr<FormattingChannel> pFC(new FormattingChannel(pPF, pSplitter));
    AutoPtr<AsyncChannel> pAsync(new AsyncChannel(pFC));
}

void ZerberusService::runTask()
{
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

        return;
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
    // Block until user or service manager requests a halt
    // 
    while (!isCancelled()) {
        sleep(1000);
    }

    logger.information("Library unloading");

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
}

