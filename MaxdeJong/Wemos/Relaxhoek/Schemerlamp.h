#pragma once
 
#include <Arduino.h>
 
class Schemerlamp {
public:
    Schemerlamp(uint8_t ldrPin, uint8_t ledPin, int drempel = 500);
    void begin();
    void update();
    bool isAan();
    int getLdrWaarde();
 
private:
    uint8_t _ldrPin;
    uint8_t _ledPin;
    int _drempel;
    bool _lampAan;
    int _ldrWaarde;
};
 

