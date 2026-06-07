#include "LichtkrantController.h"
#include <string.h>
#include <stdlib.h>
#include <Arduino.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing secrets.h"
#endif

LichtkrantController::LichtkrantController(Display *display, Communication *comm)
    : _display(display), _comm(comm), _noodModus(false), _vorigeWifiCheck(0)
{
    strncpy(_normaleTekst, "Welkom", sizeof(_normaleTekst) - 1);
    strncpy(_noodTekst,    "NOOD: volg de veiligheidsinstructies", sizeof(_noodTekst) - 1);
    _normaalAssembly[0] = '\0';
    _noodAssembly[0]    = '\0';
}

void LichtkrantController::begin() {
    Serial.begin(9600);
    delay(100);
    _display->begin();
    _comm->connect(WIFI_SSID, WIFI_PASSWORD);
    _comm->begin();
    _setModus(false);
}

void LichtkrantController::update() {
    _display->update();

    char buf[256];
    if (_comm->receive(buf, sizeof(buf))) {
        _verwerkCommando(buf);
    }

    unsigned long nu = millis();
    if (nu - _vorigeWifiCheck >= 5000) {
        _vorigeWifiCheck = nu;
        if (!_comm->isConnected()) {
            Serial.println(F("[WiFi] Verbinding verloren, opnieuw verbinden..."));
            _comm->connect(WIFI_SSID, WIFI_PASSWORD);
        }
    }
}

void LichtkrantController::_setModus(bool nood) {
    _noodModus = nood;
    _display->toonTekst(_noodModus ? _noodTekst : _normaleTekst);
}

void LichtkrantController::_verwerkTekstDeel(const char *incoming, char *assembly,
                                              char *doelTekst, bool isNood) {
    const char *dataPtr = strstr(incoming, "\"Data\":\"");
    if (dataPtr == nullptr) return;
    dataPtr += 8; // sla "Data":" over

    const char *einde = strchr(dataPtr, '"');
    if (einde == nullptr) return;

    size_t len = (size_t)(einde - dataPtr);
    strncat(assembly, dataPtr, len);

    const char *morePtr = strstr(incoming, "\"More\":");
    int more = (morePtr != nullptr) ? atoi(morePtr + 7) : 0;

    if (more == 0) {
        // Laatste deel ontvangen: kopieer naar doeltekst en refresh display
        strncpy(doelTekst, assembly, 127);
        doelTekst[127]  = '\0';
        assembly[0]     = '\0';
        if (isNood == _noodModus) {
            _setModus(_noodModus);
        }
    }
}

void LichtkrantController::_verwerkCommando(const char *incoming) {
    // Noodsituatie (0x001) of noodknop (0x210)
    if (strstr(incoming, "0x001") || strstr(incoming, "0x210")) {
        const char *dataPtr = strstr(incoming, "\"Data\":");
        int waarde = (dataPtr != nullptr) ? atoi(dataPtr + 7) : 1;
        _setModus(waarde == 1);
        return;
    }

    // Noodtekst deel 1 — reset assembly buffer
    if (strstr(incoming, "0x150")) {
        _noodAssembly[0] = '\0';
        _verwerkTekstDeel(incoming, _noodAssembly, _noodTekst, true);
        return;
    }
    // Noodtekst deel 2 of 3
    if (strstr(incoming, "0x160") || strstr(incoming, "0x170")) {
        _verwerkTekstDeel(incoming, _noodAssembly, _noodTekst, true);
        return;
    }

    // Normale tekst deel 1 — reset assembly buffer
    if (strstr(incoming, "0x180")) {
        _normaalAssembly[0] = '\0';
        _verwerkTekstDeel(incoming, _normaalAssembly, _normaleTekst, false);
        return;
    }
    // Normale tekst deel 2 of 3
    if (strstr(incoming, "0x190") || strstr(incoming, "0x101")) {
        _verwerkTekstDeel(incoming, _normaalAssembly, _normaleTekst, false);
        return;
    }
}
