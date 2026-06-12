#pragma once
#include <Arduino.h>
#include "Motor.h"

class Vibratiemotor : public Motor {
public:
    Vibratiemotor(uint8_t pin) : _pin(pin) {}

    void begin() override {
        pinMode(_pin, OUTPUT);
        // Lagere PWM-frequentie: ESP8266 software-PWM op 1 kHz
        // steelt interrupt-tijd van de WiFi-stack
        analogWriteFreq(200);
    }

    void setSpeed(uint8_t pct) override {
        if (pct > 100) pct = 100;
        uint16_t pulse = (pct == 0) ? 0 : (pct * 1000u) / 100u;
        analogWrite(_pin, pulse);
    }

private:
    uint8_t _pin;
};
