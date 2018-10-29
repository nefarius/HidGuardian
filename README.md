![Devil](devil.png)

# HidGuardian

Blocks various input devices from being accessed by user-mode applications.

[![Build status](https://ci.appveyor.com/api/projects/status/6i1f715tm961idmg/branch/master?svg=true)](https://ci.appveyor.com/project/nefarius/hidguardian/branch/master) [![Discord](https://img.shields.io/discord/346756263763378176.svg)](https://discord.gg/QTJpBX5) [![Website](https://img.shields.io/website-up-down-green-red/https/vigem.org.svg?label=ViGEm.org)](https://vigem.org/) [![PayPal Donate](https://img.shields.io/badge/paypal-donate-blue.svg)](<https://paypal.me/NefariusMaximus>) [![Support on Patreon](https://img.shields.io/badge/patreon-donate-orange.svg)](<https://www.patreon.com/nefarius>) [![GitHub followers](https://img.shields.io/github/followers/nefarius.svg?style=social&label=Follow)](https://github.com/nefarius) [![Twitter Follow](https://img.shields.io/twitter/follow/nefariusmaximus.svg?style=social&label=Follow)](https://twitter.com/nefariusmaximus)

![Disclaimer](http://nefarius.at/public/Alpha-Disclaimer.png)

**ATTENTION: this is in no way a finished product, don't use in production!**

**Please consider the master branch unstable until the disclaimer disappears!**

## The Problem

Games and other user-mode applications enumerate Joysticks, Gamepads and similar devices through various well-known APIs ([DirectInput](https://msdn.microsoft.com/en-us/library/windows/desktop/ee416842(v=vs.85).aspx), [XInput](https://msdn.microsoft.com/en-us/library/windows/desktop/hh405053(v=vs.85).aspx), [Raw Input](https://msdn.microsoft.com/en-us/library/windows/desktop/ms645536(v=vs.85).aspx)) and continuously read their reported input states. The primary collection of devices available through DirectInput are [HID](https://en.wikipedia.org/wiki/Human_interface_device)-Class based devices. When emulating virtual devices with ViGEm the system (and subsequently the application) may not be able to distinguish between e.g. a "real" physical HID Gamepad which acts as a "feeder" and the virtual ViGEm device, therefore suffer from side effects like doubled input. Since coming up with a solution for each application available would become quite tedious a more generalized approach was necessary to reliably solve these issues.

## The Semi-Solution

A common way for intercepting the Game's communication with the input devices would be hooking the mentioned input APIs within the target process. While a stable and user-friendly implementation for the end-user might be achievable for some processes, targeting the wide variety of Games available on the market is a difficult task. Hooking APIs involves manipulating the target processes memory which also might falsely trigger Anti-Cheat systems and ban innocent users.

## The Real Solution

Meet `HidGuardian`: a Windows kernel-mode driver sitting on top of every input device attached to the system. With its companion user-mode component [`HidVigil`](https://github.com/ViGEm/HidVigil) it morphs into a powerful device firewall toolkit allowing for fine-grained access restrictions to input devices.

## Demo

Outdated, replace with demo video.

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

## Contribute

### Bugs & Features

Found a bug and want it fixed? Open a detailed issue on the [GitHub issue tracker](../../issues)!

Have an idea for a new feature? [Check out the project board](https://projects.vigem.org/public/board/26b2bebf0b0092d017f9fd629354cb6e2b255c087f13f8594190f43e2055), maybe it's already on there! If not, let's have a chat about your request on [Discord](https://discord.vigem.org) or the [community forums](https://forums.vigem.org).

### Questions & Support

Please respect that the GitHub issue tracker isn't a helpdesk. We offer a [Discord server](https://discord.vigem.org) and [forums](https://forums.vigem.org), where you're welcome to check out and engage in discussions!
