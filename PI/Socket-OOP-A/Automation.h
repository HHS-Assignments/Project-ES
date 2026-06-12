#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>

// Eén tijdgestuurde actie: ALS tijd == hh:mm DAN stuur CAN-bericht (id, data).
struct TimeAction {
    int         hour, minute;      // runtime aanpasbaar (geen #define)
    uint32_t    canId;
    uint8_t     data;
    std::string name;
    int         lastFiredDay = -1; // dag-van-jaar waarop voor het laatst gevuurd
};

// Voert tijdgestuurde automatiseringen uit op basis van de Pi-klok.
// De acties worden geladen via AutomationConfig.h (laadAutomatiseringen).
class AutomationEngine {
public:
    void addAction(const TimeAction &a);

    // Update de dag/nacht-tijden (via CAN 0x192) en reset de vuur-status.
    // Actie [0] = dag, [1] = nacht (volgorde van laden).
    void setDagNachtTijden(int dagH, int dagM, int nachtH, int nachtM);

    // Bepaalt en vuurt de huidige modus direct (opstart of na 0x192).
    void evalueerNu();

    // Start de 1s-pollthread; fire wordt aangeroepen met (canId, data).
    void start(std::function<void(uint32_t id, uint8_t data)> fire);
    void stop();

private:
    std::vector<TimeAction> _actions;   // [0]=dag, [1]=nacht
    std::mutex              _mu;
    std::atomic<bool>       _running{false};
    std::thread             _thread;
    std::function<void(uint32_t, uint8_t)> _fire;

    void _loop();
    int  _huidigeModus();   // index in _actions; aanroepen met _mu vast
};
