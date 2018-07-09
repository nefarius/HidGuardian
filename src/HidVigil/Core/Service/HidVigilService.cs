using System.Text;
using HidVigil.Core.WebHooks;
using Serilog;
using WebSocketSharp;
using WebSocketSharp.Server;
using JsonConfig;
namespace HidVigil.Core.Service
{
    public class HidVigilService
    {
        private HttpServer _server;

        public void Start()
        {
            Log.Information("Service starting");

            _server = new HttpServer(Config.Global.HttpServer.ListenUrl);

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