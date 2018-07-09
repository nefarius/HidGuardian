using System;
using System.Collections.Generic;
using System.Linq;
using HidVigil.Core.HidCerberus;
using HidVigil.Core.Types;
using JetBrains.Annotations;
using Newtonsoft.Json;
using WebSocketSharp;
using WebSocketSharp.Server;
using JsonConfig;

namespace HidVigil.Core.WebHooks
{
    public class HidGuardianInterface : WebSocketBehavior
    {
        private readonly HidCerberusWrapper _hcSystem;

        private readonly Dictionary<Guid, AccessRequest> _requestQueue =
            new Dictionary<Guid, AccessRequest>();

        [UsedImplicitly]
        public HidGuardianInterface()
        {
            _hcSystem = new HidCerberusWrapper();
            _hcSystem.AccessRequestReceived += ProcessAccessRequestReceived;
        }

        ~HidGuardianInterface()
        {
            _hcSystem.Dispose();
        }

        private void ProcessAccessRequestReceived(object sender, AccessRequestReceivedEventArgs args)
        {
            var obj = new AccessRequest
            {
                HardwareId = args.HardwareId,
                DeviceId = args.DeviceId,
                InstanceId = args.InstanceId,
                ProcessId = args.ProcessId
            };

            _requestQueue.Add(obj.RequestId, obj);

            Send(JsonConvert.SerializeObject(obj));

            if (obj.Signal.WaitOne(TimeSpan.FromMilliseconds(Config.Global.HidGuardian.Timeout)))
            {
                args.IsHandled = obj.IsHandled;
                args.IsAllowed = obj.IsAllowed;
                args.IsPermanent = obj.IsPermanent;
            }

            _requestQueue.Remove(obj.RequestId);
        }

        protected override void OnOpen()
        {
            Serilog.Log.Information("WebSocket connection established");
            base.OnOpen();
        }

        protected override void OnClose(CloseEventArgs e)
        {
            Serilog.Log.Information("WebSocket connection closed");
            base.OnClose(e);
        }

        protected override void OnMessage(MessageEventArgs e)
        {
            var obj = JsonConvert.DeserializeObject<AccessRequest>(e.Data);

            var request = _requestQueue.Where(r => r.Key == obj.RequestId)
                .Select(r => (KeyValuePair<Guid, AccessRequest>?) r)
                .FirstOrDefault();

            if (request == null)
            {
                return;
            }

            request.Value.Value.IsHandled = obj.IsHandled;
            request.Value.Value.IsAllowed = obj.IsAllowed;
            request.Value.Value.IsPermanent = obj.IsPermanent;

            request.Value.Value.Signal.Set();
        }
    }
}