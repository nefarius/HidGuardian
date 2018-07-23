using System;
using System.Collections.Generic;
using System.Linq;
using Fleck;
using HidVigil.Core.HidCerberus;
using HidVigil.Core.Types;
using JsonConfig;
using Newtonsoft.Json;
using Serilog;

namespace HidVigil.Core.Service
{
    public class HidVigilService
    {
        private readonly SynchronizedCollection<IWebSocketConnection> _currentConnections =
            new SynchronizedCollection<IWebSocketConnection>();

        private readonly Dictionary<Guid, IAccessRequest> _requestQueue =
            new Dictionary<Guid, IAccessRequest>();

        private HidCerberusWrapper _hcSystem;
        private WebSocketServer _server;

        public HidVigilService()
        {
            // Redirect logging to Serilog
            FleckLog.LogAction = (level, message, ex) =>
            {
                switch (level)
                {
                    case LogLevel.Debug:
                        Log.Debug(message, ex);
                        break;
                    case LogLevel.Error:
                        Log.Error(message, ex);
                        break;
                    case LogLevel.Warn:
                        Log.Warning(message, ex);
                        break;
                    default:
                        Log.Information(message, ex);
                        break;
                }
            };
        }

        public void Start()
        {
            Log.Information("Service starting");

            try
            {
                // Try to boot up HidCerberus sub-system
                _hcSystem = new HidCerberusWrapper();
                _hcSystem.AccessRequestReceived += HcSystemOnAccessRequestReceived;
            }
            catch (Exception ex)
            {
                Log.Fatal("Fatal error: {Exception}", ex);
                Stop();
                return;
            }

            // Set up WebSocket server
            _server = new WebSocketServer(Config.Global.WebSocket.Server.Location) {RestartAfterListenError = true};

            // Start listening for connections
            _server.Start(connection =>
            {
                connection.OnOpen = () =>
                {
                    Log.Information("Client {RemoteAddress}:{RemotePort} connected ({Origin})",
                        connection.ConnectionInfo.ClientIpAddress,
                        connection.ConnectionInfo.ClientPort,
                        connection.ConnectionInfo.Origin);

                    _currentConnections.Add(connection);
                };

                // Clean-up on disconnect
                connection.OnClose = () =>
                {
                    Log.Information("Connection closed");

                    _currentConnections.Remove(connection);
                };

                // Log error
                connection.OnError = exception => Log.Error("Unexpected error: {Exception}", exception);

                // Handle incoming message
                connection.OnMessage = message =>
                {
                    var result = JsonConvert.DeserializeObject<AccessRequestResult>(message);

                    // Grab pending request (if still in queue)
                    var request = _requestQueue.Where(r => r.Key == result.RequestId)
                        .Select(r => (KeyValuePair<Guid, IAccessRequest>?)r)
                        .FirstOrDefault();

                    if (request == null)
                    {
                        Log.Warning("Request {Id} missing from queue", result.RequestId);
                        return;
                    }

                    request.Value.Value.SubmitResult(result.IsAllowed, result.IsPermanent);
                };
            });
        }

        private void HcSystemOnAccessRequestReceived(object sender, AccessRequestReceivedEventArgs args)
        {
            // Enqueue object for later completion
            _requestQueue.Add(args.AccessRequest.RequestId, args.AccessRequest);

            // Nothing to send without any clients
            if (_currentConnections.Count <= 0)
            {
                return;
            }

            // Broadcast request to client(s)
            _currentConnections.ToList().ForEach(c => c.Send(JsonConvert.SerializeObject(args.AccessRequest)));

            // Notify subsystem that the request is in progress
            args.AccessRequest.IsHandled = true;
        }

        public void Stop()
        {
            Log.Information("Service stopping");

            _currentConnections.ToList().ForEach(c => c.Close());
            
            _server.Dispose();
            _requestQueue.Clear();

            try
            {
                _hcSystem?.Dispose();
            }
            catch (Exception ex)
            {
                Log.Fatal("Fatal error: {Exception}", ex);
            }
        }
    }
}