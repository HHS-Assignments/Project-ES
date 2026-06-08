#include "RelaxstoelController.h"
#include <string.h>
#include <stdlib.h>
#include <Arduino.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing secrets.h"
#endif

RelaxstoelController& RelaxstoelController::getInstance() {
    static RelaxstoelController instance;
    return instance;
}

void RelaxstoelController::init(Motor *motor, Lamp *lamp,
                                 Lichtsensor *sensor, Communication *comm,
                                 int ldrDrempel) {
    _motor       = motor;
    _lamp        = lamp;
    _sensor      = sensor;
    _comm        = comm;
    _ldrDrempel  = ldrDrempel;
}

void RelaxstoelController::begin() {
    Serial.begin(9600);
    delay(100);
    _motor->begin();
    _lamp->begin();
    _sensor->begin();
    _comm->connect(WIFI_SSID, WIFI_PASSWORD);
    _comm->begin();
}

void RelaxstoelController::update() {
    char buf[256];
    if (_comm->receive(buf, sizeof(buf))) {
        _verwerkCommando(buf);
    }

    // Lamp automatisch aansturen op basis van LDR
    int ldr = _sensor->getWaarde();
    if (ldr < _ldrDrempel) _lamp->setAan();
    else                   _lamp->setUit();

    unsigned long nu = millis();

    // Stuur LDR waarde elke 500ms als CAN 0x400
    if (nu - _vorigeLdrTijd >= 500) {
        _vorigeLdrTijd = nu;
        if (_comm->isConnected()) {
            Serial.print(F("[0x400] LDR: ")); Serial.println(ldr);
            _comm->sendCanJson("0x400", ldr);
        }
    }

    // Herverbinden als WiFi weg is, elke 5 seconden checken
    if (nu - _vorigeWifiCheck >= 5000) {
        _vorigeWifiCheck = nu;
        if (!_comm->isConnected()) {
            Serial.println(F("[WiFi] Verbinding verloren, opnieuw verbinden..."));
            _comm->connect(WIFI_SSID, WIFI_PASSWORD);
        }
    }
}

void RelaxstoelController::_verwerkCommando(const char *incoming) {
    if (strstr(incoming, "0x140") == nullptr) return;

    const char *dataPtr = strstr(incoming, "\"Data\":");
    if (dataPtr == nullptr) return;

    int waarde = atoi(dataPtr + 7);

    if (waarde == 1 && !_motorAan) {
        _motorAan = true;
        _motor->setSpeed(50);
        Serial.println(F("[0x140] Relaxstoel AAN"));
        _comm->sendCanJson("0x430", 1);
    } else if (waarde == 0 && _motorAan) {
        _motorAan = false;
        _motor->setSpeed(0);
        Serial.println(F("[0x140] Relaxstoel UIT"));
        _comm->sendCanJson("0x430", 0);
    }
}
