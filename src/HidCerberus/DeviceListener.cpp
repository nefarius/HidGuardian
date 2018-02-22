#include "DeviceListener.h"
#include <initguid.h>
#include <usbiodef.h>
#include <Dbt.h>

#include <locale>
#include <codecvt>
#include <Poco/Logger.h>


LRESULT DeviceListener::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DeviceListener *pThis;

    if (msg == WM_NCCREATE)
    {
        pThis = static_cast<DeviceListener*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);

        SetLastError(0);
        if (!SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis)))
        {
            if (GetLastError() != 0)
                return FALSE;
        }
    }
    else
    {
        pThis = reinterpret_cast<DeviceListener*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (pThis)
    {
        PDEV_BROADCAST_HDR hdr;

        switch (msg)
        {
        case WM_DEVICECHANGE:

            switch (wParam)
            {
            case DBT_DEVICEARRIVAL:

                hdr = reinterpret_cast<PDEV_BROADCAST_HDR>(lParam);

                if (hdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
                {
                    const auto iface = reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(lParam);

                    using convert_type = std::codecvt_utf8<wchar_t>;
                    std::wstring_convert<convert_type, wchar_t> converter;

                    pThis->deviceArrived.notifyAsync(pThis, converter.to_bytes(iface->dbcc_name));
                }

            default:
                break;
            }

            break;
        case WM_NCDESTROY:
            PostQuitMessage(0);
        default:
            break;
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

DeviceListener::DeviceListener() : Task("DeviceListener"), _windowHandle(nullptr), _deviceNotify(nullptr)
{
}

DeviceListener::~DeviceListener()
{
    if (_deviceNotify != nullptr) {
        UnregisterDeviceNotification(_deviceNotify);
    }
    
    if (_windowHandle != nullptr) {
        DestroyWindow(_windowHandle);
    }

    UnregisterClass(_windowClass.lpszClassName, _windowClass.hInstance);
}

void DeviceListener::runTask()
{
    auto& logger = Poco::Logger::get(std::string(typeid(this).name()) + std::string("::") + std::string(__func__));

    logger.debug("Listening for window messages");

    ZeroMemory(&_windowClass, sizeof(WNDCLASSEX));

    _windowClass.cbSize = sizeof(WNDCLASSEX);
    _windowClass.style = CS_HREDRAW | CS_VREDRAW;
    _windowClass.lpfnWndProc = WndProc;
    _windowClass.lpszClassName = L"TempMsgOnlyWindow";

    _windowClass.hInstance = GetModuleHandle(nullptr);
    if (_windowClass.hInstance == nullptr)
    {
        throw std::runtime_error("Could not get the instance handle");
    }

    if (!RegisterClassEx(&_windowClass))
    {
        throw std::runtime_error("Could not get register the window class");
    }

    _windowHandle = CreateWindow(
        _windowClass.lpszClassName,
        L"Temporary Message-only Window",
        WS_OVERLAPPEDWINDOW,
        0,
        0,
        100,
        100,
        NULL,
        NULL,
        _windowClass.hInstance,
        this);

    if (_windowHandle == nullptr)
    {
        throw std::runtime_error("Could not get create the temporary window");
    }

    DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
    ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));

    NotificationFilter.dbcc_size = sizeof(NotificationFilter);
    NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    NotificationFilter.dbcc_reserved = 0;

    NotificationFilter.dbcc_classguid = GUID_DEVINTERFACE_USB_DEVICE;

    _deviceNotify = RegisterDeviceNotification(_windowHandle, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);

    if (_deviceNotify == nullptr) {
        throw std::runtime_error("Couldn't register for device notification");
    }

    //
    // Message loop
    // 
    MSG msg;
    while (!sleep(25))
    {
        PeekMessage(&msg, _windowHandle, 0, 0, PM_REMOVE);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    logger.debug("Listener terminating");
}
