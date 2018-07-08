using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using HidVigil.Core.HidCerberus;

namespace HidVigil
{
    class Program
    {
        static void Main(string[] args)
        {
            using (var hcw = new HidCerberusWrapper())
            {
                hcw.AccessRequestReceived += HcwOnAccessRequestReceived;
                Console.ReadKey();
            }
        }

        private static void HcwOnAccessRequestReceived(object sender, AccessRequestReceivedEventArgs args)
        {
            Console.WriteLine($"HWID: {args.HardwareId}, PID: {args.ProcessId}");
            
            args.IsHandled = true;
            args.IsAllowed = true;
        }
    }
}
