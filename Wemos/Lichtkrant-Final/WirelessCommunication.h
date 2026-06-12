#pragma once
#include "Communication.h"

class WirelessCommunication : public Communication {
public:
    virtual bool connect(const char *ssid, const char *pass) = 0;
    virtual bool isConnected()                               = 0;
    virtual ~WirelessCommunication() {}
};
