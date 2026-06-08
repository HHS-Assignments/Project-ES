// =============================================================================
//  CanSocketBridge.h
//  ---------------------------------------------------------------------------
//  Framework for the automation layer on Raspberry Pi A.
//
//  Responsibilities:
//    * Read CAN frames from the local CAN interface (SocketCAN).
//    * Filter them by CAN-ID.
//    * Forward the relevant frames over the TCP/JSON socket to Pi B,
//      which acts as a Wi-Fi AP for the Wemos modules.
//    * Receive JSON messages from Pi B (originating from a Wemos) and
//      translate them back into CAN frames on the bus.
//
//  Project: ES - Smart Office (HHS)
//  Target : Raspberry Pi A
// =============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <atomic>
#include <thread>
#include <linux/can.h>

namespace pia {

// -----------------------------------------------------------------------------
// CONFIGURATION CONSTANTS  (tweak these in one place)
// -----------------------------------------------------------------------------
constexpr const char* CAN_INTERFACE          = "can0";
constexpr const char* PI_B_HOST              = "192.168.4.1"; // Pi B AP IP
constexpr uint16_t    PI_B_PORT              = 5000;
constexpr int         SOCKET_RECONNECT_MS    = 1000;
constexpr int         CAN_READ_TIMEOUT_MS    = 100;

// -----------------------------------------------------------------------------
// JSON FIELD NAMES (matches Wemos firmware contract)
//   { "id": <can_id>, "data": "<hex_payload>", "target": "<wemos_name>" }
// -----------------------------------------------------------------------------
constexpr const char* JSON_FIELD_CAN_ID      = "id";
constexpr const char* JSON_FIELD_PAYLOAD     = "data";
constexpr const char* JSON_FIELD_TARGET      = "target";

// -----------------------------------------------------------------------------
// CAN ID MAP  (high-level meaning of every ID on the bus)
//   - Bits 0-3 : device type
//   - Bits 4-7 : function
//   (Adjust to the actual ES project ID-plan.)
// -----------------------------------------------------------------------------
enum CanId : uint32_t {
    // --- Balieconsole (reception desk MC) ---
    CAN_ID_BALIE_RELAX_CTRL      = 0x101,  // turn relax chair on/off
    CAN_ID_BALIE_LIGHT_CTRL      = 0x102,  // light control
    CAN_ID_BALIE_DOOR_CTRL       = 0x103,  // door control

    // --- Relaxstoel (Wemos: relax) ---
    CAN_ID_RELAX_STATUS          = 0x201,
    CAN_ID_RELAX_PRESENCE        = 0x202,

    // --- Verlichting (Wemos: light) ---
    CAN_ID_LIGHT_STATUS          = 0x301,
    CAN_ID_LIGHT_DIM             = 0x302,

    // --- Deur (Wemos: door) ---
    CAN_ID_DOOR_STATUS           = 0x401,
    CAN_ID_DOOR_LOCK             = 0x402,

    // --- Klimaat (Wemos: climate) ---
    CAN_ID_CLIMATE_TEMP          = 0x501,
    CAN_ID_CLIMATE_SETPOINT      = 0x502
};

// -----------------------------------------------------------------------------
// WEMOS TARGET NAMES (used in JSON "target" field)
// -----------------------------------------------------------------------------
constexpr const char* TARGET_RELAX    = "wemos_relax";
constexpr const char* TARGET_LIGHT    = "wemos_light";
constexpr const char* TARGET_DOOR     = "wemos_door";
constexpr const char* TARGET_CLIMATE  = "wemos_climate";
constexpr const char* TARGET_UNKNOWN  = "unknown";

// -----------------------------------------------------------------------------
// BRIDGE CLASS
// -----------------------------------------------------------------------------
class CanSocketBridge {
public:
    CanSocketBridge();
    ~CanSocketBridge();

    bool start();   // open CAN + socket and spin worker threads
    void stop();

private:
    // --- Direction: CAN bus -> Pi B ---
    void canRxLoop();
    bool shouldForward(uint32_t canId);
    const char* routeCanIdToTarget(uint32_t canId);
    std::string buildJson(uint32_t canId, const uint8_t* data, uint8_t dlc,
                          const char* target);

    // --- Direction: Pi B -> CAN bus ---
    void socketRxLoop();
    void handleIncomingJson(const std::string& jsonStr);
    bool sendCanFrame(uint32_t canId, const uint8_t* data, uint8_t dlc);

    // --- Connection helpers ---
    bool openCan();
    void closeCan();
    bool openSocket();
    void closeSocket();

    int  canFd_     = -1;
    int  sockFd_    = -1;
    std::atomic<bool> running_{false};
    std::thread canThread_;
    std::thread sockThread_;
};

} // namespace pia
