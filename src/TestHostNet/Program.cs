using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace TestHostNet
{
    class Program
    {
        static void Main(string[] args)
        {
            var handle = hc_init();

            hc_register_access_request_event(handle, ProcessAccessRequest);

            Console.ReadKey();

            hc_shutdown(handle);
        }

        static bool ProcessAccessRequest(
            string HardwareId,
            string DeviceId,
            string InstanceId,
            int ProcessId,
            out bool IsAllowed,
            out bool IsPermananet
            )
        {
            Console.WriteLine(HardwareId);

            IsAllowed = IsPermananet = true;

            return true;
        }

        private delegate bool EVT_HC_PROCESS_ACCESS_REQUEST(
            string HardwareId,
            string DeviceId,
            string InstanceId,
            int ProcessId,
            out bool IsAllowed,
            out bool IsPermananet
            );

        [DllImport("HidCerberus.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern IntPtr hc_init();

        [DllImport("HidCerberus.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern void hc_shutdown(IntPtr handle);

        [DllImport("HidCerberus.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern void hc_register_access_request_event(IntPtr handle, EVT_HC_PROCESS_ACCESS_REQUEST callback);
    }
}
