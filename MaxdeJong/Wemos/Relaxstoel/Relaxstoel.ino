#include "Relaxstoel.h"

Motor motor(D2);

void setup() {
    motor.begin();
}

void loop() {
    motor.setSpeed(75);  
    delay(2500);
    motor.setSpeed(100); 
    delay(2500);
    motor.setSpeed(50);
    delay(2500);
}