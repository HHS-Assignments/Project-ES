#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW  // Meestgebruikt voor deze modules
#define MAX_DEVICES 4                       // 4 matrices van 8x8
#define CS_PIN D8

MD_Parola display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// Button on D2: wire the button between D2 and GND
#define BUTTON_PIN D4

const char* normalText = "Geen Brand";
const char* pressedText = "Brand!!";
void setup() {
  display.begin();
  display.setIntensity(5);       // Helderheid 0-15
  display.displayClear();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  // Lees knop (gesloten = LOW met INPUT_PULLUP)
  if (digitalRead(BUTTON_PIN) == LOW) {
    display.displayScroll(pressedText, PA_LEFT, PA_SCROLL_LEFT, 50);
  } else {
    display.displayScroll(normalText, PA_LEFT, PA_SCROLL_LEFT, 50);
  }

  while (!display.displayAnimate()) {
    // Wacht tot animatie klaar is
  }
}