#include "CanGatewayController.h"
#include <iostream>

CanGatewayController& CanGatewayController::getInstance() {
    static CanGatewayController instance;
    return instance;
}

void CanGatewayController::init(BusCommunication *bus, Communication *socket,
                                DataParser *parser) {
    _bus    = bus;
    _socket = socket;
    _parser = parser;
}

bool CanGatewayController::begin() {
    if (!_bus->begin()) {
        std::cerr << "[A] CAN-bus openen mislukt\n";
        return false;
    }
    if (!_socket->begin()) {
        std::cerr << "[A] Socket-server starten mislukt\n";
        _bus->stop();
        return false;
    }
    return true;
}

void CanGatewayController::run() {
    _running = true;
    _busThread = std::thread(&CanGatewayController::_busNaarSocket, this);
    _socketNaarBus();  // blokkeert in de hoofdthread
    stop();
}

void CanGatewayController::stop() {
    if (!_running.exchange(false)) return;
    _socket->stop();
    _bus->stop();
    if (_busThread.joinable()) _busThread.join();
}

// CAN -> JSON -> Pi B
void CanGatewayController::_busNaarSocket() {
    CanMessage msg;
    while (_running) {
        if (!_bus->readMessage(msg)) break;

        std::string json = _parser->messageToJson(msg);
        if (_socket->isConnected()) {
            if (_socket->send(json)) {
                std::cout << "[A] CAN->B " << json << "\n";
            } else {
                std::cerr << "[A] verzenden naar Pi B mislukt\n";
            }
        }
    }
}

// Pi B -> JSON -> CAN
void CanGatewayController::_socketNaarBus() {
    std::string line;
    while (_running) {
        if (!_socket->receive(line)) break;

        CanMessage msg;
        if (!_parser->jsonToMessage(line, msg)) {
            std::cerr << "[A] ongeldige JSON: " << line << "\n";
            continue;
        }
        if (_bus->writeMessage(msg)) {
            std::cout << "[A] B->CAN id=0x" << std::hex << msg.id << std::dec
                      << " data=" << (int)msg.data[0] << "\n";
        }
    }
}
