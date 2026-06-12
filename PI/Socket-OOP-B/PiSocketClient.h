#pragma once
#include <cstdint>
#include <mutex>
#include "Communication.h"

// TCP-client naar Pi A, met automatische reconnect (zoals piALink in de oude code).
class PiSocketClient : public Communication {
public:
    PiSocketClient(const std::string &host, uint16_t port);

    bool begin() override;                       // geen directe verbinding; receive() verbindt
    bool send(const std::string &line) override;
    bool receive(std::string &line) override;    // blokkeert; herverbindt bij verbroken link
    bool isConnected() override;
    void stop() override;

private:
    std::string _host;
    uint16_t    _port;
    int         _fd = -1;
    std::mutex  _mu;          // beschermt _fd bij gelijktijdig zenden/sluiten
    std::string _acc;         // ontvangstbuffer voor onvolledige regels
    bool        _stopped = false;

    bool _connect();
    void _drop();
};
