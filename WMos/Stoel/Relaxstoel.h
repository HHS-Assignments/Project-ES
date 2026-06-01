#pragma once
#include <Arduino.h>

class Motor {
public:
    Motor(uint8_t pin);
    void begin();
    void setSpeed(uint8_t pct);

private:
    uint8_t _pin;
};