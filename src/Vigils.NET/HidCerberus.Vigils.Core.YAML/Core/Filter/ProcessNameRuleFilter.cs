namespace HidCerberus.Vigils.Core.YAML.Core.Filter
{
    public class ProcessNameRuleFilter : RuleFilter
    {
        public ProcessNameRuleFilter(string value) : base(value)
        {
        }

        public override bool Validate(int processId)
        {
            throw new System.NotImplementedException();
        }
    }
}