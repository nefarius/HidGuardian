﻿using System;
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

        private delegate bool EvtHcProcessAccessRequest(
            string hardwareId,
            string deviceId,
            string instanceId,
            uint processId,
            out bool isAllowed,
            out bool isPermananet
        );
    }
}