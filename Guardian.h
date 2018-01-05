#pragma once

NTSTATUS AmIAffected(PDEVICE_CONTEXT DeviceContext);

BOOLEAN AmIWhitelisted(DWORD Pid);
