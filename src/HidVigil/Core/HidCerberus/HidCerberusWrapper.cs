using HidVigil.Core.Util.Native;

namespace HidVigil.Core.HidCerberus
{
    public partial class HidCerberusWrapper : NativeLibraryWrapper<HidCerberusWrapper>
    {
        public HidCerberusWrapper()
        {
            LoadNativeLibrary("HidCerberus", @"x86\HidCerberus.dll", @"x64\HidCerberus.dll");
        }
    }
}