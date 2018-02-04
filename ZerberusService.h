#pragma once

#define POCO_NO_UNWINDOWS
#include <Poco/Util/ServerApplication.h>
#include <Poco/Util/Option.h>
#include <Poco/Util/OptionSet.h>

using namespace Poco::Util;

class ZerberusService :
    public Poco::Util::ServerApplication
{
private:
    std::string _logfile;
    int _threadCount = 20;
protected:
    int main(const std::vector<std::string>& args);
    void defineOptions(OptionSet& options);
    void handleOption(const std::string& name, const std::string& value);
public:
    ZerberusService() {};
    ~ZerberusService() {};
};

