using System;
using System.Collections.Generic;
using System.Linq;
using HidVigil.Core.HidCerberus;
using HidVigil.Core.Types;
using JetBrains.Annotations;
using Newtonsoft.Json;
using WebSocketSharp;
using WebSocketSharp.Server;

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

            if (obj.Signal.WaitOne(TimeSpan.FromSeconds(10)))
            {
                args.IsHandled = obj.IsHandled;
                args.IsAllowed = obj.IsAllowed;
                args.IsPermanent = obj.IsPermanent;
            }

            _requestQueue.Remove(obj.RequestId);
        }

        protected override void OnMessage(MessageEventArgs e)
        {
            var obj = JsonConvert.DeserializeObject<AccessRequest>(e.Data);

            var request = _requestQueue.First(r => r.Key == obj.RequestId);

            request.Value.IsHandled = obj.IsHandled;
            request.Value.IsAllowed = obj.IsAllowed;
            request.Value.IsPermanent = obj.IsPermanent;

            request.Value.Signal.Set();
        }
    }
}