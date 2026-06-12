#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include "BusCommunication.h"

// Hoe een CAN-triggerwaarde met de drempel vergeleken wordt.
enum class Vergelijking { Groter, Kleiner, Gelijk, Altijd };

// Eén automatiseringsregel: "if this, then that".
//   Tijd-trigger:  ALS tijd == hh:mm                    DAN stuur actieId + actieData
//   CAN-trigger:   ALS bericht triggerId voldoet aan
//                  (waarde <vergelijking> drempel)      DAN stuur actieId + actieData
struct Automation {
    std::string name;

    // Tijd-trigger (hour >= 0 betekent: dit is een tijd-regel)
    int hour   = -1;
    int minute = -1;
    int lastFiredDay = -1;     // dag-van-jaar waarop voor het laatst gevuurd

    // CAN-trigger (triggerId != 0 betekent: dit is een CAN-regel)
    uint32_t     triggerId    = 0;
    Vergelijking vergelijking = Vergelijking::Altijd;
    int          drempel      = 0;
    bool         conditieWasWaar = false;  // voor edge-detectie/her-bewapening

    // Actie
    uint32_t actieId  = 0;
    uint8_t  actieData = 0;
};

// Voert de automatiseringsregels uit. Tijd-regels via een 1s-pollthread op
// de Pi-klok; CAN-regels via onCanMessage() vanuit de gateway-controller.
// De regels worden geladen via AutomationConfig.h (laadAutomatiseringen).
class AutomationEngine {
public:
    void addTijdActie(int hour, int minute, uint32_t actieId, uint8_t data,
                      std::string name);
    void addCanActie(uint32_t triggerId, Vergelijking v, int drempel,
                     uint32_t actieId, uint8_t data, std::string name);

    // true  = CAN-regels vuren eenmalig per overschrijding (her-bewapening
    //         zodra de conditie weer onwaar is)
    // false = CAN-regels vuren bij elk bericht dat aan de conditie voldoet
    void setEdgeTriggered(bool edge);

    // Evalueert alle CAN-regels tegen een binnenkomend bericht.
    void onCanMessage(const CanMessage &msg);

    // Update de dag/nacht-tijden (via CAN 0x192). Zoekt de regels op
    // actie-ID: 0x410 = dag, 0x420 = nacht (volgorde in config maakt niet uit).
    void setDagNachtTijden(int dagH, int dagM, int nachtH, int nachtM);

    // Bepaalt en vuurt de huidige dag/nacht-modus direct (opstart of na 0x192).
    void evalueerNu();

    // Start de 1s-pollthread voor tijd-regels; fire = (canId, data).
    void start(std::function<void(uint32_t id, uint8_t data)> fire);
    void stop();

private:
    std::vector<Automation> _regels;
    std::mutex              _mu;
    std::atomic<bool>       _running{false};
    bool                    _edgeTriggered = true;
    std::thread             _thread;
    std::function<void(uint32_t, uint8_t)> _fire;

    void _loop();
    Automation* _vindOpActieId(uint32_t actieId);     // aanroepen met _mu vast
    static int  _frameWaarde(const CanMessage &msg);  // 16-bit BE of byte 0
    static bool _voldoet(const Automation &a, int waarde);
};
