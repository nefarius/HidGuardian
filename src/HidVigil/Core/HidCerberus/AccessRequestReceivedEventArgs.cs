using System;

namespace HidVigil.Core.HidCerberus
{
    public class AccessRequestReceivedEventArgs : EventArgs
    {
        public AccessRequestReceivedEventArgs(string hardwareId, string deviceId, string instanceId, uint processId)
        {
            HardwareId = hardwareId;
            DeviceId = deviceId;
            InstanceId = instanceId;
            ProcessId = processId;
        }

        public string HardwareId { get; }

        public string DeviceId { get; }

        public string InstanceId { get; }

        public uint ProcessId { get; }

        public bool IsHandled { get; set; }

        public bool IsAllowed { get; set; }

        public bool IsPermanent { get; set; }
    }
}