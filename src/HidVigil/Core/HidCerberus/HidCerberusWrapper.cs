using System;
using HidVigil.Core.Exceptions;
using HidVigil.Core.Types;
using HidVigil.Core.Util.Native;
using Newtonsoft.Json;

namespace HidVigil.Core.HidCerberus
{
    public partial class HidCerberusWrapper : NativeLibraryWrapper<HidCerberusWrapper>, IDisposable
    {
        public delegate void AccessRequestReceivedEventHandler(object sender, AccessRequestReceivedEventArgs args);

        /// <summary>
        ///     Reference to delegate to avoid garbage collection.
        /// </summary>
        private readonly EvtHcProcessAccessRequest _callback;

        /// <summary>
        ///     Native handle to HidCerberus sub-system.
        /// </summary>
        private readonly IntPtr _hcHandle;

        /// <summary>
        ///     Loads native library and registers callback for handling access requests.
        /// </summary>
        public HidCerberusWrapper()
        {
            LoadNativeLibrary("HidCerberus", @"x86\HidCerberus.dll", @"x64\HidCerberus.dll");

            _hcHandle = hc_init();

            if (_hcHandle == IntPtr.Zero)
                throw new HidCerberusInitializationFailedException("Failed to initialize the HidCerberus subsystem.");

            _callback = ProcessAccessRequest;
            hc_register_access_request_event(_hcHandle, _callback);
        }

        /// <summary>
        ///     Gets called when an access request is incoming from the driver.
        /// </summary>
        public event AccessRequestReceivedEventHandler AccessRequestReceived;

        /// <summary>
        ///     Gets called whenever a pending access request requires processing.
        /// </summary>
        /// <param name="handle">The native handle to request context.</param>
        /// <param name="hardwareIds">The list of one or more hardware IDs.</param>
        /// <param name="hardwareIdsCount">The count of hardware IDs in the list.</param>
        /// <param name="deviceId">The device identifier.</param>
        /// <param name="instanceId">The instance identifier.</param>
        /// <param name="processId">The ID of the process generating the access request.</param>
        /// <returns>True if the request will be handled, false otherwise.</returns>
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

            // Returning false will free the context memory
            return args.AccessRequest.IsHandled;
        }

        /// <summary>
        ///     Private implementation of <see cref="IAccessRequest"/>.
        /// </summary>
        private class AccessRequest : IAccessRequest
        {
            private AccessRequest()
            {
                RequestId = Guid.NewGuid();
            }

            [JsonIgnore] private IntPtr NativeHandle { get; set; }

            public Guid RequestId { get; }

            /// <summary>
            ///     The list of one or more hardware IDs.
            /// </summary>
            public string[] HardwareIds { get; private set; }

            /// <summary>
            ///     The device identifier.
            /// </summary>
            public string DeviceId { get; private set; }

            /// <summary>
            ///     The instance identifier.
            /// </summary>
            public string InstanceId { get; private set; }

            /// <summary>
            ///     The ID of the process generating the access request.
            /// </summary>
            public uint ProcessId { get; private set; }

            /// <summary>
            ///     True if the host handles the request. False will result in a default action.
            /// </summary>
            [JsonIgnore] public bool IsHandled { get; set; }

            /// <summary>
            ///     Submit the result of the access request decision back to the driver.
            /// </summary>
            /// <param name="isAllowed">True if access shall be granted, false otherwise.</param>
            /// <param name="isPermanent">True if the driver should remember this decision for the lifetime of the affected device.</param>
            public void SubmitResult(bool isAllowed, bool isPermanent)
            {
                // This call will complete the request and free the context memory
                hc_submit_access_request_result(NativeHandle, isAllowed, isPermanent);
            }

            /// <summary>
            ///     <see cref="AccessRequest" /> object factory.
            /// </summary>
            /// <param name="handle">The native context handle.</param>
            /// <param name="hardwareIds">The list of one or more hardware IDs.</param>
            /// <param name="deviceId">The device identifier.</param>
            /// <param name="instanceId">The instance identifier.</param>
            /// <param name="processId">The ID of the process generating the access request.</param>
            /// <returns>An <see cref="AccessRequest" /> object.</returns>
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