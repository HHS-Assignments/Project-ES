#pragma once
#include <string>

// Generieke communicatie-interface (lijn-gebaseerd, line-delimited JSON).
// Gespiegeld aan de Communication interface op de Wemos.
class Communication {
public:
    virtual bool begin()                       = 0;
    virtual bool send(const std::string &line) = 0;
    virtual bool receive(std::string &line)    = 0;
    virtual bool isConnected()                 = 0;
    virtual void stop()                        = 0;
    virtual ~Communication() = default;
};
