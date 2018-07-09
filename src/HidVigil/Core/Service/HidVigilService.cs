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
        private readonly Dictionary<Guid, AccessRequest> _requestQueue =
            new Dictionary<Guid, AccessRequest>();

        private HidCerberusWrapper _hcSystem;
        private WebSocketServer _server;

        public void Start()
        {
            Log.Information("Service starting");

            //
            // Redirect logging to Serilog
            // 
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

            //
            // Set up WebSocket server
            // 
            _server = new WebSocketServer(Config.Global.WebSocket.Server.Location);

            //
            // Start listening for connections
            // 
            _server.Start(connection =>
            {
                connection.OnOpen = () =>
                {
                    Log.Information("Client {RemoteAddress}:{RemotePort} connected ({Origin})",
                        connection.ConnectionInfo.ClientIpAddress,
                        connection.ConnectionInfo.ClientPort,
                        connection.ConnectionInfo.Origin);

                    _hcSystem = new HidCerberusWrapper();
                    _hcSystem.AccessRequestReceived += (sender, args) =>
                    {
                        var timeout = TimeSpan.FromMilliseconds(Config.Global.HidGuardian.Timeout);

                        var obj = new AccessRequest
                        {
                            HardwareId = args.HardwareId,
                            DeviceId = args.DeviceId,
                            InstanceId = args.InstanceId,
                            ProcessId = args.ProcessId,
                            ExpiresOn = DateTime.Now.Add(timeout)
                        };

                        _requestQueue.Add(obj.RequestId, obj);

                        connection.Send(JsonConvert.SerializeObject(obj));

                        if (obj.Signal.WaitOne(timeout))
                        {
                            args.IsHandled = obj.IsHandled;
                            args.IsAllowed = obj.IsAllowed;
                            args.IsPermanent = obj.IsPermanent;
                        }

                        _requestQueue.Remove(obj.RequestId);
                    };
                };

                //
                // Clean-up on disconnect
                // 
                connection.OnClose = () =>
                {
                    Log.Information("Connection closed");

                    _hcSystem.Dispose();
                    _hcSystem = null;
                };

                //
                // Log error
                // 
                connection.OnError = exception => Log.Error("Unexpected error: {Exception}", exception);

                //
                // handle incoming message
                // 
                connection.OnMessage = message =>
                {
                    var obj = JsonConvert.DeserializeObject<AccessRequest>(message);

                    var request = _requestQueue.Where(r => r.Key == obj.RequestId)
                        .Select(r => (KeyValuePair<Guid, AccessRequest>?) r)
                        .FirstOrDefault();

                    if (request == null) return;

                    request.Value.Value.IsHandled = obj.IsHandled;
                    request.Value.Value.IsAllowed = obj.IsAllowed;
                    request.Value.Value.IsPermanent = obj.IsPermanent;

                    request.Value.Value.Signal.Set();
                };
            });
        }

        public void Stop()
        {
            Log.Information("Service stopping");

            _server.Dispose();
            _hcSystem?.Dispose();
        }
    }
}