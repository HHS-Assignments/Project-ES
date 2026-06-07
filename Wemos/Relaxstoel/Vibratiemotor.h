#pragma once
#include <Arduino.h>
#include "Motor.h"

class Vibratiemotor : public Motor {
public:
    Vibratiemotor(uint8_t pin) : _pin(pin) {}

    void begin() override {
        pinMode(_pin, OUTPUT);
    }

    void setSpeed(uint8_t pct) override {
        if (pct > 100) pct = 100;
        uint16_t pulse = (pct == 0) ? 0 : (pct * 1000u) / 100u;
        analogWrite(_pin, pulse);
    }

private:
    uint8_t _pin;
};
