# HidGuardian
Blocks HID devices from being accessed by user-mode applications.

## The Problem
Games and other user-mode applications enumerate Joysticks, Gamepads and similar devices through various well-known APIs ([DirectInput](https://msdn.microsoft.com/en-us/library/windows/desktop/ee416842(v=vs.85).aspx), [XInput](https://msdn.microsoft.com/en-us/library/windows/desktop/hh405053(v=vs.85).aspx), [Raw Input](https://msdn.microsoft.com/en-us/library/windows/desktop/ms645536(v=vs.85).aspx)) and continously read their reported input states. The primary collection of devices available through DirectInput are [HID](https://en.wikipedia.org/wiki/Human_interface_device)-Class based devices. When emulating virtual devices with ViGEm the system (and subsequently the application) may not be able to distinguish between e.g. a "real" physical HID Gamepad which acts as a "feeder" and the virtual ViGEm device, therefore suffer from side effects like doubled input. Since coming up with a solution for each application available would become quite tedious a more generalized approach was necessary to reliably solve these issues.

## The Semi-Solution
A common way for intercepting the Game's communication with the input devices would be hooking the mentioned input APIs within the target process. While a stable and user-friendly implementation for the end-user might be achievable for some processes, targeting the wide variety of Games available on the market is a difficult task. Hooking APIs involves manipulating the target processes memory which also might falsely trigger Anti-Cheat systems and ban innocent users.

## The Real Solution
`HidGuardian` is an upper filter driver for the `HIDClass` device class therefore targeting and attaching itself to every HID device connected to the system. On startup it queries the `AffectedDevices` value in the service's `Parameters` key to check if the current device in the driver stack is "blacklisted". If a matching Hardware ID is found, every call of the `CreateFile(...)` API will be answered with an `ERROR_ACCESS_DENIED` thus failing the attempt to open a file handle and communicate with the affected device. If the guardian is attached to a device which shouldn't get blocked it will unload itself from the driver stack.

## Demo
Sony DualShock 4 and generic USB Gamepad connected:

![](http://content.screencast.com/users/Nefarius/folders/Snagit/media/f7532345-da15-41f8-b403-1d3c42ace1a9/11.19.2016-19.33.png)

`HidGuardian.sys` active and hiding the DualShock 4:

![](http://content.screencast.com/users/Nefarius/folders/Snagit/media/08741f7b-8272-4d16-9c18-0376f716dc42/11.19.2016-19.28.png)

## Manual Installation
```
devcon.exe install HidGuardian.inf Root\HidGuardian
devcon.exe classfilter HIDClass upper -HidGuardian
```

## Manual Removal
```
devcon.exe remove Root\HidGuardian
devcon.exe classfilter HIDClass upper !HidGuardian
```
Re-plug your devices or reboot the system for the driver to get unloaded and removed.
