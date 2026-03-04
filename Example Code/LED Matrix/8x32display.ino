#include <LedControl.h>

// 4 modules in serie
LedControl lc = LedControl(D7, D5, D8, 4);

// 8 rijen, 32 kolommen (4 modules x 8)
bool pijl[8][32] = {
  {0,0,0,0,1,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},
  {0,0,0,0,0,1,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,1,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},
  {1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1},
  {0,0,0,0,0,0,1,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},
  {0,0,0,0,0,1,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},
  {0,0,0,0,1,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0}
};

void setup() {
  // Alle 4 modules initialiseren
  for (int m = 0; m < 4; m++) {
    lc.shutdown(m, false);
    lc.setIntensity(m, 8);
    lc.clearDisplay(m);
  }

  // Afbeelding tonen
  tekenAfbeelding();
}

void tekenAfbeelding() {
  for (int rij = 0; rij < 8; rij++) {
    for (int module = 0; module < 4; module++) {
      for (int kolom = 0; kolom < 8; kolom++) {
        // Globale kolom = module * 8 + kolom
        lc.setLed(module, rij, kolom, pijl[rij][module * 8 + kolom]);
      }
    }
  }
}

void loop() {}
