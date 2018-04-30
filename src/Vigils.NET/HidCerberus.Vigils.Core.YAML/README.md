# YAML-based rule processor
TBD

```yaml
rules:
    - name: Test Rule matching a process by name
      description: This rule whitelists any rundll32 process trying to access this device
      hardwareId: 'HID\VID_054C&PID_05C4'
      isAllowed: yes
      isPermanent: yes
      filter:
          processName: rundll32
    - name: Test Rule matching a process by main executable path
      description: Here we match an absolute path
      hardwareId: 'USB\VID_045E&PID_028E'
      isAllowed: yes
      isPermanent: yes
      filter:
          processPath: C:\Temp\SCPUser.exe
    - name: Test Rule matching a Windows Service by name
      description: Windows Services are a thing as well :)
      hardwareId: 'HID\VID_03F0&PID_0024'
      isAllowed: yes
      isPermanent: no
      filter:
          serviceName: WSearch

```