using System;
using System.IO;
using System.Linq;
using HidCerberus.Vigils.Core.YAML.Core;
using HidCerberus.Vigils.Core.YAML.Core.YAML;
using YamlDotNet.Serialization;
using YamlDotNet.Serialization.NamingConventions;

namespace HidCerberus.Vigils.Core.YAML.Public
{
    public class EntryPoint
    {
        private static Document Config { get; set; }

        static EntryPoint()
        {
            using (TextReader reader = File.OpenText(@"rules.yaml"))
            {
                var deserializer = new DeserializerBuilder()
                    .WithNamingConvention(new CamelCaseNamingConvention())
                    .WithTypeConverter(new RuleFilterConverter())
                    .Build();

                Config = deserializer.Deserialize<Document>(reader);
            }
        }

        public static bool ProcessAccessRequest(string hardwareId, int processId, out bool isAllowed, out bool isPermanent)
        {
            var match = Config.Rules.FirstOrDefault(r =>
                r.HardwareId.Equals(hardwareId, StringComparison.InvariantCultureIgnoreCase));

            if (match != null)
            {
                match.Filter.Validate(processId);

                isAllowed = match.IsAllowed;
                isPermanent = match.IsPermanent;

                return true;
            }

            isAllowed = false;
            isPermanent = false;

            return false;
        }
    }
}