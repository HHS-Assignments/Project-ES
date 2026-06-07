#pragma once

class Display {
public:
    virtual void begin()                      = 0;
    virtual void toonTekst(const char *tekst) = 0;
    virtual void update()                     = 0;
    virtual ~Display() {}
};
