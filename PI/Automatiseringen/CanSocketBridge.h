// =============================================================================
//  CanSocketBridge.h
//  ---------------------------------------------------------------------------
//  Automatiseringslaag op Raspberry Pi A.
//
//  Verantwoordelijkheden:
//    * Lees CAN frames van het lokale CAN interface (SocketCAN, can0).
//    * Filter op Message ID volgens het busprotocol van het ES project
//      (zie "Documentatie Canbus", Kai Diemel).
//    * Forward de relevante frames over de TCP/JSON socket naar Pi B,
//      die als Wi-Fi AP de Wemos modules bedient.
//    * Ontvang JSON berichten van Pi B (afkomstig van een Wemos) en
//      vertaal ze terug naar CAN frames op de bus.
//
//  Owner-tabel (samenvatting uit de documentatie):
//    Balie         : 0x001, 0x100, 0x110, 0x120, 0x130, 0x140,
//                    0x150, 0x160, 0x170, 0x180, 0x190, 0x101
//    Luchtsluis    : 0x200, 0x210, 0x220
//    Lichtvoorz.   : 0x300
//    Pi (deze!)    : 0x400, 0x410, 0x420, 0x430
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
// CONFIGURATIE CONSTANTEN  (op één plek aanpasbaar)
// -----------------------------------------------------------------------------
constexpr const char* CAN_INTERFACE          = "can0";
constexpr const char* PI_B_HOST              = "192.168.4.1"; // Pi B AP IP
constexpr uint16_t    PI_B_PORT              = 5000;
constexpr int         SOCKET_RECONNECT_MS    = 1000;
constexpr int         CAN_READ_TIMEOUT_MS    = 100;

// -----------------------------------------------------------------------------
// JSON FIELD NAMES (contract met Wemos firmware)
//   { "id": <can_id>, "data": "<hex_payload>", "target": "<wemos_name>" }
// -----------------------------------------------------------------------------
constexpr const char* JSON_FIELD_CAN_ID      = "id";
constexpr const char* JSON_FIELD_PAYLOAD     = "data";
constexpr const char* JSON_FIELD_TARGET      = "target";

// -----------------------------------------------------------------------------
// CAN MESSAGE ID's  (zie busprotocol-documentatie)
// -----------------------------------------------------------------------------
enum CanId : uint32_t {
    // --- Globaal / broadcast ---
    CAN_ID_EMERGENCY              = 0x001, // Noodsituatie -> alle ontvangers

    // --- Eigenaar: Balie (reception desk MC) ---
    CAN_ID_BALIE_LED_COLOR        = 0x100, // LED kleur lichtvoorziening
    CAN_ID_BALIE_LED_BRIGHTNESS   = 0x110, // Helderheid leds
    CAN_ID_BALIE_LOCK_OPEN_ALL    = 0x120, // Alle deuren open (luchtsluis)
    CAN_ID_BALIE_DECON_CYCLE      = 0x130, // Decontaminatie cyclus buiten->binnen
    CAN_ID_BALIE_RELAX_SWITCH     = 0x140, // Switch relaxstoel status (->Pi)
    CAN_ID_BALIE_EMERGTEXT_1      = 0x150, // Noodtekst lichtkrant deel 1 (->Pi)
    CAN_ID_BALIE_EMERGTEXT_2      = 0x160, // Noodtekst lichtkrant deel 2 (->Pi)
    CAN_ID_BALIE_EMERGTEXT_3      = 0x170, // Noodtekst lichtkrant deel 3 (->Pi)
    CAN_ID_BALIE_TEXT_1           = 0x180, // Tekst lichtkrant deel 1     (->Pi)
    CAN_ID_BALIE_TEXT_2           = 0x190, // Tekst lichtkrant deel 2     (->Pi)
    CAN_ID_BALIE_TEXT_3           = 0x101, // Tekst lichtkrant deel 3     (->Pi)

    // --- Eigenaar: Luchtsluis ---
    CAN_ID_LUCHTSLUIS_TEMP        = 0x200, // Temperatuur sensor data
    CAN_ID_LUCHTSLUIS_EMERGBTN    = 0x210, // Noodknop status
    CAN_ID_LUCHTSLUIS_RFID_UID    = 0x220, // RFID UID (Pi & Balie)

    // --- Eigenaar: Lichtvoorziening ---
    CAN_ID_LICHT_CO2              = 0x300, // CO2 waardes

    // --- Eigenaar: Pi (deze node) ---
    CAN_ID_PI_LDR_VALUE           = 0x400, // LDR waarde van Wemos -> Balie
    CAN_ID_PI_DAYMODE             = 0x410, // Dag modus -> alle ontvangers
    CAN_ID_PI_NIGHTMODE           = 0x420, // Nacht modus -> alle ontvangers
    CAN_ID_PI_RELAX_STATUS        = 0x430  // Status relaxstoel -> Balie
};

// -----------------------------------------------------------------------------
// WEMOS TARGET NAMES (gebruikt in JSON "target" veld)
//   - relaxstoel : ontvangt 0x140 (switch)
//   - lichtkrant : ontvangt 0x150-0x170 (nood) + 0x180/0x190/0x101 (normaal)
//   - broadcast  : voor nood / dag-nacht modus die naar elke Wemos moet
// -----------------------------------------------------------------------------
constexpr const char* TARGET_RELAXSTOEL  = "wemos_relaxstoel";
constexpr const char* TARGET_LICHTKRANT  = "wemos_lichtkrant";
constexpr const char* TARGET_BROADCAST   = "broadcast";
constexpr const char* TARGET_UNKNOWN     = "unknown";

// -----------------------------------------------------------------------------
// BRIDGE CLASS
// -----------------------------------------------------------------------------
class CanSocketBridge {
public:
    CanSocketBridge();
    ~CanSocketBridge();

    bool start();   // open CAN + socket en start worker threads
    void stop();

private:
    // --- Richting 1: CAN bus -> Pi B -> Wemos ---
    void canRxLoop();
    bool shouldForward(uint32_t canId);
    const char* routeCanIdToTarget(uint32_t canId);
    std::string buildJson(uint32_t canId, const uint8_t* data, uint8_t dlc,
                          const char* target);

    // --- Richting 2: Pi B -> CAN bus ---
    void socketRxLoop();
    void handleIncomingJson(const std::string& jsonStr);
    bool isPiOwnedId(uint32_t canId);
    bool sendCanFrame(uint32_t canId, const uint8_t* data, uint8_t dlc);

    // --- Connectie helpers ---
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
