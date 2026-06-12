#pragma once
#include <cstdint>
#include <mutex>
#include "Communication.h"

// TCP-server voor één peer (Pi B). Accepteert opnieuw na een disconnect.
class PiSocketServer : public Communication {
public:
    explicit PiSocketServer(uint16_t port);

    bool begin() override;
    bool send(const std::string &line) override;
    bool receive(std::string &line) override;   // blokkeert; accepteert peer indien nodig
    bool isConnected() override;
    void stop() override;

private:
    uint16_t    _port;
    int         _srvFd  = -1;
    int         _peerFd = -1;
    std::mutex  _mu;          // beschermt _peerFd bij gelijktijdig zenden/sluiten
    std::string _acc;         // ontvangstbuffer voor onvolledige regels

    bool _accept();
    void _dropPeer();
};
