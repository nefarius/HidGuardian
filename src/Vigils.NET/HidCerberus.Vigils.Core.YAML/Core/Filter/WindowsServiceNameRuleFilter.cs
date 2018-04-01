namespace HidCerberus.Vigils.Core.YAML.Core.Filter
{
    public class WindowsServiceNameRuleFilter : RuleFilter
    {
        public WindowsServiceNameRuleFilter(string value) : base(value)
        {
        }

        public override bool Validate(int processId)
        {
            throw new System.NotImplementedException();
        }
    }
}