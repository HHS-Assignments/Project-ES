#include "Relaxstoel.h"

Motor::Motor(uint8_t pin) : _pin(pin) {
    
}

void Motor::begin() {
    pinMode(_pin, OUTPUT);
}

void Motor::setSpeed(uint8_t pct) {
    analogWrite(_pin, map(pct, 0, 100, 0, 1023));
}