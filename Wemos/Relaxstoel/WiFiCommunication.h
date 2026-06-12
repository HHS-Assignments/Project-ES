#pragma once
#include <ESP8266WiFi.h>
#include "WirelessCommunication.h"

class WiFiCommunication : public WirelessCommunication {
public:
    WiFiCommunication(const char *piHost, int piPort, int receivePort);

    void begin()                                        override;
    bool connect(const char *ssid, const char *pass)   override;
    void reconnect()                                    override;
    bool isConnected()                                  override;
    bool send(const char *data)                         override;
    bool receive(char *buf, size_t len)                 override;

    bool sendCanJson(const char *canId, int data);

private:
    const char *_piHost;
    int         _piPort;
    WiFiServer  _server;
    const char *_ssid             = nullptr;
    const char *_pass             = nullptr;
    unsigned long _laatsHerverbind = 0;

    bool _readClientLine(WiFiClient *client, char *buf, size_t len);
};
