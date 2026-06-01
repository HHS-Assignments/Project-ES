#include "Schemerlamp.h"
 
Schemerlamp::Schemerlamp(uint8_t ldrPin, uint8_t ledPin, int drempel)
    : _ldrPin(ldrPin), _ledPin(ledPin), _drempel(drempel),
      _lampAan(false), _ldrWaarde(0) {}
 
void Schemerlamp::begin() {
    pinMode(_ledPin, OUTPUT);
    digitalWrite(_ledPin, LOW);
}
 
void Schemerlamp::update() {
    _ldrWaarde = analogRead(_ldrPin);
 
    if (_ldrWaarde > _drempel) {
        _lampAan = true;
        digitalWrite(_ledPin, HIGH);
    } else {
        _lampAan = false;
        digitalWrite(_ledPin, LOW);
    }
}
 
bool Schemerlamp::isAan() {
    return _lampAan;
}
 
int Schemerlamp::getLdrWaarde() {
    return _ldrWaarde;
}
