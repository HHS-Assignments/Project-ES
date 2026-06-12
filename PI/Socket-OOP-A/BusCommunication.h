#pragma once
#include <cstdint>
#include "Communication.h"

// Eén bericht op de bus (CAN: 11-bit ID + max 8 databytes).
struct CanMessage {
    uint32_t id      = 0;
    uint8_t  data[8] = {};
    uint8_t  len     = 0;
};

// Bus-interface: voegt frame-gebaseerde IO toe aan Communication.
class BusCommunication : public Communication {
public:
    virtual bool readMessage(CanMessage &msg)        = 0;
    virtual bool writeMessage(const CanMessage &msg) = 0;

    // Lijn-gebaseerde IO is niet van toepassing op een bus;
    // de gateway gebruikt readMessage/writeMessage.
    bool send(const std::string &) override    { return false; }
    bool receive(std::string &) override       { return false; }
};
