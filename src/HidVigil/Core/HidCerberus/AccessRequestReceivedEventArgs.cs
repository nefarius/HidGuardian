using System;
using HidVigil.Core.Types;

namespace HidVigil.Core.HidCerberus
{
    public class AccessRequestReceivedEventArgs : EventArgs
    {
        public AccessRequestReceivedEventArgs(IAccessRequest accessRequest)
        {
            AccessRequest = accessRequest;
        }

        public IAccessRequest AccessRequest { get; }
    }
}
