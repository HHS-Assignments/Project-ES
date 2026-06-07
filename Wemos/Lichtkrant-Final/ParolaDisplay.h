#pragma once
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include "Display.h"

class ParolaDisplay : public Display {
public:
    ParolaDisplay(MD_MAX72XX::moduleType_t hwType, uint8_t csPin, uint8_t numDevices);

    void begin()                      override;
    void toonTekst(const char *tekst) override;
    void update()                     override;

private:
    MD_Parola _parola;
    char      _huidigeTekst[128];
};
