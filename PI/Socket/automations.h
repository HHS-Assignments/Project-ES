#pragma once
// automations.h
// "If this, then that"-configuratie voor Pi-automatiseringen.
//
//   Tijd-trigger:  ALS tijd == hh:mm                          DAN stuur CAN id + data
//   CAN-trigger:   ALS CAN id binnenkomt en waarde voldoet
//                  aan (vergelijking, drempel)                DAN stuur CAN id + data
//
// De dag/nacht-tijden zijn runtime-variabelen: ze worden aangepast via CAN
// ID 0x192 (tijdconfiguratie vanaf de balieconsole, layout per 4-byte helft
// [00][uur][min][00]).

#include <cstdint>
#include <ctime>
#include <mutex>

// true  = CAN-regels vuren eenmalig per overschrijding (her-bewapening
//         zodra de conditie weer onwaar is)
// false = CAN-regels vuren bij elk bericht dat aan de conditie voldoet
#define AUTOMATION_EDGE_TRIGGERED true

// ---------- Tijd-regels ----------

struct TimeAction {
    int      hour, minute;     // wanneer (runtime aanpasbaar, geen #define)
    uint32_t canId;            // actie: CAN ID
    uint8_t  data;             // actie: databyte
    const char *name;
    int      lastFiredDay;     // dag-van-jaar waarop voor het laatst gevuurd (-1 = nog niet)
};

// Volgorde vast: [0] = dag, [1] = nacht
static TimeAction g_automations[] = {
    {  8, 0, 0x410, 1, "dag_modus",   -1 },   // default: 08:00 dag
    { 22, 0, 0x420, 1, "nacht_modus", -1 },   // default: 22:00 nacht
};
enum { AUTO_DAG = 0, AUTO_NACHT = 1 };
static const int kNumAutomations = 2;

// ---------- CAN-regels ----------

enum Vergelijking { VGL_GROTER, VGL_KLEINER, VGL_GELIJK };

struct CanAction {
    uint32_t     triggerId;        // ALS dit CAN ID binnenkomt...
    Vergelijking vergelijking;     // ...en de waarde voldoet aan...
    int          drempel;          // ...deze drempel...
    uint32_t     actieId;          // DAN stuur dit CAN ID...
    uint8_t      actieData;        // ...met deze data
    const char  *name;
    bool         conditieWasWaar;  // voor edge-detectie/her-bewapening
};

static CanAction g_canAutomations[] = {
    // ALS CAN 0x300 (CO2) > 800  DAN stuur 0x120 data 1 (alle deuren open)
    { 0x300, VGL_GROTER, 800, 0x120, 1, "co2_deuren_open", false },
};
static const int kNumCanAutomations = 1;

static std::mutex g_autoMu;   // beschermt beide regel-tabellen

// ---------- Helpers ----------

// Waarde uit een CAN-frame: 16-bit big-endian bij 2+ bytes, anders byte 0.
// (Zelfde conventie als 0x300/0x400.)
inline int frameWaarde(const uint8_t *data, uint8_t len) {
    if (len >= 2) return (data[0] << 8) | data[1];
    if (len == 1) return data[0];
    return 0;
}

// Evalueert CAN-regel [index] tegen een waarde; true = actie moet vuren.
// Met AUTOMATION_EDGE_TRIGGERED vuurt een regel alleen op de overgang
// onwaar -> waar (her-bewapening zodra de conditie weer onwaar is).
inline bool checkCanAutomatisering(int index, int waarde) {
    std::lock_guard<std::mutex> lk(g_autoMu);
    CanAction &a = g_canAutomations[index];

    bool waar = false;
    switch (a.vergelijking) {
        case VGL_GROTER:  waar = waarde >  a.drempel; break;
        case VGL_KLEINER: waar = waarde <  a.drempel; break;
        case VGL_GELIJK:  waar = waarde == a.drempel; break;
    }

    bool vuren = AUTOMATION_EDGE_TRIGGERED ? (waar && !a.conditieWasWaar)
                                           : waar;
    a.conditieWasWaar = waar;
    return vuren;
}

// Parseert de 0x192-payload. Layout per 4-byte helft: [00][uur][min][00]
//   byte 1 = dag-uur, byte 2 = dag-minuut, byte 5 = nacht-uur, byte 6 = nacht-minuut
// (voorbeeld: 08:00/14:50 -> 00 08 00 00 00 0E 32 00)
// Geeft false bij ongeldige lengte of tijden.
inline bool parseTijdConfig(const uint8_t *data, uint8_t len,
                            int &dagH, int &dagM, int &nachtH, int &nachtM) {
    if (len != 8) return false;
    dagH   = data[1];
    dagM   = data[2];
    nachtH = data[5];
    nachtM = data[6];
    return dagH < 24 && dagM < 60 && nachtH < 24 && nachtM < 60;
}

// Zet nieuwe dag/nacht-tijden en reset de vuur-status zodat een
// nieuw omslagmoment vandaag opnieuw kan vuren.
inline void setDagNachtTijden(int dagH, int dagM, int nachtH, int nachtM) {
    std::lock_guard<std::mutex> lk(g_autoMu);
    g_automations[AUTO_DAG].hour     = dagH;
    g_automations[AUTO_DAG].minute   = dagM;
    g_automations[AUTO_NACHT].hour   = nachtH;
    g_automations[AUTO_NACHT].minute = nachtM;
    g_automations[AUTO_DAG].lastFiredDay   = -1;
    g_automations[AUTO_NACHT].lastFiredDay = -1;
}

// Bepaalt welke modus nu actief hoort te zijn op basis van de Pi-klok:
// dagtijd <= nu < nachttijd -> AUTO_DAG, anders AUTO_NACHT.
inline int bepaalHuidigeModus() {
    time_t t = time(nullptr);
    tm lt{};
    localtime_r(&t, &lt);
    int nu = lt.tm_hour * 60 + lt.tm_min;

    std::lock_guard<std::mutex> lk(g_autoMu);
    int dag   = g_automations[AUTO_DAG].hour   * 60 + g_automations[AUTO_DAG].minute;
    int nacht = g_automations[AUTO_NACHT].hour * 60 + g_automations[AUTO_NACHT].minute;
    return (nu >= dag && nu < nacht) ? AUTO_DAG : AUTO_NACHT;
}
