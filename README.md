![Devil](devil.png)

# HidGuardian

Blocks various input devices from being accessed by user-mode applications.

---

This was a research project and an attempt of turning the proof of concept project [HideDS4](https://github.com/nefarius/HideDS4) into a Windows kernel-mode filter driver that allows system-wide hiding of joysticks and gamepads, addressing doubled-input issues in games running with remapping utilities. It has been discontinued [in favour of better solutions](https://github.com/ViGEm/HidHide). The code will stay up for anyone to use as either an inspiration or a negative example 😜 Do bear in mind, that the code may contain unaddressed issues! Compile and use at your own risk! No support or binaries provided!

---

## The Problem

Games and other user-mode applications enumerate Joysticks, Gamepads and similar devices through various well-known APIs ([DirectInput](https://msdn.microsoft.com/en-us/library/windows/desktop/ee416842(v=vs.85).aspx), [XInput](https://msdn.microsoft.com/en-us/library/windows/desktop/hh405053(v=vs.85).aspx), [Raw Input](https://msdn.microsoft.com/en-us/library/windows/desktop/ms645536(v=vs.85).aspx)) and continuously read their reported input states. The primary collection of devices available through DirectInput are [HID](https://en.wikipedia.org/wiki/Human_interface_device)-Class based devices. When emulating virtual devices with ViGEm the system (and subsequently the application) may not be able to distinguish between e.g. a "real" physical HID Gamepad which acts as a "feeder" and the virtual ViGEm device, therefore suffer from side effects like doubled input. Since coming up with a solution for each application available would become quite tedious a more generalized approach was necessary to reliably solve these issues.

## The Semi-Solution

A common way for intercepting the Game's communication with the input devices would be hooking the mentioned input APIs within the target process. While a stable and user-friendly implementation for the end-user might be achievable for some processes, targeting the wide variety of Games available on the market is a difficult task. Hooking APIs involves manipulating the target processes memory which also might falsely trigger Anti-Cheat systems and ban innocent users.

## The Real Solution

Meet `HidGuardian`: a Windows kernel-mode driver sitting on top of every input device attached to the system. With its companion user-mode component [`HidCerberus`](https://github.com/ViGEm/HidCerberus) it morphs into a powerful device firewall toolkit allowing for fine-grained access restrictions to input devices.

## Supported Systems

The driver is built for Windows 7/8/8.1/10 (x86 and amd64).

## How to build

### Prerequisites

- Visual Studio **2017** ([Community Edition](https://www.visualstudio.com/thank-you-downloading-visual-studio/?sku=Community&rel=15) is just fine)
- [WDK for Windows 10, version 1803](https://developer.microsoft.com/en-us/windows/hardware/windows-driver-kit)
- [.NET Core SDK 2.1](https://www.microsoft.com/net/download/dotnet-core/2.1) (or greater, required for building only)

You can either build directly within Visual Studio or in PowerShell by running the build script:

```PowerShell
.\build.ps1
```

Do bear in mind that you'll need to **sign** the driver to use it without [test mode](<https://technet.microsoft.com/en-us/ff553484(v=vs.96)>).
