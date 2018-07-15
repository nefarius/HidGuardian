# HidGuardian
Windows kernel-mode filter driver restricting access to input peripherals.

## About
`HidGuardian` (abbreviated as `HG`) is an upper filter driver for device classes like `HIDClass` (Keyboards, Mice, Gamepads, Joysticks, Wheels, ...), `XnaComposite` (Xbox 360-compatible controllers) or `XboxComposite` (Xbox One-compatible controllers) therefore targeting and attaching itself to every input device connected to the system. A user-mode companion library and service initialize the driver and listen for access request events containing information about the device and the process ID demanding access. 

## Workflow
Regardless of used higher-level API ([DirectInput](https://msdn.microsoft.com/en-us/library/windows/desktop/ee416842(v=vs.85).aspx), [XInput](https://msdn.microsoft.com/en-us/library/windows/desktop/hh405053(v=vs.85).aspx), [Raw Input](https://msdn.microsoft.com/en-us/library/windows/desktop/ms645536(v=vs.85).aspx)), accessing (opening) an input device will at its core result in a call of the [`CreateFile`](<https://docs.microsoft.com/en-us/windows/desktop/api/fileapi/nf-fileapi-createfilea>) function, invoking the [`EVT_WDF_DEVICE_FILE_CREATE`](<https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/content/wdfdevice/nc-wdfdevice-evt_wdf_device_file_create>) callback within the filter driver. Without `HidGuardian`, this request would get answered by the original devices function driver, typically resulting in a successful completion giving the user-mode application a valid handle to said device. With `HG` joining the party, this flow gets altered as follows:

![Flowchart](https://vigem.org/wp-content/uploads/2018/07/2018-07-15_13-38-25.png)

<details><summary>Show source</summary>
<p>

Flowchart created with [flowchart.js](<https://github.com/adrai/flowchart.js>)

```js
open=>start: Device Access Request
allowed=>end: Access granted
denied=>end: Access denied
hg=>operation: Request reaches HidGuardian
hc_present=>condition: Is HidCerberus present?
default=>operation: Apply default action
allow=>operation: Allow access
forward=>subroutine: Forward request to lower driver
ask=>operation: Ask HidCerberus what to do
submit=>subroutine: Submit details to HC (HWID, PID, ...)
wait=>subroutine: Wait for decision through HidVigil
pass=>subroutine: Pass result back to HidGuardian
eval=>condition: Access granted?
deny=>operation: Fail request with STATUS_ACCESS_DENIED

open->hg->hc_present
hc_present(no, right)->default->allow->forward->allowed
hc_present(yes)->ask->submit->wait->pass->eval
eval(yes, right)->allow
eval(no)->deny->denied
```

</p>
</details>

As a generic filter driver has limited capabilities of querying details about process IDs (name of the main executable, path to image file, ownership, etc.) a little help from a user-mode component (labeled `HidCerberus`) is required to reverse-lookup the process ID and make the decision if the open request shall be granted or denied.

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
devcon.exe classfilter HIDClass upper !HidGuardian
devcon.exe classfilter XnaComposite upper !HidGuardian
devcon.exe classfilter XboxComposite upper !HidGuardian
devcon.exe remove Root\HidGuardian
```
Re-plug your devices or reboot the system for the driver to get unloaded and removed.