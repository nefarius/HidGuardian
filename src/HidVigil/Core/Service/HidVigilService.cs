using Serilog;

namespace HidVigil.Core.Service
{
    public class HidVigilService
    {
        public void Start()
        {
            Log.Information("Service starting");
        }

        public void Stop()
        {
            Log.Information("Service stopping");
        }
    }
}