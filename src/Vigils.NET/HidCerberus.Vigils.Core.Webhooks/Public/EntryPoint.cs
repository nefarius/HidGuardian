using System.IO;
using System.Reflection;
using Serilog;

namespace HidCerberus.Vigils.Core.Webhooks.Public
{
    public class EntryPoint
    {
        static EntryPoint()
        {
            var assemblyFolder = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);

            Log.Logger = new LoggerConfiguration()
                .MinimumLevel.Information()
                .WriteTo.RollingFile(Path.Combine(assemblyFolder, "HidCerberus.Vigils.Core.Webhooks-{Date}.log"))
                .CreateLogger();
        }

        public static bool ProcessAccessRequest(
            string hardwareId,
            string deviceId,
            string instanceId,
            uint processId,
            out bool isAllowed,
            out bool isPermanent
        )
        {
            isAllowed = isPermanent = false;

            return false;
        }
    }
}