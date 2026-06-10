#include "secrets.h"
#include "Vibratiemotor.h"
#include "Schemerlamp.h"
#include "LDR.h"
#include "WiFiCommunication.h"
#include "RelaxstoelController.h"

// XIAO ESP32C6 pinout:
//  D2 = GPIO4  → Vibratiemotor (PWM)
//  D3 = GPIO5  → Schemerlamp (LED)
//  A0 = GPIO2  → LDR (analoog)
//  D7 = GPIO20 → Knop (INPUT_PULLUP)
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
