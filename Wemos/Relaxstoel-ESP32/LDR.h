#pragma once
#include <Arduino.h>
#include "Lichtsensor.h"

class LDR : public Lichtsensor {
public:
    LDR(uint8_t pin) : _pin(pin) {}

    void begin()     override {
        // ESP32: standaard 12-bit (0-4095), zetten naar 10-bit (0-1023) voor compatibiliteit
        analogReadResolution(10);
    }
    int  getWaarde() override { return analogRead(_pin); }

private:
    uint8_t _pin;
};
