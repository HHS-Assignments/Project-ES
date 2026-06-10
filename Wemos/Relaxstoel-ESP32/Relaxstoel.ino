#include "secrets.h"
#include "Vibratiemotor.h"
#include "Schemerlamp.h"
#include "LDR.h"
#include "WiFiCommunication.h"
#include "RelaxstoelController.h"

// XIAO ESP32C6 pinout:
//  D2 = GPIO2  → Vibratiemotor (PWM)
//  D3 = GPIO21 → Schemerlamp (LED)
//  A0 = GPIO0  → LDR (analoog, A0 mapt naar D0 in BSP)
//  D7 = GPIO17 → Knop (INPUT_PULLUP)
// Let op: gebruik GPIO3 (RF-switch power) en GPIO14 (antennekeuze) NIET voor eigen I/O.
Vibratiemotor     motor(D2);
Schemerlamp       lamp(D3);
LDR               ldr(A0);
WiFiCommunication wifi("10.42.0.1", 9000, 9001);

void setup() {
    RelaxstoelController::getInstance().init(&motor, &lamp, &ldr, &wifi, 100, D7);
    RelaxstoelController::getInstance().begin();
}
void loop() {
    RelaxstoelController::getInstance().update();
}
