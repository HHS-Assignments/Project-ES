
#include <FastLED.h>

#define NUM_LEDS 8
#define DATA_PIN 2
#define CHASE_SPEED 40      // ms between steps (lower = faster)
#define TAIL_LENGTH 3       // how many LEDs glow in the trailing fade
#define BRIGHTNESS 200

CRGB leds[NUM_LEDS];

void setup() {
  FastLED.addLeds<WS2813, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
}

void loop() {
  runwayChase();
}

void runwayChase() {
  // Run the light from LED 0 → end, like a plane approaching
  for (int pos = 0; pos < NUM_LEDS + TAIL_LENGTH; pos++) {

    // Clear all LEDs
    FastLED.clear();

    // Draw the chasing head + fading tail
    for (int t = 0; t < TAIL_LENGTH; t++) {
      int ledIndex = pos - t;
      if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
        // Head is brightest white, tail fades to amber
        uint8_t fade = 255 - (t * (255 / TAIL_LENGTH));
        if (t == 0) {
          leds[ledIndex] = CRGB(255, 255, 255);           // Bright white head
        } else {
          leds[ledIndex] = CRGB(255, fade / 2, 0);        // Amber fade tail
        }
      }
    }

    FastLED.show();
    delay(CHASE_SPEED);
  }

  // Brief pause at end before repeating (like a cycle reset)
  delay(120);
}