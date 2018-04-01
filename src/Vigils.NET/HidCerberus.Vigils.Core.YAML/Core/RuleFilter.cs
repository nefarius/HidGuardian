namespace HidCerberus.Vigils.Core.YAML.Core
{
    public abstract class RuleFilter
    {
        public string Value { get; }

        protected RuleFilter(string value)
        {
            Value = value;
        }

        public abstract bool Validate(int processId);
    }
}