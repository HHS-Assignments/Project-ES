#include "ParolaDisplay.h"
#include <string.h>

ParolaDisplay::ParolaDisplay(MD_MAX72XX::moduleType_t hwType, uint8_t csPin, uint8_t numDevices)
    : _parola(hwType, csPin, numDevices) {
    _huidigeTekst[0] = '\0';
}

void ParolaDisplay::begin() {
    _parola.begin();
    _parola.setIntensity(5);
    _parola.setPause(0);
    _parola.displayClear();
}

void ParolaDisplay::toonTekst(const char *tekst) {
    strncpy(_huidigeTekst, tekst, sizeof(_huidigeTekst) - 1);
    _huidigeTekst[sizeof(_huidigeTekst) - 1] = '\0';
    _parola.displayClear();
    _parola.displayScroll(_huidigeTekst, PA_LEFT, PA_SCROLL_LEFT, 50);
}

void ParolaDisplay::update() {
    if (_parola.displayAnimate()) {
        _parola.displayReset();
    }
}
