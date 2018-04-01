namespace HidCerberus.Vigils.Core.YAML.Core.Filter
{
    public class ProcessImagePathRuleFilter : RuleFilter
    {
        public ProcessImagePathRuleFilter(string value) : base(value)
        {
        }

        public override bool Validate(int processId)
        {
            throw new System.NotImplementedException();
        }
    }
}