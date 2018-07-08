using HidVigil.Core.Service;
using Nancy;
using Serilog;
using Topshelf;

namespace HidVigil
{
    class Program
    {
        static void Main(string[] args)
        {
            Log.Logger = new LoggerConfiguration()
                .MinimumLevel.Information()
                .WriteTo.Console()
                .WriteTo.RollingFile(@"Logs\HidVigil-{Date}.log")
                .CreateLogger();

            HostFactory.Run(x =>
            {
                StaticConfiguration.DisableErrorTraces = false;

                x.Service<HidVigilService>(s =>
                {
                    s.ConstructUsing(name => new HidVigilService());
                    s.WhenStarted(tc => tc.Start());
                    s.WhenStopped(tc => tc.Stop());
                });

                x.RunAsLocalSystem();
                x.SetDescription("HidCerberus Configuration Host for HidGuardian Filter Drivers");
                x.SetDisplayName("HidCerberus Service");
                x.SetServiceName("HidVigil");
            });
        }
    }
}
