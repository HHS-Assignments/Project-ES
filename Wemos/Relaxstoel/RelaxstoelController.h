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
              int ldrDrempel = 100);

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
    unsigned long _vorigeLdrTijd   = 0;
    unsigned long _vorigeWifiCheck = 0;

    void _verwerkCommando(const char *incoming);
};
