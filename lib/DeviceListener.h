#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string>
#include <vector>
#include <initializer_list>

#include <Poco/Task.h>
#include <Poco/BasicEvent.h>

using Poco::BasicEvent;

class DeviceListener :
    public Poco::Task
{
    WNDCLASSEX _windowClass{};
    HWND _windowHandle;
    std::vector<HDEVNOTIFY> _deviceNotifiers;
    std::vector<GUID> _interfaceGuids;
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

public:
    BasicEvent<std::string> deviceArrived;
    BasicEvent<std::string> deviceRemoved;

    DeviceListener(std::initializer_list<GUID> interfaceGuids);
    ~DeviceListener();

    void runTask() override;
};

