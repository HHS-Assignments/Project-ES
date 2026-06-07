#pragma once
#include <stdint.h>

class Motor {
public:
    virtual void begin()               = 0;
    virtual void setSpeed(uint8_t pct) = 0;
    virtual ~Motor() {}
};
