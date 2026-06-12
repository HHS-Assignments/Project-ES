// Pi A — CAN <-> TCP gateway (OOP-versie)
// Usage:  ./pi_a <listen_port> [can_iface=can0]
// Build:  g++ -O2 -std=c++17 -pthread main.cpp CAN.cpp PiSocketServer.cpp DataParser.cpp CanGatewayController.cpp -o pi_a

#include <cstdio>
#include <cstdlib>
#include <iostream>

#include "CAN.h"
#include "PiSocketServer.h"
#include "DataParser.h"
#include "CanGatewayController.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <listen_port> [can_iface=can0]\n", argv[0]);
        return 1;
    }
    uint16_t port  = (uint16_t)std::atoi(argv[1]);
    const char *iface = (argc >= 3) ? argv[2] : "can0";

    CAN            bus(iface);
    PiSocketServer server(port);
    DataParser     parser;

    CanGatewayController &gw = CanGatewayController::getInstance();
    gw.init(&bus, &server, &parser);
    if (!gw.begin()) return 1;

    std::cout << "[A] CAN=" << iface << "  luistert op :" << port << "\n";
    gw.run();
    return 0;
}
