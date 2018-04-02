using System.Runtime.InteropServices;

namespace HidCerberus.Vigils.Core.YAML.Core.Filter
{
    public partial class WindowsServiceNameRuleFilter : RuleFilter
    {
        public WindowsServiceNameRuleFilter(string value) : base(value)
        {
        }

        public override bool Validate(int processId)
        {
            var control = OpenSCManager(null, null, (uint) ACCESS_MASK.GENERIC_READ);

            var service = OpenService(control, Value, (uint) SERVICE_ACCESS.SERVICE_QUERY_STATUS);

            var bufLen = Marshal.SizeOf<SERVICE_STATUS_PROCESS>();
            var svcProc = Marshal.AllocHGlobal(bufLen);

            if (!QueryServiceStatusEx(service, 0x0, svcProc, bufLen, out var _))
            {
                CloseServiceHandle(service);
                CloseServiceHandle(control);

                return false;
            }

            CloseServiceHandle(service);
            CloseServiceHandle(control);

            var proc = Marshal.PtrToStructure<SERVICE_STATUS_PROCESS>(svcProc);

            return proc.processID == processId;
        }
    }
}