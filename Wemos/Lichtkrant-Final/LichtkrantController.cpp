#include "LichtkrantController.h"
#include <string.h>
#include <stdlib.h>
#include <Arduino.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing secrets.h"
#endif

LichtkrantController& LichtkrantController::getInstance() {
    static LichtkrantController instance;
    return instance;
}

void LichtkrantController::init(Display *display, Communication *comm) {
    _display = display;
    _comm    = comm;
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

void LichtkrantController::_verwerkTekstDeel(const char *incoming, char delen[3][64],
                                              int index, char *doelTekst, bool isNood) {
    const char *dataPtr = strstr(incoming, "\"Data\":\"");
    if (dataPtr == nullptr) return;
    dataPtr += 8; // sla "Data":" over

    const char *einde = strchr(dataPtr, '"');
    if (einde == nullptr) return;

    // Deel 1 start een nieuwe tekst: maak de oude delen 2 en 3 leeg
    if (index == 0) {
        delen[1][0] = '\0';
        delen[2][0] = '\0';
    }

    // Tekst in het vaste slot van dit CAN ID zetten
    size_t len = (size_t)(einde - dataPtr);
    if (len > 63) len = 63;
    memcpy(delen[index], dataPtr, len);
    delen[index][len] = '\0';

    // Tekst opnieuw samenstellen uit alle slots (More-vlag wordt genegeerd)
    doelTekst[0] = '\0';
    for (int i = 0; i < 3; i++) {
        strncat(doelTekst, delen[i], 127 - strlen(doelTekst));
    }

    if (isNood == _noodModus) {
        _setModus(_noodModus);
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

    // Noodtekst: 0x150 = deel 1, 0x160 = deel 2, 0x170 = deel 3
    if (strstr(incoming, "0x150")) { _verwerkTekstDeel(incoming, _noodDelen, 0, _noodTekst, true); return; }
    if (strstr(incoming, "0x160")) { _verwerkTekstDeel(incoming, _noodDelen, 1, _noodTekst, true); return; }
    if (strstr(incoming, "0x170")) { _verwerkTekstDeel(incoming, _noodDelen, 2, _noodTekst, true); return; }

    // Normale tekst: 0x180 = deel 1, 0x190 = deel 2, 0x191 = deel 3
    if (strstr(incoming, "0x180")) { _verwerkTekstDeel(incoming, _normaalDelen, 0, _normaleTekst, false); return; }
    if (strstr(incoming, "0x190")) { _verwerkTekstDeel(incoming, _normaalDelen, 1, _normaleTekst, false); return; }
    if (strstr(incoming, "0x191")) { _verwerkTekstDeel(incoming, _normaalDelen, 2, _normaleTekst, false); return; }
}
