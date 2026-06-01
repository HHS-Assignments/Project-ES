#include "Schemerlamp.h"

#define LDR_PIN A0
#define LED_PIN D5
#define DREMPELWAARDE 500   

Schemerlamp schemerlamp(LDR_PIN, LED_PIN, DREMPELWAARDE);

void setup() {
    Serial.begin(9600);
    schemerlamp.begin();
    Serial.println("Schemerlamp gestart");
}

void loop() {
    schemerlamp.update();

    Serial.print("LDR waarde: ");
    Serial.print(schemerlamp.getLdrWaarde());
    Serial.print(" | Lamp: ");
    Serial.println(schemerlamp.isAan() ? "AAN" : "UIT");

    delay(500);
}