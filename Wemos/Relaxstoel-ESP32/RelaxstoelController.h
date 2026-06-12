#pragma once
#include "Motor.h"
#include "Lamp.h"
#include "Lichtsensor.h"
#include "Communication.h"

class RelaxstoelController {
public:
    // Geeft altijd dezelfde instantie terug
    static RelaxstoelController& getInstance();

    // Dependency injection — aanroepen vóór begin()
    void init(Motor *motor, Lamp *lamp,
              Lichtsensor *sensor, Communication *comm,
              int ldrDrempel = 100, uint8_t knopPin = 255);

    void begin();
    void update();

    // Kopiëren en toewijzen verboden
    RelaxstoelController(const RelaxstoelController&)            = delete;
    RelaxstoelController& operator=(const RelaxstoelController&) = delete;

private:
    RelaxstoelController() = default; // private: niemand kan new aanroepen

    Motor         *_motor          = nullptr;
    Lamp          *_lamp           = nullptr;
    Lichtsensor   *_sensor         = nullptr;
    Communication *_comm           = nullptr;

    bool          _motorAan        = false;
    int           _ldrDrempel      = 100;
    int           _laatsteLdr      = 1024;  // start "licht" zodat lamp niet flitst bij boot
    unsigned long _vorigeLdrTijd   = 0;
    unsigned long _vorigeLdrLees   = 0;
    unsigned long _vorigeWifiCheck = 0;

    uint8_t       _knopPin         = 255;   // 255 = geen knop geconfigureerd
    bool          _vorigeKnopStaat = true;  // HIGH = niet ingedrukt (INPUT_PULLUP)

    void _verwerkCommando(const char *incoming);
    void _setMotor(bool aan);
};
