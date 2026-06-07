#pragma once
#include "Display.h"
#include "Communication.h"

class LichtkrantController {
public:
    // Geeft altijd dezelfde instantie terug
    static LichtkrantController& getInstance();

    // Dependency injection — aanroepen vóór begin()
    void init(Display *display, Communication *comm);

    void begin();
    void update();

    // Kopiëren en toewijzen verboden
    LichtkrantController(const LichtkrantController&)            = delete;
    LichtkrantController& operator=(const LichtkrantController&) = delete;

private:
    LichtkrantController() = default; // private: niemand kan new aanroepen

    Display       *_display        = nullptr;
    Communication *_comm           = nullptr;

    bool _noodModus                = false;

    char _normaleTekst[128]        = "Welkom";
    char _noodTekst[128]           = "NOOD: volg de veiligheidsinstructies";
    char _normaalAssembly[128]     = {};
    char _noodAssembly[128]        = {};

    unsigned long _vorigeWifiCheck = 0;

    void _setModus(bool nood);
    void _verwerkTekstDeel(const char *incoming, char *assembly, char *doelTekst, bool isNood);
    void _verwerkCommando(const char *incoming);
};
