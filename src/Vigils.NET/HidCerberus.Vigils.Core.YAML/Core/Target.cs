using System;

namespace HidCerberus.Vigils.Core.YAML.Core
{
    public class Target
    {
        public string HardwareId { get; set; }

        public string DeviceId { get; set; }

        public string InstanceId { get; set; }

        public override bool Equals(object obj)
        {
            var other = (Target)obj;

            var hwId = other.HardwareId?.Equals(HardwareId, StringComparison.InvariantCultureIgnoreCase);
            var devId = other.DeviceId?.Equals(DeviceId, StringComparison.InvariantCultureIgnoreCase);
            var instId = other.InstanceId?.Equals(InstanceId, StringComparison.InvariantCultureIgnoreCase);

            if (hwId.HasValue && devId.HasValue && instId.HasValue)
                return (hwId.Value && devId.Value && instId.Value);

            if (hwId.HasValue && devId.HasValue && !instId.HasValue)
                return (hwId.Value && devId.Value);

            if (hwId.HasValue && !devId.HasValue && instId.HasValue)
                return (hwId.Value && instId.Value);

            return hwId.Value;
        }
    }
}