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

            // Set up WebSocket server
            _server = new WebSocketServer(Config.Global.WebSocket.Server.Location);

            // Start listening for connections
            _server.Start(connection =>
            {
                connection.OnOpen = () =>
                {
                    Log.Information("Client {RemoteAddress}:{RemotePort} connected ({Origin})",
                        connection.ConnectionInfo.ClientIpAddress,
                        connection.ConnectionInfo.ClientPort,
                        connection.ConnectionInfo.Origin);

                    try
                    {
                        // Try to boot up HidCerberus sub-system
                        _hcSystem = new HidCerberusWrapper();
                    }
                    catch (Exception ex)
                    {
                        Log.Fatal("Fatal error: {Exception}", ex);
                        connection.Send(JsonConvert.SerializeObject("Internal server error, can't connect, sorry =("));
                        connection.Close();
                        return;
                    }

                    // Listen for access requests from HidCerberus
                    _hcSystem.AccessRequestReceived += (sender, args) =>
                    {
                        // Timeout defines how long the request will stay on halt
                        var timeout = TimeSpan.FromMilliseconds(Config.Global.HidGuardian.Timeout);

                        // Message object getting sent to the client
                        var obj = new AccessRequest
                        {
                            HardwareId = args.HardwareId,
                            DeviceId = args.DeviceId,
                            InstanceId = args.InstanceId,
                            ProcessId = args.ProcessId,
                            ExpiresOn = DateTime.Now.Add(timeout)
                        };

                        // Enqueue object for later completion
                        _requestQueue.Add(obj.RequestId, obj);

                        // Send request to client
                        connection.Send(JsonConvert.SerializeObject(obj));

                        // Wait until response comes back in
                        if (obj.Signal.WaitOne(timeout))
                        {
                            args.IsHandled = obj.IsHandled;
                            args.IsAllowed = obj.IsAllowed;
                            args.IsPermanent = obj.IsPermanent;
                        }

                        // Pop from queue
                        _requestQueue.Remove(obj.RequestId);
                    };
                };

                // Clean-up on disconnect
                connection.OnClose = () =>
                {
                    Log.Information("Connection closed");

                    _hcSystem.Dispose();
                    _hcSystem = null;
                };

                // Log error
                connection.OnError = exception => Log.Error("Unexpected error: {Exception}", exception);

                // Handle incoming message
                connection.OnMessage = message =>
                {
                    var obj = JsonConvert.DeserializeObject<AccessRequest>(message);

                    // Grab pending request (if still in queue)
                    var request = _requestQueue.Where(r => r.Key == obj.RequestId)
                        .Select(r => (KeyValuePair<Guid, AccessRequest>?) r)
                        .FirstOrDefault();

                    // Request timed out, abort
                    if (request == null) return;

                    request.Value.Value.IsHandled = obj.IsHandled;
                    request.Value.Value.IsAllowed = obj.IsAllowed;
                    request.Value.Value.IsPermanent = obj.IsPermanent;

                    // Fire event to complete this request
                    request.Value.Value.Signal.Set();
                };
            });
        }

        public void Stop()
        {
            Log.Information("Service stopping");

            // Complete all pending requests
            foreach (var request in _requestQueue)
            {
                request.Value.Signal.Set();
            }

            _requestQueue.Clear();

            try
            {
                _server.Dispose();
                _hcSystem?.Dispose();
            }
            catch (Exception ex)
            {
                Log.Fatal("Fatal error: {Exception}", ex);
            }
        }
    }
}