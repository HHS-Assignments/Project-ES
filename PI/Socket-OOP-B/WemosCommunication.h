#pragma once
#include <cstdint>
#include <vector>
#include "Communication.h"

// Eén Wemos-doel (naam, IP, poort waarop zijn TCP-server luistert).
struct WemosTarget {
    std::string name;
    std::string ip;
    uint16_t    port;
};

// Communicatie met de Wemos-apparaten:
// - send():    fan-out van één regel naar ALLE bekende Wemos'es (korte TCP-verbinding per stuk,
//              gespiegeld aan hoe de Wemos zelf zendt)
// - receive(): TCP-server; accepteert een inkomende Wemos-verbinding, leest één regel en ACK't
class WemosCommunication : public Communication {
public:
    WemosCommunication(uint16_t receivePort, std::vector<WemosTarget> targets);

    bool begin() override;
    bool send(const std::string &line) override;
    bool receive(std::string &line) override;   // blokkeert tot een Wemos iets stuurt
    bool isConnected() override;
    void stop() override;

private:
    uint16_t                 _receivePort;
    std::vector<WemosTarget> _targets;
    int                      _srvFd = -1;
};
