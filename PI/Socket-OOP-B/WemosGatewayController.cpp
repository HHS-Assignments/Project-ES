#include "WemosGatewayController.h"
#include <iostream>

WemosGatewayController& WemosGatewayController::getInstance() {
    static WemosGatewayController instance;
    return instance;
}

void WemosGatewayController::init(Communication *piSocket, Communication *wemos,
                                  DataParser *parser) {
    _piSocket = piSocket;
    _wemos    = wemos;
    _parser   = parser;
}

bool WemosGatewayController::begin() {
    if (!_wemos->begin()) {
        std::cerr << "[B] Wemos-server starten mislukt\n";
        return false;
    }
    if (!_piSocket->begin()) {
        std::cerr << "[B] Pi A-verbinding initialiseren mislukt\n";
        _wemos->stop();
        return false;
    }
    return true;
}

void WemosGatewayController::run() {
    _running = true;
    _piThread = std::thread(&WemosGatewayController::_piNaarWemos, this);
    _wemosNaarPi();  // blokkeert in de hoofdthread
    stop();
}

void WemosGatewayController::stop() {
    if (!_running.exchange(false)) return;
    _piSocket->stop();
    _wemos->stop();
    if (_piThread.joinable()) _piThread.join();
}

// Pi A -> fan-out naar alle Wemos'es.
// De fan-out gebeurt in een losse thread per bericht zodat een trage of
// onbereikbare Wemos het lezen van Pi A niet blokkeert.
void WemosGatewayController::_piNaarWemos() {
    std::string line;
    while (_running) {
        if (!_piSocket->receive(line)) break;
        if (!_parser->isValid(line)) {
            std::cerr << "[B] ongeldige JSON van Pi A: " << line << "\n";
            continue;
        }
        std::cout << "[B] A-> " << line << "\n";
        std::thread([this, line] { _wemos->send(line); }).detach();
    }
}

// Wemos -> Pi A
void WemosGatewayController::_wemosNaarPi() {
    std::string line;
    while (_running) {
        if (!_wemos->receive(line)) break;
        if (!_parser->isValid(line)) {
            std::cerr << "[B] ongeldige JSON van Wemos: " << line << "\n";
            continue;
        }
        if (!_piSocket->send(line)) {
            std::cerr << "[B] Pi A niet verbonden; bericht weggegooid\n";
        }
    }
}
