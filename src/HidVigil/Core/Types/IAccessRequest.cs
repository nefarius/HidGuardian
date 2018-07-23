using System;

namespace HidVigil.Core.Types
{
    public interface IAccessRequest
    {
        Guid RequestId { get; }

        string[] HardwareIds { get; }

        string DeviceId { get; }

        string InstanceId { get; }

        uint ProcessId { get; }

        bool IsHandled { get; set; }

        void SubmitResult(bool isAllowed, bool isPermanent);
    }
}
