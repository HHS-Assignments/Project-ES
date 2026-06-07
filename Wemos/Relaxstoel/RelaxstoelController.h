#pragma once
#include "Motor.h"
#include "Lamp.h"
#include "Lichtsensor.h"
#include "WiFiCommunication.h"

class RelaxstoelController {
public:
    RelaxstoelController(Motor *motor, Lamp *lamp,
                         Lichtsensor *sensor, WiFiCommunication *comm,
                         int ldrDrempel = 100);
    void begin();
    void update();

private:
    Motor             *_motor;
    Lamp              *_lamp;
    Lichtsensor       *_sensor;
    WiFiCommunication *_comm;

    bool          _motorAan;
    int           _ldrDrempel;
    unsigned long _vorigeLdrTijd;
    unsigned long _vorigeWifiCheck;

    void _verwerkCommando(const char *incoming);
};
