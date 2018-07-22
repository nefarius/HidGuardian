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
            IntPtr Handle,
            string[] HardwareIds,
            int HardwareIdsCount,
            string DeviceId,
            string InstanceId,
            int ProcessId
            )
        {
            foreach(var id in HardwareIds)
            {
                Console.WriteLine(id);
            }

            return true;
        }

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate bool EVT_HC_PROCESS_ACCESS_REQUEST(
            IntPtr Handle,
            [MarshalAs(UnmanagedType.LPArray, ArraySubType = UnmanagedType.LPStr, SizeParamIndex = 2)]
            string[] HardwareIds,
            int HardwareIdsCount,
            string DeviceId,
            string InstanceId,
            int ProcessId
            );

        [DllImport("HidCerberus.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern IntPtr hc_init();

        [DllImport("HidCerberus.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern void hc_shutdown(IntPtr handle);

        [DllImport("HidCerberus.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern void hc_register_access_request_event(IntPtr handle, EVT_HC_PROCESS_ACCESS_REQUEST callback);
    }
}
