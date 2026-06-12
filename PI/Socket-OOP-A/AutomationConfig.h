#pragma once
#include "Automation.h"

// "If this, then that"-configuratie van de Pi-automatiseringen.
//
//   Tijd-trigger:  ALS tijd == hh:mm                          DAN stuur CAN id + data
//   CAN-trigger:   ALS CAN id binnenkomt en waarde voldoet
//                  aan (vergelijking, drempel)                DAN stuur CAN id + data
//
// De dag/nacht-tijden zijn defaults; ze worden runtime overschreven door
// CAN 0x192 (tijdconfiguratie vanaf de balieconsole). De dag/nacht-regels
// worden herkend aan hun actie-ID (0x410 = dag, 0x420 = nacht).

// true  = CAN-regels vuren eenmalig per overschrijding (her-bewapening
//         zodra de conditie weer onwaar is)
// false = CAN-regels vuren bij elk bericht dat aan de conditie voldoet
#define AUTOMATION_EDGE_TRIGGERED true

inline void laadAutomatiseringen(AutomationEngine &engine) {
    engine.setEdgeTriggered(AUTOMATION_EDGE_TRIGGERED);

    //                   uur min  actie  data  naam
    engine.addTijdActie(  8,  0,  0x410, 1,    "dag_modus");
    engine.addTijdActie( 22,  0,  0x420, 1,    "nacht_modus");

    //                 trigger  vergelijking          drempel  actie  data  naam
    // ALS CAN 0x300 (CO2) > 600  DAN stuur 0x120 data 1 (alle deuren open)
    engine.addCanActie(0x300,   Vergelijking::Groter, 600,     0x120, 1,   "co2_deuren_open");

    engine.addCanActie(0x310,   Vergelijking::Groter, 25,     0x120, 1,   "temp_deuren_open");

    // deur sluiten
    engine.addCanActie(0x300,   Vergelijking::Kleiner, 600,     0x120, 0,   "co2_deuren_dicht");
    engine.addCanActie(0x310,   Vergelijking::Kleiner, 25,     0x120, 0,   "temp_deuren_dicht");
}
