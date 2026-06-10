#pragma once
#include <Arduino.h>
#include "Motor.h"

class Vibratiemotor : public Motor {
public:
    Vibratiemotor(uint8_t pin) : _pin(pin) {}

    void begin() override {
        // ESP32: LEDC PWM koppelen aan pin (1 kHz, 10-bit resolutie → 0-1023)
        ledcAttach(_pin, 1000, 10);
    }

    void setSpeed(uint8_t pct) override {
        if (pct > 100) pct = 100;
        uint16_t duty = (pct == 0) ? 0 : (pct * 1023u) / 100u;
        ledcWrite(_pin, duty);
    }

private:
    uint8_t _pin;
};
