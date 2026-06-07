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

RelaxstoelController relaxstoel(&motor, &lamp, &ldr, &wifi, 100);

void setup() { relaxstoel.begin(); }
void loop()  { relaxstoel.update(); }
