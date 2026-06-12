#pragma once
#include <atomic>
#include <thread>
#include "BusCommunication.h"
#include "Communication.h"
#include "DataParser.h"

// Orchestrator van Pi A: koppelt de CAN-bus aan de socket naar Pi B.
// Zelfde singleton-patroon als de Wemos controllers.
class CanGatewayController {
public:
    // Geeft altijd dezelfde instantie terug
    static CanGatewayController& getInstance();

    // Dependency injection — aanroepen vóór begin()
    void init(BusCommunication *bus, Communication *socket, DataParser *parser);

    bool begin();
    void run();   // blokkeert tot stop()
    void stop();

    // Kopiëren en toewijzen verboden
    CanGatewayController(const CanGatewayController&)            = delete;
    CanGatewayController& operator=(const CanGatewayController&) = delete;

private:
    CanGatewayController() = default; // private: niemand kan new aanroepen

    BusCommunication *_bus    = nullptr;
    Communication    *_socket = nullptr;
    DataParser       *_parser = nullptr;

    std::atomic<bool> _running{false};
    std::thread       _busThread;

    void _busNaarSocket();   // thread: CAN -> JSON -> Pi B
    void _socketNaarBus();   // hoofdlus: Pi B -> JSON -> CAN
};
