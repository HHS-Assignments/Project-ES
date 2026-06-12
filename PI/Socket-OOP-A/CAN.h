#pragma once
#include "BusCommunication.h"

// SocketCAN-implementatie van BusCommunication (bijv. can0 via MCP2515).
class CAN : public BusCommunication {
public:
    explicit CAN(const char *iface = "can0");

    bool begin() override;
    bool isConnected() override;
    void stop() override;

    bool readMessage(CanMessage &msg) override;
    bool writeMessage(const CanMessage &msg) override;

private:
    std::string _iface;
    int         _fd = -1;
};
