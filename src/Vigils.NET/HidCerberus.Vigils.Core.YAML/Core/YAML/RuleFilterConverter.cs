using System;
using HidCerberus.Vigils.Core.YAML.Core.Filter;
using YamlDotNet.Core;
using YamlDotNet.Core.Events;
using YamlDotNet.Serialization;

namespace HidCerberus.Vigils.Core.YAML.Core.YAML
{
    public class RuleFilterConverter : IYamlTypeConverter
    {
        public bool Accepts(Type type)
        {
            return typeof(RuleFilter).IsAssignableFrom(type);
        }

        public object ReadYaml(IParser parser, Type type)
        {
            RuleFilter filter = null;

            parser.Expect<MappingStart>();
            while (parser.Allow<MappingEnd>() == null)
            {
                var filterType = parser.Expect<Scalar>().Value;
                var filterValue = parser.Expect<Scalar>().Value;

                switch (filterType)
                {
                    case "processName":
                        filter = new ProcessNameRuleFilter(filterValue);
                        break;
                    case "processPath":
                        filter = new ProcessImagePathRuleFilter(filterValue);
                        break;
                    case "serviceName":
                        filter = new WindowsServiceNameRuleFilter(filterValue);
                        break;
                }
            }

            return filter;
        }

        public void WriteYaml(IEmitter emitter, object value, Type type)
        {
            throw new NotImplementedException();
        }
    }
}