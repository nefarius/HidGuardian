using System;
using HidVigil.Core.Types;
using HidVigil.Core.Util.Native;
using Newtonsoft.Json;

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
            IntPtr handle,
            string[] hardwareIds,
            int hardwareIdsCount,
            string deviceId,
            string instanceId,
            uint processId
        )
        {
            var args = new AccessRequestReceivedEventArgs(
                AccessRequest.CreateAccessRequest(
                    handle,
                    hardwareIds,
                    deviceId,
                    instanceId,
                    processId
                ));

            AccessRequestReceived?.Invoke(this, args);

            return args.AccessRequest.IsHandled;
        }

        private class AccessRequest : IAccessRequest
        {
            private AccessRequest()
            {
                RequestId = Guid.NewGuid();
            }

            [JsonIgnore]
            public IntPtr NativeHandle { get; set; }

            public Guid RequestId { get; }

            public string[] HardwareIds { get; private set; }

            public string DeviceId { get; private set; }

            public string InstanceId { get; private set; }

            public uint ProcessId { get; private set; }

            [JsonIgnore]
            public bool IsHandled { get; set; }

            public static IAccessRequest CreateAccessRequest(
                IntPtr handle,
                string[] hardwareIds,
                string deviceId,
                string instanceId,
                uint processId)
            {
                return new AccessRequest
                {
                    NativeHandle = handle,
                    HardwareIds = hardwareIds,
                    DeviceId = deviceId,
                    InstanceId = instanceId,
                    ProcessId = processId
                };
            }

            public void SubmitResult(bool isAllowed, bool isPermanent)
            {
                hc_submit_access_request_result(NativeHandle, isAllowed, isPermanent);
            }
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