using System;
using Nancy.Hosting.Self;
using Serilog;

namespace HidVigil.Core.Service
{
    public class HidVigilService
    {
        private NancyHost _nancyHost;

        public void Start()
        {
            Log.Information("Service starting");

            _nancyHost = new NancyHost(new Uri("http://localhost:35122"));
            _nancyHost.Start();
        }

        public void Stop()
        {
            Log.Information("Service stopping");

            _nancyHost.Stop();
        }
    }
}