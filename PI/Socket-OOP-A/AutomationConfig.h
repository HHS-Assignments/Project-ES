#pragma once
#include "Automation.h"

// "If this, then that"-configuratie van de Pi-automatiseringen.
//
//   ALS tijd == hh:mm  DAN stuur CAN id + data
//
// Volgorde is van belang: actie [0] = dag, actie [1] = nacht.
// De tijden zijn defaults; ze worden runtime overschreven door
// CAN 0x192 (tijdconfiguratie vanaf de balieconsole).
inline void laadAutomatiseringen(AutomationEngine &engine) {
    //                 uur min  CAN ID  data  naam
    engine.addAction({  8,  0,  0x410,  1,    "dag_modus"   });
    engine.addAction({ 22,  0,  0x420,  1,    "nacht_modus" });
}
