namespace HidCerberus.Vigils.Core.YAML.Core
{
    public class Rule
    {
        public string Name { get; set; }

        public string Description { get; set; }

        public string HardwareId { get; set; }

        public bool IsAllowed { get; set; }

        public bool IsPermanent { get; set; }

        public RuleFilter Filter { get; set; }
    }
}