#pragma once
#include <stddef.h>

class Communication {
public:
    virtual void begin()                                      = 0;
    virtual bool send(const char *data)                       = 0;
    virtual bool receive(char *buf, size_t len)               = 0;
    virtual bool sendCanJson(const char *canId, int data)     = 0;
    virtual bool isConnected()                                = 0;
    virtual bool connect(const char *ssid, const char *pass)  = 0;
    virtual void reconnect() {}
    virtual ~Communication() {}
};
