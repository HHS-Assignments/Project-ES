#include "secrets.h"
#include "Vibratiemotor.h"
#include "Schemerlamp.h"
#include "LDR.h"
#include "WiFiCommunication.h"
#include "RelaxstoelController.h"

Vibratiemotor     motor(D2);
Schemerlamp       lamp(D5);
LDR               ldr(A0);
WiFiCommunication wifi("10.42.0.1", 9000, 9001);

void setup() {
    RelaxstoelController::getInstance().init(&motor, &lamp, &ldr, &wifi, 100);
    RelaxstoelController::getInstance().begin();
}
void loop() {
    RelaxstoelController::getInstance().update();
}
