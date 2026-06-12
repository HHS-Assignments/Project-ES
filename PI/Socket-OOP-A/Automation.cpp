#include "Automation.h"
#include <ctime>
#include <chrono>
#include <iostream>

void AutomationEngine::addAction(const TimeAction &a) {
    std::lock_guard<std::mutex> lk(_mu);
    _actions.push_back(a);
}

void AutomationEngine::setDagNachtTijden(int dagH, int dagM, int nachtH, int nachtM) {
    std::lock_guard<std::mutex> lk(_mu);
    if (_actions.size() < 2) return;
    _actions[0].hour   = dagH;
    _actions[0].minute = dagM;
    _actions[1].hour   = nachtH;
    _actions[1].minute = nachtM;
    _actions[0].lastFiredDay = -1;
    _actions[1].lastFiredDay = -1;
    std::printf("[A] tijdconfig: dag %02d:%02d, nacht %02d:%02d\n",
                dagH, dagM, nachtH, nachtM);
}

// dagtijd <= nu < nachttijd -> dag (index 0), anders nacht (index 1)
int AutomationEngine::_huidigeModus() {
    time_t t = time(nullptr);
    tm lt{};
    localtime_r(&t, &lt);
    int nu    = lt.tm_hour * 60 + lt.tm_min;
    int dag   = _actions[0].hour * 60 + _actions[0].minute;
    int nacht = _actions[1].hour * 60 + _actions[1].minute;
    return (nu >= dag && nu < nacht) ? 0 : 1;
}

void AutomationEngine::evalueerNu() {
    uint32_t id;
    uint8_t  data;
    std::string name;
    {
        std::lock_guard<std::mutex> lk(_mu);
        if (_actions.size() < 2 || !_fire) return;
        int idx = _huidigeModus();
        id   = _actions[idx].canId;
        data = _actions[idx].data;
        name = _actions[idx].name;
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

        // Verzamel acties die nu moeten vuren (onder de lock),
        // vuur ze daarna buiten de lock af.
        std::vector<std::pair<uint32_t, uint8_t>> teVuren;
        {
            std::lock_guard<std::mutex> lk(_mu);
            for (auto &a : _actions) {
                if (a.hour == lt.tm_hour && a.minute == lt.tm_min &&
                    a.lastFiredDay != lt.tm_yday) {
                    a.lastFiredDay = lt.tm_yday;
                    std::cout << "[A] automatisering '" << a.name << "'\n";
                    teVuren.push_back({a.canId, a.data});
                }
            }
        }
        for (auto &[id, data] : teVuren) _fire(id, data);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
