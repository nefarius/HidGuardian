#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string>

#include <Poco/Task.h>
#include <Poco/BasicEvent.h>

using Poco::BasicEvent;

class DeviceListener :
    public Poco::Task
{
    WNDCLASSEX _windowClass;
    HWND _windowHandle;
    HDEVNOTIFY _deviceNotify;
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

public:
    BasicEvent<std::string> deviceArrived;

    DeviceListener();
    ~DeviceListener();

    void runTask() override;
};

