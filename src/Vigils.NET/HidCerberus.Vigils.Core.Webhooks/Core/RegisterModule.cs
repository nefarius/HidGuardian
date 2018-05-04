using Nancy;

namespace HidCerberus.Vigils.Core.Webhooks.Core
{
    public class RegisterModule : NancyModule
    {
        public RegisterModule(IAppConfiguration appConfig) : base("/api/v1")
        {
            Get("/", args => "Hello from Nancy running on CoreCLR");
        }
    }
}