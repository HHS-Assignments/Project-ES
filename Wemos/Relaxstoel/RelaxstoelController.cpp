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
                                 int ldrDrempel, uint8_t knopPin) {
    _motor       = motor;
    _lamp        = lamp;
    _sensor      = sensor;
    _comm        = comm;
    _ldrDrempel  = ldrDrempel;
    _knopPin     = knopPin;
}

void RelaxstoelController::begin() {
    Serial.begin(115200);
    delay(100);
    _motor->begin();
    _lamp->begin();
    _sensor->begin();
    if (_knopPin != 255) {
        pinMode(_knopPin, INPUT_PULLUP);
        Serial.print(F("[Knop] Geconfigureerd op pin "));
        Serial.println(_knopPin);
    }
    _comm->connect(WIFI_SSID, WIFI_PASSWORD);
    _comm->begin();

    // Timers initialiseren zodat ze niet direct afvuren op de eerste update()
    unsigned long nu = millis();
    _vorigeLdrTijd   = nu;
    _vorigeWifiCheck = nu;
}

void RelaxstoelController::update() {
    char buf[256];
    if (_comm->receive(buf, sizeof(buf))) {
        _verwerkCommando(buf);
    }

    // Fysieke knop: toggle bij neergaande flank (INPUT_PULLUP: HIGH=los, LOW=ingedrukt)
    if (_knopPin != 255) {
        bool huidig = digitalRead(_knopPin);
        if (_vorigeKnopStaat == HIGH && huidig == LOW) {
            // Knop net ingedrukt
            _setMotor(!_motorAan);
            Serial.println(F("[Knop] Toggle relaxstoel"));
        }
        _vorigeKnopStaat = huidig;
    }

    unsigned long nu = millis();

    // LDR maximaal 5x per seconde uitlezen: de ESP8266 ADC wordt gedeeld
    // met de WiFi-radio en te vaak analogRead() aanroepen verbreekt de verbinding
    if (nu - _vorigeLdrLees >= 200) {
        _vorigeLdrLees = nu;
        _laatsteLdr = _sensor->getWaarde();
        if (_laatsteLdr < _ldrDrempel) _lamp->setAan();
        else                           _lamp->setUit();
    }
    int ldr = _laatsteLdr;

    // Stuur LDR waarde elke 10 seconden als CAN 0x400
    if (nu - _vorigeLdrTijd >= 10000) {
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
            _comm->reconnect();
        }
    }
}

void RelaxstoelController::_setMotor(bool aan) {
    if (aan == _motorAan) return;
    _motorAan = aan;
    if (_motorAan) {
        _motor->setSpeed(50);
        Serial.println(F("[Motor] Relaxstoel AAN"));
    } else {
        _motor->setSpeed(0);
        Serial.println(F("[Motor] Relaxstoel UIT"));
    }
    // Alleen status sturen als WiFi er is; anders blokkeert client.connect() seconden
    if (_comm->isConnected()) {
        _comm->sendCanJson("0x430", _motorAan ? 1 : 0);
    }
}

void RelaxstoelController::_verwerkCommando(const char *incoming) {
    if (strstr(incoming, "0x140") == nullptr) return;

    const char *dataPtr = strstr(incoming, "\"Data\":");
    if (dataPtr == nullptr) return;

    // Sla spaties en aanhalingstekens over zodat zowel "Data":1 als "Data":"01" werkt
    const char *p = dataPtr + 7;
    while (*p == ' ' || *p == '"') p++;

    int waarde = atoi(p);
    _setMotor(waarde != 0);
}
