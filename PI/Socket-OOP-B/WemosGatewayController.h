#pragma once
#include <atomic>
#include <thread>
#include "Communication.h"
#include "DataParser.h"

// Orchestrator van Pi B: routeert berichten tussen Pi A en de Wemos-apparaten.
// Zelfde singleton-patroon als de Wemos controllers.
class WemosGatewayController {
public:
    // Geeft altijd dezelfde instantie terug
    static WemosGatewayController& getInstance();

    // Dependency injection — aanroepen vóór begin()
    void init(Communication *piSocket, Communication *wemos, DataParser *parser);

    bool begin();
    void run();   // blokkeert tot stop()
    void stop();

    // Kopiëren en toewijzen verboden
    WemosGatewayController(const WemosGatewayController&)            = delete;
    WemosGatewayController& operator=(const WemosGatewayController&) = delete;

private:
    WemosGatewayController() = default; // private: niemand kan new aanroepen

    Communication *_piSocket = nullptr;
    Communication *_wemos    = nullptr;
    DataParser    *_parser   = nullptr;

    std::atomic<bool> _running{false};
    std::thread       _piThread;

    void _piNaarWemos();    // thread: Pi A -> fan-out naar Wemos'es
    void _wemosNaarPi();    // hoofdlus: Wemos -> Pi A
};
