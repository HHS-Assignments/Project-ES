#include "secrets.h"
#include "ParolaDisplay.h"
#include "WiFiCommunication.h"
#include "LichtkrantController.h"

ParolaDisplay     display(MD_MAX72XX::FC16_HW, D8, 4);
WiFiCommunication wifi("10.42.0.1", 9000, 9001);

void setup() {
    LichtkrantController::getInstance().init(&display, &wifi);
    LichtkrantController::getInstance().begin();
}
void loop() {
    LichtkrantController::getInstance().update();
}
