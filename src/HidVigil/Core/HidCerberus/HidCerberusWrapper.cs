using System;
using HidVigil.Core.Util.Native;

namespace HidVigil.Core.HidCerberus
{
    public partial class HidCerberusWrapper : NativeLibraryWrapper<HidCerberusWrapper>, IDisposable
    {
        public delegate void AccessRequestReceivedEventHandler(object sender, AccessRequestReceivedEventArgs args);

        private readonly EvtHcProcessAccessRequest _callback;

        private readonly IntPtr _hcHandle;

        /// <summary>
        ///     Loads native library and registers callback for handling access requests.
        /// </summary>
        public HidCerberusWrapper()
        {
            LoadNativeLibrary("HidCerberus", @"x86\HidCerberus.dll", @"x64\HidCerberus.dll");

            _hcHandle = hc_init();
            _callback = ProcessAccessRequest;
            hc_register_access_request_event(_hcHandle, _callback);
        }

        /// <summary>
        ///     Gets called when an access request is incoming from the driver.
        /// </summary>
        public event AccessRequestReceivedEventHandler AccessRequestReceived;

        private bool ProcessAccessRequest(
            string hardwareId,
            string deviceId,
            string instanceId,
            uint processId,
            out bool isAllowed,
            out bool isPermananet
        )
        {
            var args = new AccessRequestReceivedEventArgs(
                hardwareId,
                deviceId,
                instanceId,
                processId);

            AccessRequestReceived?.Invoke(this, args);

            isAllowed = args.IsAllowed;
            isPermananet = args.IsPermanent;

            return args.IsHandled;
        }

        #region IDisposable Support

        private bool _disposedValue; // To detect redundant calls

        protected virtual void Dispose(bool disposing)
        {
            if (!_disposedValue)
            {
                if (disposing)
                {
                    // TODO: dispose managed state (managed objects).
                }

                // TODO: free unmanaged resources (unmanaged objects) and override a finalizer below.
                // TODO: set large fields to null.
                hc_shutdown(_hcHandle);

                _disposedValue = true;
            }
        }

        // TODO: override a finalizer only if Dispose(bool disposing) above has code to free unmanaged resources.
        ~HidCerberusWrapper()
        {
            // Do not change this code. Put cleanup code in Dispose(bool disposing) above.
            Dispose(false);
        }

        // This code added to correctly implement the disposable pattern.
        public void Dispose()
        {
            // Do not change this code. Put cleanup code in Dispose(bool disposing) above.
            Dispose(true);
            // TODO: uncomment the following line if the finalizer is overridden above.
            GC.SuppressFinalize(this);
        }

        #endregion
    }
}