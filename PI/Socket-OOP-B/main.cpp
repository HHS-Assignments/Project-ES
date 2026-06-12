// Pi B — TCP router tussen Pi A en de Wemos-apparaten (OOP-versie)
// Usage:  ./pi_b <piA_ip> <piA_port> <web_port>
// Build:  g++ -O2 -std=c++17 -pthread main.cpp PiSocketClient.cpp WemosCommunication.cpp DataParser.cpp WemosGatewayController.cpp -o pi_b

#include <cstdio>
#include <cstdlib>

#include "PiSocketClient.h"
#include "WemosCommunication.h"
#include "DataParser.h"
#include "WemosGatewayController.h"

int main(int argc, char **argv) {
    if (argc < 4) {
        std::fprintf(stderr, "Usage: %s <piA_ip> <piA_port> <web_port>\n", argv[0]);
        return 1;
    }
    std::string piIp = argv[1];
    uint16_t piPort  = (uint16_t)std::atoi(argv[2]);
    uint16_t webPort = (uint16_t)std::atoi(argv[3]);

    // Bekende Wemos-doelen (poort = waar de Wemos WiFiServer op luistert)
    std::vector<WemosTarget> targets = {
        {"wemos_relaxstoel", "10.42.0.101", 9001},
        {"wemos_lichtkrant", "10.42.0.100", 9001},
    };

    PiSocketClient     piSocket(piIp, piPort);
    WemosCommunication wemos(webPort, targets);
    DataParser         parser;

    WemosGatewayController &gw = WemosGatewayController::getInstance();
    gw.init(&piSocket, &wemos, &parser);
    if (!gw.begin()) return 1;

    gw.run();
    return 0;
}
