![](devil.png)

# HidGuardian
Blocks various input devices from being accessed by user-mode applications.

[![Build status](https://ci.appveyor.com/api/projects/status/6i1f715tm961idmg/branch/master?svg=true)](https://ci.appveyor.com/project/nefarius/hidguardian/branch/master) [![Discord](https://img.shields.io/discord/346756263763378176.svg)](https://discord.gg/QTJpBX5)  [![Website](https://img.shields.io/website-up-down-green-red/https/vigem.org.svg?label=ViGEm.org)](https://vigem.org/) [![PayPal Donate](https://img.shields.io/badge/paypal-donate-blue.svg)](<https://paypal.me/NefariusMaximus>) [![Support on Patreon](https://img.shields.io/badge/patreon-donate-orange.svg)](<https://www.patreon.com/nefarius>) [![GitHub followers](https://img.shields.io/github/followers/nefarius.svg?style=social&label=Follow)](https://github.com/nefarius) [![Twitter Follow](https://img.shields.io/twitter/follow/nefariusmaximus.svg?style=social&label=Follow)](https://twitter.com/nefariusmaximus)

![Disclaimer](http://nefarius.at/public/Alpha-Disclaimer.png)

**Please consider the master branch unstable until the disclaimer disappears!**

## The Problem
Games and other user-mode applications enumerate Joysticks, Gamepads and similar devices through various well-known APIs ([DirectInput](https://msdn.microsoft.com/en-us/library/windows/desktop/ee416842(v=vs.85).aspx), [XInput](https://msdn.microsoft.com/en-us/library/windows/desktop/hh405053(v=vs.85).aspx), [Raw Input](https://msdn.microsoft.com/en-us/library/windows/desktop/ms645536(v=vs.85).aspx)) and continously read their reported input states. The primary collection of devices available through DirectInput are [HID](https://en.wikipedia.org/wiki/Human_interface_device)-Class based devices. When emulating virtual devices with ViGEm the system (and subsequently the application) may not be able to distinguish between e.g. a "real" physical HID Gamepad which acts as a "feeder" and the virtual ViGEm device, therefore suffer from side effects like doubled input. Since coming up with a solution for each application available would become quite tedious a more generalized approach was necessary to reliably solve these issues.

## The Semi-Solution
A common way for intercepting the Game's communication with the input devices would be hooking the mentioned input APIs within the target process. While a stable and user-friendly implementation for the end-user might be achievable for some processes, targeting the wide variety of Games available on the market is a difficult task. Hooking APIs involves manipulating the target processes memory which also might falsely trigger Anti-Cheat systems and ban innocent users.

## The Real Solution
`HidGuardian` is an upper filter driver for device classes like `HIDClass`, `XnaComposite` or `XboxComposite` therefore targeting and attaching itself to every input device connected to the system. On startup it queries the `AffectedDevices` value in the service's `Parameters` key to check if the current device in the driver stack is "blacklisted". If a matching Hardware ID is found, every call of the `CreateFile(...)` API will be queued until [the user-mode service](src/HidCerberus) has decided if the request is allowed or shall be blocked. If the result of the decision denies access, the original open request will be answered with the status `ERROR_ACCESS_DENIED` thus failing the attempt to open a file handle and communicate with the affected device. If allowed the open request will simply get forwarded in the stack untouched. If the guardian is attached to a device which shouldn't get blocked it will unload itself from the driver stack.

## Demo
Sony DualShock 4 and generic USB Gamepad connected:

![](https://lh3.googleusercontent.com/-VKcvDa-Ejms/WnB7HxOBG0I/AAAAAAAAAzc/dvFV_Qtycf8_bH7MYKwHln6ecKt8wmhHgCHMYCw/s0/11.19.2016-19.33.png)

`HidGuardian.sys` active and hiding the DualShock 4:

![](https://lh3.googleusercontent.com/-6_EXN7RwMcM/WnB7NaxorHI/AAAAAAAAAzg/FDKOJVyn39cp3zcqY-B7kWmOEeAfherVQCHMYCw/s0/11.19.2016-19.28.png)

## Manual Installation
Get [`devcon`](https://downloads.vigem.org/other/microsoft/devcon.zip) and execute:
```
devcon.exe install HidGuardian.inf Root\HidGuardian
devcon.exe classfilter HIDClass upper -HidGuardian
devcon.exe classfilter XnaComposite upper -HidGuardian
devcon.exe classfilter XboxComposite upper -HidGuardian
```

## Manual Removal
Get [`devcon`](https://downloads.vigem.org/other/microsoft/devcon.zip) and execute:
```
devcon.exe remove Root\HidGuardian
devcon.exe classfilter HIDClass upper !HidGuardian
devcon.exe classfilter XnaComposite upper !HidGuardian
devcon.exe classfilter XboxComposite upper !HidGuardian
```
Re-plug your devices or reboot the system for the driver to get unloaded and removed.
