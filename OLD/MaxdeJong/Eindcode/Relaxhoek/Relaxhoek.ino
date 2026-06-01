#include "Relaxstoel.h"
#include "Schemerlamp.h"

Motor motor(D2);
Schemerlamp schemerlamp(A0, D5, 100);

const uint8_t START_BTN = D7;
const uint8_t ESTOP_BTN = D6;
bool running = false;

void runStep(uint8_t speed, unsigned long ms) {
    motor.setSpeed(speed);
    unsigned long start = millis();
    while (millis() - start < ms) {
        if (digitalRead(ESTOP_BTN) == LOW) {
            motor.setSpeed(0);
            running = false;
            while (digitalRead(ESTOP_BTN) == LOW) delay(10);
            return;
        }
        schemerlamp.update();
        delay(50);
    }
}

void setup() {
    Serial.begin(9600);
    motor.begin();
    schemerlamp.begin();
    pinMode(START_BTN, INPUT_PULLUP);
    pinMode(ESTOP_BTN, INPUT_PULLUP);
}

void loop() {
    // Serial.println(analogRead(A0));
    // delay(200);
    schemerlamp.update();

    if (digitalRead(ESTOP_BTN) == LOW) {
        running = false;
        motor.setSpeed(0);
        while (digitalRead(ESTOP_BTN) == LOW) delay(10);
        return;
    }
    if (digitalRead(START_BTN) == LOW && !running) {
        delay(50);
        if (digitalRead(START_BTN) == LOW) {
            running = true;
            while (digitalRead(START_BTN) == LOW) delay(10);
        }
    }
    if (!running) return;
    runStep(10,  2500);
    if (!running) return;
    runStep(50,  2500);
    if (!running) return;
    runStep(100, 2500);
    if (!running) return;
}