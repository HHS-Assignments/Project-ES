#pragma once
// automations.h
// "If this, then that"-configuratie voor Pi-automatiseringen.
//   ALS tijd == hh:mm  DAN stuur CAN-bericht (id, data).
//
// De tijden zijn runtime-variabelen: ze worden aangepast via CAN ID 0x192
// (tijdconfiguratie vanaf de balieconsole, formaat 4x 16-bit big-endian:
//  [dagUur][dagMin][nachtUur][nachtMin]).

#include <cstdint>
#include <ctime>
#include <mutex>

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

static std::mutex g_autoMu;   // beschermt g_automations (0x192-handler vs. automation-thread)

// Parseert de 0x192-payload: 4x 16-bit big-endian [dagUur][dagMin][nachtUur][nachtMin].
// Geeft false bij ongeldige lengte of tijden.
inline bool parseTijdConfig(const uint8_t *data, uint8_t len,
                            int &dagH, int &dagM, int &nachtH, int &nachtM) {
    if (len != 8) return false;
    dagH   = (data[0] << 8) | data[1];
    dagM   = (data[2] << 8) | data[3];
    nachtH = (data[4] << 8) | data[5];
    nachtM = (data[6] << 8) | data[7];
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
