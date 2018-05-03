using Nancy;
using Nancy.TinyIoc;

namespace HidCerberus.Vigils.Core.Webhooks.Core
{
    public class WebhooksBootstrapper : DefaultNancyBootstrapper
    {
        private readonly IAppConfiguration appConfig;

        public WebhooksBootstrapper()
        {
        }

        public WebhooksBootstrapper(IAppConfiguration appConfig)
        {
            this.appConfig = appConfig;
        }

        protected override void ConfigureApplicationContainer(TinyIoCContainer container)
        {
            base.ConfigureApplicationContainer(container);

            container.Register<IAppConfiguration>(appConfig);
        }
    }
}