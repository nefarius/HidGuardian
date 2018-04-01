using System;
using System.Diagnostics;

namespace HidCerberus.Vigils.Core.YAML.Core.Filter
{
    public class ProcessNameRuleFilter : RuleFilter
    {
        public ProcessNameRuleFilter(string value) : base(value)
        {
        }

        public override bool Validate(int processId)
        {
            return Value.Equals(Process.GetProcessById(processId).ProcessName,
                StringComparison.InvariantCultureIgnoreCase);
        }
    }
}