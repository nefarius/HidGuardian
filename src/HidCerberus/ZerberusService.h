#pragma once

#define POCO_NO_UNWINDOWS
#include <Poco/Util/ServerApplication.h>
#include <Poco/SharedPtr.h>
#include <Poco/Data/Session.h>

using Poco::SharedPtr;
using Poco::Data::Session;
using namespace Poco::Util;

class ZerberusService :
    public Poco::Util::ServerApplication
{
    SharedPtr<Session> _session;

    void enumerateDeviceInterface(const std::string& name, const std::string& value);
    void help(const std::string& name, const std::string& value);
    void displayHelp() const;
    void onDeviceArrived(const void* pSender, std::string& name);

protected:
    void initialize(Application& self);
    int main(const std::vector<std::string>& args);
    void defineOptions(OptionSet& options);
    void handleOption(const std::string& name, const std::string& value);
public:
    ZerberusService() {};
    ~ZerberusService() {};
};

