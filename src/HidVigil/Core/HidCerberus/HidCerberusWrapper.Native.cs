using System;
using System.Runtime.InteropServices;

namespace HidVigil.Core.HidCerberus
{
    public partial class HidCerberusWrapper
    {
        [DllImport("HidCerberus.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern IntPtr hc_init();

        [DllImport("HidCerberus.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern void hc_shutdown(IntPtr handle);

        [DllImport("HidCerberus.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern void hc_register_access_request_event(IntPtr handle, EvtHcProcessAccessRequest callback);

        [DllImport("HidCerberus.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern void hc_submit_access_request_result(IntPtr handle, bool isAllowed, bool isPermanent);

        private delegate bool EvtHcProcessAccessRequest(
            IntPtr Handle,
            [MarshalAs(UnmanagedType.LPArray, ArraySubType = UnmanagedType.LPStr, SizeParamIndex = 2)]
            string[] HardwareIds,
            int HardwareIdsCount,
            string DeviceId,
            string InstanceId,
            uint ProcessId
        );
    }
}