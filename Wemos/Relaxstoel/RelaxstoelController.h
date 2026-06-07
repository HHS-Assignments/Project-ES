#pragma once
#include "Motor.h"
#include "Lamp.h"
#include "Lichtsensor.h"
#include "Communication.h"

class RelaxstoelController {
public:
    RelaxstoelController(Motor *motor, Lamp *lamp,
                         Lichtsensor *sensor, Communication *comm,
                         int ldrDrempel = 100);
    void begin();
    void update();

private:
    Motor             *_motor;
    Lamp              *_lamp;
    Lichtsensor       *_sensor;
    Communication     *_comm;

    bool          _motorAan;
    int           _ldrDrempel;
    unsigned long _vorigeLdrTijd;
    unsigned long _vorigeWifiCheck;

    void _verwerkCommando(const char *incoming);
};
