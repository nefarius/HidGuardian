using System;
using System.Diagnostics;

namespace HidCerberus.Vigils.Core.YAML.Core.Filter
{
    public class ProcessImagePathRuleFilter : RuleFilter
    {
        public ProcessImagePathRuleFilter(string value) : base(value)
        {
        }

        public override bool Validate(int processId)
        {
            return Value.Equals(Process.GetProcessById(processId).MainModule.FileName,
                StringComparison.InvariantCultureIgnoreCase);
        }
    }
}