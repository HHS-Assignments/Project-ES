#include "Schemerlamp.h"
#include "Relaxstoel.h"


#define LDR_PIN       A0
#define LED_PIN       D5
#define MOTOR_PIN     D2
#define START_BTN     D3
#define ESTOP_BTN     D6
#define DREMPELWAARDE 500


Schemerlamp schemerlamp(LDR_PIN, LED_PIN, DREMPELWAARDE);
Motor       motor(MOTOR_PIN);

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
        delay(50);
    }
}

void setup() {
    Serial.begin(9600);
    schemerlamp.begin();
    motor.begin();
    motor.setSpeed(0);
    pinMode(START_BTN, INPUT_PULLUP);
    pinMode(ESTOP_BTN, INPUT_PULLUP);
    Serial.println("Relaxhoek gestart");
}

void loop() {

    schemerlamp.update();

    Serial.print("LDR: ");
    Serial.print(schemerlamp.getLdrWaarde());
    Serial.print(" | Lamp: ");
    Serial.print(schemerlamp.isAan() ? "AAN" : "UIT");
    Serial.print(" | Motor: ");
    Serial.println(running ? "LOOPT" : "GESTOPT");


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

    if (!running) {
        delay(500);
        return;
    }

    runStep(10,  2500); if (!running) return;
    runStep(50,  2500); if (!running) return;
    runStep(100, 2500); if (!running) return;

    motor.setSpeed(0);
    running = false;
}
