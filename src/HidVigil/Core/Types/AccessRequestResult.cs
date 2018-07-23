using System;

namespace HidVigil.Core.Types
{
    public class AccessRequestResult
    {
        public Guid RequestId { get; set; }

        public bool IsAllowed { get; set; }

        public bool IsPermanent { get; set; }
    }
}