#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <OAIdl.h>

#include <string>
#include <vector>

#include "HidCerberus.h"

#define POCO_NO_UNWINDOWS
#include <Poco/Task.h>
#include <Poco/Random.h>
#include <Poco/AutoPtr.h>

using Poco::Task;
using Poco::Random;
using Poco::AutoPtr;

class GuardedDevice : public Task
{
    std::string _devicePath;
    HANDLE _deviceHandle = INVALID_HANDLE_VALUE;
    Random _rnd;
    const int _bufferSize = 1024;
    PHC_HANDLE _hcHandle;

	long CreateSafeArrayFromBSTRArray
	(
		BSTR* pBSTRArray,
		ULONG ulArraySize,
		SAFEARRAY** ppSafeArrayReceiver
	);

public:
    GuardedDevice(std::string devicePath, PHC_HANDLE handle);

    void runTask() override;

protected:
    ~GuardedDevice();
};

