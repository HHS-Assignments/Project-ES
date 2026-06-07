#pragma once
#include <Arduino.h>
#include "Lamp.h"

class Schemerlamp : public Lamp {
public:
    Schemerlamp(uint8_t pin) : _pin(pin) {}

    void begin()  override { pinMode(_pin, OUTPUT); digitalWrite(_pin, LOW); }
    void setAan() override { digitalWrite(_pin, HIGH); }
    void setUit() override { digitalWrite(_pin, LOW); }

private:
    uint8_t _pin;
};
