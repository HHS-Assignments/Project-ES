#include "Automation.h"
#include <ctime>
#include <chrono>
#include <cstdio>
#include <iostream>

void AutomationEngine::addTijdActie(int hour, int minute, uint32_t actieId,
                                    uint8_t data, std::string name) {
    Automation a;
    a.name      = std::move(name);
    a.hour      = hour;
    a.minute    = minute;
    a.actieId   = actieId;
    a.actieData = data;
    std::lock_guard<std::mutex> lk(_mu);
    _regels.push_back(std::move(a));
}

void AutomationEngine::addCanActie(uint32_t triggerId, Vergelijking v, int drempel,
                                   uint32_t actieId, uint8_t data, std::string name) {
    Automation a;
    a.name         = std::move(name);
    a.triggerId    = triggerId;
    a.vergelijking = v;
    a.drempel      = drempel;
    a.actieId      = actieId;
    a.actieData    = data;
    std::lock_guard<std::mutex> lk(_mu);
    _regels.push_back(std::move(a));
}

void AutomationEngine::setEdgeTriggered(bool edge) {
    std::lock_guard<std::mutex> lk(_mu);
    _edgeTriggered = edge;
}

// Waarde uit een CAN-frame: 16-bit big-endian bij 2+ bytes, anders byte 0.
// (Zelfde conventie als 0x300/0x400.)
int AutomationEngine::_frameWaarde(const CanMessage &msg) {
    if (msg.len >= 2) return (msg.data[0] << 8) | msg.data[1];
    if (msg.len == 1) return msg.data[0];
    return 0;
}

bool AutomationEngine::_voldoet(const Automation &a, int waarde) {
    switch (a.vergelijking) {
        case Vergelijking::Groter:  return waarde >  a.drempel;
        case Vergelijking::Kleiner: return waarde <  a.drempel;
        case Vergelijking::Gelijk:  return waarde == a.drempel;
        case Vergelijking::Altijd:  return true;
    }
    return false;
}

void AutomationEngine::onCanMessage(const CanMessage &msg) {
    // Verzamel te vuren acties onder de lock, vuur daarna buiten de lock.
    std::vector<std::pair<uint32_t, uint8_t>> teVuren;
    {
        std::lock_guard<std::mutex> lk(_mu);
        if (!_fire) return;
        int waarde = _frameWaarde(msg);
        for (auto &a : _regels) {
            if (a.triggerId == 0 || a.triggerId != msg.id) continue;

            bool waar = _voldoet(a, waarde);
            bool vuren = _edgeTriggered ? (waar && !a.conditieWasWaar)  // alleen op de flank
                                        : waar;                        // bij elk passend bericht
            a.conditieWasWaar = waar;

            if (vuren) {
                std::cout << "[A] automatisering '" << a.name << "' (waarde "
                          << waarde << ")\n";
                teVuren.push_back({a.actieId, a.actieData});
            }
        }
    }
    for (auto &[id, data] : teVuren) _fire(id, data);
}

Automation* AutomationEngine::_vindOpActieId(uint32_t actieId) {
    for (auto &a : _regels) {
        if (a.hour >= 0 && a.actieId == actieId) return &a;
    }
    return nullptr;
}

void AutomationEngine::setDagNachtTijden(int dagH, int dagM, int nachtH, int nachtM) {
    std::lock_guard<std::mutex> lk(_mu);
    Automation *dag   = _vindOpActieId(0x410);
    Automation *nacht = _vindOpActieId(0x420);
    if (dag == nullptr || nacht == nullptr) {
        std::cerr << "[A] tijdconfig genegeerd: geen dag/nacht-regels geladen\n";
        return;
    }
    dag->hour     = dagH;
    dag->minute   = dagM;
    nacht->hour   = nachtH;
    nacht->minute = nachtM;
    dag->lastFiredDay   = -1;
    nacht->lastFiredDay = -1;
    std::printf("[A] tijdconfig: dag %02d:%02d, nacht %02d:%02d\n",
                dagH, dagM, nachtH, nachtM);
}

void AutomationEngine::evalueerNu() {
    uint32_t id;
    uint8_t  data;
    std::string name;
    {
        std::lock_guard<std::mutex> lk(_mu);
        if (!_fire) return;
        Automation *dag   = _vindOpActieId(0x410);
        Automation *nacht = _vindOpActieId(0x420);
        if (dag == nullptr || nacht == nullptr) return;

        time_t t = time(nullptr);
        tm lt{};
        localtime_r(&t, &lt);
        int nu      = lt.tm_hour * 60 + lt.tm_min;
        int dagMin  = dag->hour * 60 + dag->minute;
        int nachtMn = nacht->hour * 60 + nacht->minute;

        const Automation *actief = (nu >= dagMin && nu < nachtMn) ? dag : nacht;
        id   = actief->actieId;
        data = actief->actieData;
        name = actief->name;
    }
    std::cout << "[A] automatisering '" << name << "' (directe evaluatie)\n";
    _fire(id, data);
}

void AutomationEngine::start(std::function<void(uint32_t, uint8_t)> fire) {
    {
        std::lock_guard<std::mutex> lk(_mu);
        _fire = std::move(fire);
    }
    _running = true;
    _thread = std::thread(&AutomationEngine::_loop, this);
}

void AutomationEngine::stop() {
    _running = false;
    if (_thread.joinable()) _thread.join();
}

void AutomationEngine::_loop() {
    while (_running) {
        time_t t = time(nullptr);
        tm lt{};
        localtime_r(&t, &lt);

        // Verzamel tijd-regels die nu moeten vuren (onder de lock),
        // vuur ze daarna buiten de lock af.
        std::vector<std::pair<uint32_t, uint8_t>> teVuren;
        {
            std::lock_guard<std::mutex> lk(_mu);
            for (auto &a : _regels) {
                if (a.hour < 0) continue;  // geen tijd-regel
                if (a.hour == lt.tm_hour && a.minute == lt.tm_min &&
                    a.lastFiredDay != lt.tm_yday) {
                    a.lastFiredDay = lt.tm_yday;
                    std::cout << "[A] automatisering '" << a.name << "'\n";
                    teVuren.push_back({a.actieId, a.actieData});
                }
            }
        }
        for (auto &[id, data] : teVuren) _fire(id, data);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
