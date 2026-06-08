// =============================================================================
//  CanSocketBridge.cpp
//  ---------------------------------------------------------------------------
//  Implementatie van het automatisering / routing framework op Pi A.
//
//  Flow:
//      CAN bus  --->  canRxLoop()  --->  shouldForward()
//                                   --->  routeCanIdToTarget()
//                                   --->  buildJson()
//                                   --->  TCP send naar Pi B
//
//      Pi B  --->  socketRxLoop()  --->  handleIncomingJson()
//                                   --->  sendCanFrame()  --->  CAN bus
//
//  Alle routing-beslissingen leven in de switch-case blokken hieronder, zodat
//  een nieuw apparaat / message-id alleen vereist:
//      1. een CanId enum waarde toevoegen in CanSocketBridge.h
//      2. een case in shouldForward() + routeCanIdToTarget()
//      3. (optioneel) een case in handleIncomingJson() voor Wemos->CAN
//
//  Conventies uit het busprotocol:
//      - Wemos modules hangen NIET direct aan de bus. De Pi vertaalt voor hen.
//      - 0x4xx zijn van de Pi zelf -> deze plaatsen we op de bus wanneer een
//        Wemos data opstuurt (bv. LDR waarde, relaxstoel status).
//      - 0x140 / 0x150-0x170 / 0x180-0x101 hebben bestemming "Pi", dus die
//        moeten we vanaf de bus doorsturen naar de juiste Wemos.
//      - 0x001 (nood) en 0x410/0x420 (dag/nacht) zijn broadcast.
// =============================================================================
#include "CanSocketBridge.h"

#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can/raw.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>

// NOTE: integreert met de bestaande cJSON helper in Socket/protocol/.
#include "../Socket/protocol/cJSON.h"

namespace pia {

// =============================================================================
// LIFECYCLE
// =============================================================================
CanSocketBridge::CanSocketBridge() = default;
CanSocketBridge::~CanSocketBridge() { stop(); }

bool CanSocketBridge::start() {
    if (running_) return true;
    if (!openCan())    { std::cerr << "[bridge] CAN open faalde\n"; return false; }
    if (!openSocket()) { std::cerr << "[bridge] socket open faalde (retry later)\n"; }
    running_ = true;
    canThread_  = std::thread(&CanSocketBridge::canRxLoop,    this);
    sockThread_ = std::thread(&CanSocketBridge::socketRxLoop, this);
    return true;
}

void CanSocketBridge::stop() {
    running_ = false;
    if (canThread_.joinable())  canThread_.join();
    if (sockThread_.joinable()) sockThread_.join();
    closeCan();
    closeSocket();
}

// =============================================================================
// RICHTING 1: CAN -> SOCKET (-> Pi B -> Wemos)
// =============================================================================
void CanSocketBridge::canRxLoop() {
    struct can_frame frame {};
    while (running_) {
        ssize_t n = ::read(canFd_, &frame, sizeof(frame));
        if (n != sizeof(frame)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        uint32_t id = frame.can_id & CAN_EFF_MASK;

        if (!shouldForward(id)) continue;

        const char* target = routeCanIdToTarget(id);
        std::string json   = buildJson(id, frame.data, frame.can_dlc, target);

        if (sockFd_ < 0 && !openSocket()) continue;

        // Length-prefixed framing zodat Pi B's parser simpel kan blijven
        uint32_t len = htonl(static_cast<uint32_t>(json.size()));
        if (::send(sockFd_, &len,        sizeof(len),  MSG_NOSIGNAL) <= 0 ||
            ::send(sockFd_, json.data(), json.size(),  MSG_NOSIGNAL) <= 0) {
            std::cerr << "[bridge] socket send faalde, reconnect\n";
            closeSocket();
        }
    }
}

// ----- Filter: welke message IDs moeten van de bus naar een Wemos? -----
//
//  Volgens de eigenaar-tabel zijn de IDs met bestemming "Pi" degene die
//  wij moeten doorzetten naar Wemos. Daarnaast broadcast (0x001/0x410/0x420).
//
bool CanSocketBridge::shouldForward(uint32_t canId) {
    switch (canId) {
        // Broadcast -> elke Wemos moet dit weten
        case CAN_ID_EMERGENCY:
        case CAN_ID_PI_DAYMODE:
        case CAN_ID_PI_NIGHTMODE:
            return true;

        // Bestemming "Pi": doorsturen naar de juiste Wemos
        case CAN_ID_BALIE_RELAX_SWITCH:     // 0x140 -> relaxstoel
        case CAN_ID_BALIE_EMERGTEXT_1:      // 0x150 -> lichtkrant
        case CAN_ID_BALIE_EMERGTEXT_2:      // 0x160 -> lichtkrant
        case CAN_ID_BALIE_EMERGTEXT_3:      // 0x170 -> lichtkrant
        case CAN_ID_BALIE_TEXT_1:           // 0x180 -> lichtkrant
        case CAN_ID_BALIE_TEXT_2:           // 0x190 -> lichtkrant
        case CAN_ID_BALIE_TEXT_3:           // 0x101 -> lichtkrant
            return true;

        // RFID UID is voor Pi & Balie -> Pi mag dit ook gebruiken intern,
        // maar er is (nog) geen Wemos bestemming. Niet forwarden.
        case CAN_ID_LUCHTSLUIS_RFID_UID:
            return false;

        // Berichten die NIET bij een Wemos horen (Balie/Luchtsluis interne
        // communicatie via de CAN-bus zelf):
        case CAN_ID_BALIE_LED_COLOR:        // direct naar Lichtvoorziening MC
        case CAN_ID_BALIE_LED_BRIGHTNESS:   // idem
        case CAN_ID_BALIE_LOCK_OPEN_ALL:    // direct naar Luchtsluis MC
        case CAN_ID_BALIE_DECON_CYCLE:      // idem
        case CAN_ID_LUCHTSLUIS_TEMP:        // -> Balie
        case CAN_ID_LUCHTSLUIS_EMERGBTN:    // -> Balie
        case CAN_ID_LICHT_CO2:              // -> Balie
            return false;

        // Onze eigen Pi-IDs horen niet door ons gefilterd te worden
        case CAN_ID_PI_LDR_VALUE:
        case CAN_ID_PI_RELAX_STATUS:
            return false;

        default:
            return false;
    }
}

// ----- Routing: welke Wemos hoort bij deze CAN ID? -----
const char* CanSocketBridge::routeCanIdToTarget(uint32_t canId) {
    switch (canId) {
        // Broadcast naar alle Wemos modules
        case CAN_ID_EMERGENCY:
        case CAN_ID_PI_DAYMODE:
        case CAN_ID_PI_NIGHTMODE:
            return TARGET_BROADCAST;

        // Relaxstoel Wemos
        case CAN_ID_BALIE_RELAX_SWITCH:
            return TARGET_RELAXSTOEL;

        // Lichtkrant Wemos (zowel normale tekst als noodtekst, 3 delen elk)
        case CAN_ID_BALIE_EMERGTEXT_1:
        case CAN_ID_BALIE_EMERGTEXT_2:
        case CAN_ID_BALIE_EMERGTEXT_3:
        case CAN_ID_BALIE_TEXT_1:
        case CAN_ID_BALIE_TEXT_2:
        case CAN_ID_BALIE_TEXT_3:
            return TARGET_LICHTKRANT;

        default:
            return TARGET_UNKNOWN;
    }
}

// ----- Encode CAN frame als JSON voor de Wemos -----
std::string CanSocketBridge::buildJson(uint32_t canId,
                                       const uint8_t* data,
                                       uint8_t dlc,
                                       const char* target) {
    std::ostringstream hex;
    for (uint8_t i = 0; i < dlc; ++i)
        hex << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(data[i]);

    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, JSON_FIELD_CAN_ID,  canId);
    cJSON_AddStringToObject(root, JSON_FIELD_PAYLOAD, hex.str().c_str());
    cJSON_AddStringToObject(root, JSON_FIELD_TARGET,  target);

    char* raw = cJSON_PrintUnformatted(root);
    std::string out(raw);
    free(raw);
    cJSON_Delete(root);
    return out;
}

// =============================================================================
// RICHTING 2: SOCKET (Pi B / Wemos) -> CAN
// =============================================================================
void CanSocketBridge::socketRxLoop() {
    while (running_) {
        if (sockFd_ < 0) {
            if (!openSocket()) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(SOCKET_RECONNECT_MS));
                continue;
            }
        }

        uint32_t lenNet = 0;
        ssize_t n = ::recv(sockFd_, &lenNet, sizeof(lenNet), MSG_WAITALL);
        if (n <= 0) { closeSocket(); continue; }

        uint32_t len = ntohl(lenNet);
        if (len == 0 || len > 4096) { closeSocket(); continue; }

        std::string buf(len, '\0');
        n = ::recv(sockFd_, buf.data(), len, MSG_WAITALL);
        if (n <= 0) { closeSocket(); continue; }

        handleIncomingJson(buf);
    }
}

// Een Wemos mag alleen IDs op de bus zetten die wettelijk "van de Pi" zijn,
// want de Pi is de eigenaar/zender namens die Wemos.
bool CanSocketBridge::isPiOwnedId(uint32_t canId) {
    switch (canId) {
        case CAN_ID_PI_LDR_VALUE:
        case CAN_ID_PI_DAYMODE:
        case CAN_ID_PI_NIGHTMODE:
        case CAN_ID_PI_RELAX_STATUS:
            return true;
        default:
            return false;
    }
}

void CanSocketBridge::handleIncomingJson(const std::string& jsonStr) {
    cJSON* root = cJSON_Parse(jsonStr.c_str());
    if (!root) return;

    cJSON* idItem      = cJSON_GetObjectItem(root, JSON_FIELD_CAN_ID);
    cJSON* payloadItem = cJSON_GetObjectItem(root, JSON_FIELD_PAYLOAD);
    if (!cJSON_IsNumber(idItem) || !cJSON_IsString(payloadItem)) {
        cJSON_Delete(root); return;
    }

    uint32_t canId = static_cast<uint32_t>(idItem->valuedouble);
    const std::string hex = payloadItem->valuestring;

    uint8_t data[8] = {0};
    uint8_t dlc = 0;
    for (size_t i = 0; i + 1 < hex.size() && dlc < 8; i += 2, ++dlc) {
        data[dlc] = static_cast<uint8_t>(
            std::stoi(hex.substr(i, 2), nullptr, 16));
    }

    // --- Beslissen wat er met elk ID vanuit de Wemos gebeurt ---
    switch (canId) {
        // Normale Pi-owned telemetrie/commando's: plaatsen op de bus
        case CAN_ID_PI_LDR_VALUE:        // LDR meting -> Balie
        case CAN_ID_PI_RELAX_STATUS:     // Status relaxstoel -> Balie
        case CAN_ID_PI_DAYMODE:          // Dag modus broadcast
        case CAN_ID_PI_NIGHTMODE:        // Nacht modus broadcast
            sendCanFrame(canId, data, dlc);
            break;

        // Een Wemos mag GEEN balie/luchtsluis/licht IDs op de bus zetten:
        case CAN_ID_EMERGENCY:
        case CAN_ID_BALIE_LED_COLOR:
        case CAN_ID_BALIE_LED_BRIGHTNESS:
        case CAN_ID_BALIE_LOCK_OPEN_ALL:
        case CAN_ID_BALIE_DECON_CYCLE:
        case CAN_ID_BALIE_RELAX_SWITCH:
        case CAN_ID_BALIE_EMERGTEXT_1:
        case CAN_ID_BALIE_EMERGTEXT_2:
        case CAN_ID_BALIE_EMERGTEXT_3:
        case CAN_ID_BALIE_TEXT_1:
        case CAN_ID_BALIE_TEXT_2:
        case CAN_ID_BALIE_TEXT_3:
        case CAN_ID_LUCHTSLUIS_TEMP:
        case CAN_ID_LUCHTSLUIS_EMERGBTN:
        case CAN_ID_LUCHTSLUIS_RFID_UID:
        case CAN_ID_LICHT_CO2:
            std::cerr << "[bridge] geweigerd: Wemos mag ID 0x"
                      << std::hex << canId << " niet op de bus zetten\n";
            break;

        default:
            // Fallback: alleen toestaan als het echt een Pi-owned ID is
            if (isPiOwnedId(canId)) {
                sendCanFrame(canId, data, dlc);
            } else {
                std::cerr << "[bridge] onbekende CAN ID van Wemos: 0x"
                          << std::hex << canId << "\n";
            }
            break;
    }

    cJSON_Delete(root);
}

bool CanSocketBridge::sendCanFrame(uint32_t canId,
                                   const uint8_t* data,
                                   uint8_t dlc) {
    if (canFd_ < 0) return false;
    if (dlc > 8) dlc = 8;

    struct can_frame frame {};
    frame.can_id  = canId & CAN_SFF_MASK;   // 11-bit standaard ID
    frame.can_dlc = dlc;
    std::memcpy(frame.data, data, dlc);

    ssize_t n = ::write(canFd_, &frame, sizeof(frame));
    if (n != sizeof(frame)) {
        std::cerr << "[bridge] CAN write faalde voor ID 0x"
                  << std::hex << canId << "\n";
        return false;
    }
    return true;
}

// =============================================================================
// CONNECTIE HELPERS
// =============================================================================
bool CanSocketBridge::openCan() {
    canFd_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (canFd_ < 0) return false;

    struct ifreq ifr {};
    std::strncpy(ifr.ifr_name, CAN_INTERFACE, IFNAMSIZ - 1);
    if (::ioctl(canFd_, SIOCGIFINDEX, &ifr) < 0) { closeCan(); return false; }

    struct sockaddr_can addr {};
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (::bind(canFd_, reinterpret_cast<struct sockaddr*>(&addr),
               sizeof(addr)) < 0) {
        closeCan();
        return false;
    }
    return true;
}

void CanSocketBridge::closeCan() {
    if (canFd_ >= 0) ::close(canFd_);
    canFd_ = -1;
}

bool CanSocketBridge::openSocket() {
    sockFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sockFd_ < 0) return false;

    struct sockaddr_in srv {};
    srv.sin_family = AF_INET;
    srv.sin_port   = htons(PI_B_PORT);
    if (::inet_pton(AF_INET, PI_B_HOST, &srv.sin_addr) <= 0) {
        closeSocket(); return false;
    }
    if (::connect(sockFd_, reinterpret_cast<struct sockaddr*>(&srv),
                  sizeof(srv)) < 0) {
        closeSocket(); return false;
    }
    return true;
}

void CanSocketBridge::closeSocket() {
    if (sockFd_ >= 0) ::close(sockFd_);
    sockFd_ = -1;
}

} // namespace pia

// =============================================================================
// Optionele standalone main (compile met -DBRIDGE_STANDALONE)
// =============================================================================
#ifdef BRIDGE_STANDALONE
int main() {
    pia::CanSocketBridge bridge;
    if (!bridge.start()) return 1;
    while (true) std::this_thread::sleep_for(std::chrono::seconds(1));
}
#endif
