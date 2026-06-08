// =============================================================================
//  CanSocketBridge.cpp
//  ---------------------------------------------------------------------------
//  Implements the automation/routing framework on Pi A.
//
//  Flow:
//      CAN bus  --->  canRxLoop()  --->  shouldForward()
//                                   --->  routeCanIdToTarget()
//                                   --->  buildJson()
//                                   --->  TCP send to Pi B
//
//      Pi B  --->  socketRxLoop()  --->  handleIncomingJson()
//                                   --->  sendCanFrame()  --->  CAN bus
//
//  All routing decisions live inside the switch-case blocks below so a new
//  device only requires:
//      1. adding a CanId enum value in CanSocketBridge.h
//      2. adding a case in shouldForward() + routeCanIdToTarget()
//      3. (optional) adding a case in handleIncomingJson() for Wemos->CAN
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

// NOTE: integrate with the existing cJSON helper in Socket/protocol/.
#include "../Socket/protocol/cJSON.h"

namespace pia {

// =============================================================================
// LIFECYCLE
// =============================================================================
CanSocketBridge::CanSocketBridge() = default;

CanSocketBridge::~CanSocketBridge() { stop(); }

bool CanSocketBridge::start() {
    if (running_) return true;
    if (!openCan())    { std::cerr << "[bridge] CAN open failed\n"; return false; }
    if (!openSocket()) { std::cerr << "[bridge] socket open failed\n"; }
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
// DIRECTION 1: CAN -> SOCKET (-> Pi B -> Wemos)
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

        // Length-prefixed framing keeps Pi B's parser simple
        uint32_t len = htonl(static_cast<uint32_t>(json.size()));
        if (::send(sockFd_, &len,        sizeof(len),  MSG_NOSIGNAL) <= 0 ||
            ::send(sockFd_, json.data(), json.size(),  MSG_NOSIGNAL) <= 0) {
            std::cerr << "[bridge] socket send failed, reconnecting\n";
            closeSocket();
        }
    }
}

// ----- Filter: only IDs we know should leave the CAN bus -----
bool CanSocketBridge::shouldForward(uint32_t canId) {
    switch (canId) {
        case CAN_ID_BALIE_RELAX_CTRL:
        case CAN_ID_BALIE_LIGHT_CTRL:
        case CAN_ID_BALIE_DOOR_CTRL:
        case CAN_ID_LIGHT_DIM:
        case CAN_ID_DOOR_LOCK:
        case CAN_ID_CLIMATE_SETPOINT:
            return true;

        // Sensor/telemetry IDs stay on the bus by default
        case CAN_ID_RELAX_STATUS:
        case CAN_ID_RELAX_PRESENCE:
        case CAN_ID_LIGHT_STATUS:
        case CAN_ID_DOOR_STATUS:
        case CAN_ID_CLIMATE_TEMP:
            return false;

        default:
            return false;
    }
}

// ----- Routing: which Wemos owns this CAN ID -----
const char* CanSocketBridge::routeCanIdToTarget(uint32_t canId) {
    switch (canId) {
        case CAN_ID_BALIE_RELAX_CTRL:
            return TARGET_RELAX;

        case CAN_ID_BALIE_LIGHT_CTRL:
        case CAN_ID_LIGHT_DIM:
            return TARGET_LIGHT;

        case CAN_ID_BALIE_DOOR_CTRL:
        case CAN_ID_DOOR_LOCK:
            return TARGET_DOOR;

        case CAN_ID_CLIMATE_SETPOINT:
            return TARGET_CLIMATE;

        default:
            return TARGET_UNKNOWN;
    }
}

// ----- Encode CAN frame as JSON for the Wemos -----
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
// DIRECTION 2: SOCKET (Pi B / Wemos) -> CAN
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

    // --- Decide what to do with each ID coming back from the Wemos ---
    switch (canId) {
        case CAN_ID_RELAX_STATUS:
        case CAN_ID_RELAX_PRESENCE:
        case CAN_ID_LIGHT_STATUS:
        case CAN_ID_DOOR_STATUS:
        case CAN_ID_CLIMATE_TEMP:
            sendCanFrame(canId, data, dlc);
            break;

        case CAN_ID_BALIE_RELAX_CTRL:
        case CAN_ID_BALIE_LIGHT_CTRL:
        case CAN_ID_BALIE_DOOR_CTRL:
            // Control IDs should not normally originate from a Wemos.
            std::cerr << "[bridge] unexpected control ID from Wemos: 0x"
                      << std::hex << canId << "\n";
            break;

        default:
            std::cerr << "[bridge] unknown CAN ID from Wemos: 0x"
                      << std::hex << canId << "\n";
            break;
    }

    cJSON_Delete(root);
}

bool CanSocketBridge::sendCanFrame(uint32_t canId,
                                   const uint8_t* data,
                                   uint8_t dlc) {
    if (canFd_ < 0) return false;
    struct can_frame frame {};
    frame.can_id  = canId;
    frame.can_dlc = (dlc > 8) ? 8 : dlc;
    std::memcpy(frame.data, data, frame.can_dlc);
    return ::write(canFd_, &frame, sizeof(frame)) == sizeof(frame);
}

// =============================================================================
// CONNECTION HELPERS
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
               sizeof(addr)) < 0) { closeCan(); return false; }
    return true;
}

void CanSocketBridge::closeCan() {
    if (canFd_ >= 0) { ::close(canFd_); canFd_ = -1; }
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
    if (sockFd_ >= 0) { ::close(sockFd_); sockFd_ = -1; }
}

} // namespace pia

// =============================================================================
// OPTIONAL ENTRY POINT
// =============================================================================
#ifdef BRIDGE_STANDALONE
int main() {
    pia::CanSocketBridge bridge;
    if (!bridge.start()) return 1;
    while (true) std::this_thread::sleep_for(std::chrono::seconds(1));
}
#endif
