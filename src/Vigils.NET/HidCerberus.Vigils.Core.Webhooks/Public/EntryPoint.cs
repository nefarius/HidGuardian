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

        /// <summary>
        ///     Gets called by HidCerberus when an access decision has to be made.
        /// </summary>
        /// <param name="hardwareId">The Hardware ID identifying </param>
        /// <param name="processId"></param>
        /// <param name="isAllowed"></param>
        /// <param name="isPermanent"></param>
        /// <returns></returns>
        public static bool ProcessAccessRequest(string hardwareId, uint processId, out bool isAllowed, out bool isPermanent)
        {
            isAllowed = false;
            isPermanent = false;

            return false;
        }
    }
}