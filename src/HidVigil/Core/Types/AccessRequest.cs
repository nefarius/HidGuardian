using System;
using System.Threading;
using Newtonsoft.Json;

namespace HidVigil.Core.Types
{
    public class AccessRequest
    {
        public AccessRequest()
        {
            RequestId = Guid.NewGuid();
            Signal = new ManualResetEvent(false);
        }

        public Guid RequestId { get; set; }

        public string HardwareId { get; set; }

        public string DeviceId { get; set; }

        public string InstanceId { get; set; }

        public uint ProcessId { get; set; }

        public bool IsHandled { get; set; }

        public bool IsAllowed { get; set; }

        public bool IsPermanent { get; set; }

        [JsonIgnore]
        public ManualResetEvent Signal { get; }

        public override bool Equals(object obj)
        {
            if (ReferenceEquals(null, obj)) return false;
            if (ReferenceEquals(this, obj)) return true;
            if (obj.GetType() != this.GetType()) return false;
            return Equals((AccessRequest) obj);
        }

        protected bool Equals(AccessRequest other)
        {
            return RequestId.Equals(other.RequestId);
        }

        public override int GetHashCode()
        {
            return RequestId.GetHashCode();
        }
    }
}