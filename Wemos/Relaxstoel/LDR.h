#pragma once
#include <Arduino.h>
#include "Lichtsensor.h"

class LDR : public Lichtsensor {
public:
    LDR(uint8_t pin) : _pin(pin) {}

    void begin()     override { /* analogRead heeft geen pinMode nodig op ESP8266 */ }
    int  getWaarde() override { return analogRead(_pin); }

private:
    uint8_t _pin;
};
