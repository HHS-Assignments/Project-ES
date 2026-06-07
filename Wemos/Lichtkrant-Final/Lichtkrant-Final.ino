#include "secrets.h"
#include "ParolaDisplay.h"
#include "WiFiCommunication.h"
#include "LichtkrantController.h"

ParolaDisplay     display(MD_MAX72XX::FC16_HW, D8, 4);
WiFiCommunication wifi("10.42.0.1", 9000, 9001);

LichtkrantController lichtkrant(&display, &wifi);

void setup() { lichtkrant.begin(); }
void loop()  { lichtkrant.update(); }
