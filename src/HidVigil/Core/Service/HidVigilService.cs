using System.Net;
using System.Threading;
using System.Threading.Tasks;
using HidVigil.Core.Types;
using HidVigil.Core.WebHooks;
using Newtonsoft.Json;
using Serilog;
using WebSocketSharp;
using WebSocketSharp.Server;

namespace HidVigil.Core.Service
{
    public class HidVigilService
    {
        private HttpServer _server;

        public void Start()
        {
            Log.Information("Service starting");

            _server = new HttpServer(IPAddress.Loopback, 22408);
            _server.AddWebSocketService<HidGuardianInterface>("/HidGuardian");
            _server.Start();
        }

        public void Stop()
        {
            Log.Information("Service stopping");

            _server.Stop(CloseStatusCode.Normal, "Server shutting down");
        }
    }
}