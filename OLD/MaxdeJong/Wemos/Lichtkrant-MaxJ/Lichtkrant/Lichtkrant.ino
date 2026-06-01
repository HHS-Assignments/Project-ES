#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN D8
#define BUTTON_PIN D4

MD_Parola display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

const char* berichtNormaal = "Welkom";
const char* berichtBrand   = "BRAND, volg de veiligheidslichten";

bool brandModus  = false;
bool vorigeKnop  = false;

void setup() {
  display.begin();
  display.setIntensity(5);
  display.setPause(0);
  display.displayClear();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  display.displayScroll(berichtNormaal, PA_LEFT, PA_SCROLL_LEFT, 50);
}

void loop() {
  bool knopIngedrukt = (digitalRead(BUTTON_PIN) == LOW);
  delay(50);
  // Schakel bij elke druk (stijgende flank) van modus
  if (knopIngedrukt && !vorigeKnop) {
    brandModus = !brandModus;
    display.displayClear();
    if (brandModus) {
      display.displayScroll(berichtBrand, PA_LEFT, PA_SCROLL_LEFT, 50);
    } else {
      display.displayScroll(berichtNormaal, PA_LEFT, PA_SCROLL_LEFT, 50);
    }
  }

  vorigeKnop = knopIngedrukt;

  if (display.displayAnimate()) {
    display.displayReset();
  }
}