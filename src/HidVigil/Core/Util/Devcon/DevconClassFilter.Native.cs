using System;
using System.Runtime.InteropServices;

namespace HidVigil.Core.Util.Devcon
{
    public partial class DevconClassFilter
    {
        [Flags]
        private enum REGASM : UInt32
        {
            KEY_READ = 0x20019,
            KEY_WRITE = 0x20006
        }

        private const UInt32 DIOCR_INSTALLER = 0x00000001;

        private const UInt32 DIOCR_INTERFACE = 0x00000002;

        [DllImport("setupapi.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern IntPtr SetupDiOpenClassRegKeyEx(
            ref Guid ClassGuid, REGASM samDesired, UInt32 Flags, string MachineName, IntPtr Reserved);

        [DllImport("setupapi.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern bool SetupDiClassGuidsFromNameEx(
            string ClassName, IntPtr ClassGuidList, UInt32 ClassGuidListSize, out UInt32 RequiredSize,
            string MachineName, IntPtr Reserved);
    }
}