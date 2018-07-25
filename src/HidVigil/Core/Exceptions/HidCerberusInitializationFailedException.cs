using System;
using System.Runtime.Serialization;

namespace HidVigil.Core.Exceptions
{
    public class HidCerberusInitializationFailedException : Exception
    {
        public HidCerberusInitializationFailedException()
        {
        }

        public HidCerberusInitializationFailedException(string message) : base(message)
        {
        }

        public HidCerberusInitializationFailedException(string message, Exception innerException) : base(message, innerException)
        {
        }

        protected HidCerberusInitializationFailedException(SerializationInfo info, StreamingContext context) : base(info, context)
        {
        }
    }
}