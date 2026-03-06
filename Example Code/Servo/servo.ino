#include <Servo.h>

Servo mijnServo;

void setup() {
  mijnServo.attach(2); // D4 = GPIO2 op Wemos D1 Mini
}

void loop() {
  mijnServo.write(0);
  delay(1000);
  mijnServo.write(90);
  delay(1000);
  mijnServo.write(180);
  delay(1000);
}
