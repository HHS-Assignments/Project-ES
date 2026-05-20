#include "Relaxstoel.h"

const uint16_t MIN_PULSE = 0;
const uint16_t MAX_PULSE = 1000;

Motor::Motor(uint8_t pin) : _pin(pin) {}

void Motor::begin() {
    pinMode(_pin, OUTPUT);
}

void Motor::setSpeed(uint8_t pct) {
    if (pct > 100) pct = 100;

    uint16_t pulse;
    if (pct == 0) {
        pulse = 0;
    } else {
        pulse = MIN_PULSE + ((pct * (MAX_PULSE - MIN_PULSE)) / 100);
    }

    analogWrite(_pin, pulse);
}