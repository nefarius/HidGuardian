using System;
using HidCerberus.Vigils.Core.Webhooks.Public;

namespace CoreVigilTestHost
{
    class Program
    {
        static void Main(string[] args)
        {
            EntryPoint.ProcessAccessRequest(@"HID\VID_054C&PID_05C4", 1337, out var isAllowed1, out var isPermanent1);

            Console.WriteLine($"IsAllowed: {isAllowed1}");
            Console.WriteLine($"IsPermanent: {isPermanent1}");

            EntryPoint.ProcessAccessRequest(@"USB\VID_045E&PID_028E", 1227, out var isAllowed2, out var isPermanent2);

            Console.WriteLine($"IsAllowed: {isAllowed2}");
            Console.WriteLine($"IsPermanent: {isPermanent2}");
        }
    }
}
