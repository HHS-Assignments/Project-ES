#pragma once
#include "Display.h"
#include "WiFiCommunication.h"

class LichtkrantController {
public:
    LichtkrantController(Display *display, WiFiCommunication *comm);

    void begin();
    void update();

private:
    Display           *_display;
    WiFiCommunication *_comm;

    bool _noodModus;

    char _normaleTekst[128];
    char _noodTekst[128];
    char _normaalAssembly[128];
    char _noodAssembly[128];

    unsigned long _vorigeWifiCheck;

    void _setModus(bool nood);
    void _verwerkTekstDeel(const char *incoming, char *assembly, char *doelTekst, bool isNood);
    void _verwerkCommando(const char *incoming);
};
