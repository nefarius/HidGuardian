#pragma once

#define POCO_NO_UNWINDOWS
#include <Poco/Util/ServerApplication.h>

class ZerberusService : 
    public Poco::Util::ServerApplication
{
protected:
    int main(const std::vector<std::string>& args);
public:
    ZerberusService();
    ~ZerberusService();
};

