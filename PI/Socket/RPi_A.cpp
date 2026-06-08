/**
 * RPi_A - TCP Receiver/Dispatcher + CAN Bridge
 * ---------------------------------------------------------------------------
 *  Drie fixes t.o.v. de PoC versie:
 *
 *   (1) CanSocketBridge wordt opgestart in main() zodat Pi A actief CAN-
 *       frames naar Pi B (en daarmee naar de Wemos modules) duwt.
 *
 *   (2) De parser herkent NU twee JSON-vormen:
 *         a) "klassiek"  : { "device": "...", "sensor": "...", "data": "..." }
 *         b) "CAN-frame" : { "id": <num>, "data": "<hex>", "target": "..." }
 *       Vorm (b) is wat de Wemos en de bridge produceren. Voor deze vorm
 *       slaan we de oude DeviceDispatcher (gericht op "device" veld) over en
 *       gebruiken we een aparte CAN-handler die het frame eventueel op de
 *       lokale CAN-bus zet via de bridge.
 *
 *   (3) ACK is optioneel gemaakt. Pi B draait nu fire-and-forget en luistert
 *       niet meer op een antwoord; we sturen alleen nog een ACK terug
 *       wanneer de inkomende JSON een "ack":true veld bevat. Dit elimineert
 *       de race condition op de Pi B socket waarbij twee threads van dezelfde
 *       upstream zouden lezen.
 * ---------------------------------------------------------------------------
 */

#include "protocol/JsonMessage.h"
#include "protocol/cJSON.h"
#include "dispatch/IDeviceHandler.h"
#include "dispatch/WmosHandler.h"
#include "dispatch/UnknownHandler.h"
#include "dispatch/DeviceDispatcher.h"
#include "transport/SocketConnection.h"

// CAN bridge (zit in PI/Automatiseringen/)
#include "../Automatiseringen/CanSocketBridge.h"

#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace protocol;
using namespace transport;
using namespace dispatch;


// =============================================================================
// CONFIG  (op één plek aanpasbaar)
// =============================================================================
namespace cfg {
constexpr int  DEFAULT_TCP_PORT     = 5000;
constexpr int  RECV_BUFFER_SIZE     = 4096;

// JSON velden voor de CAN-frame vorm (afspraak met Pi B / Wemos / bridge)
constexpr const char* FIELD_CAN_ID  = "id";
constexpr const char* FIELD_DATA    = "data";
constexpr const char* FIELD_TARGET  = "target";
constexpr const char* FIELD_ACK     = "ack";    // optioneel; true => ACK sturen
} // namespace cfg


// =============================================================================
// GLOBALS
// =============================================================================
// Een enkele bridge instance. Wordt in main() gestart. socketRxLoop() in de
// bridge luistert zelf naar Pi A's eigen socket richting Pi B, maar voor het
// op de bus zetten van een binnenkomend Wemos-bericht hergebruiken we hier
// direct de helpers via een vriend-vrije route: we publiceren een minimal API.
static std::unique_ptr<pia::CanSocketBridge> g_bridge;


// =============================================================================
// LOGGING HANDLER (ongewijzigd, voor de klassieke "device" vorm)
// =============================================================================
class LoggingHandler : public IDeviceHandler {
public:
    explicit LoggingHandler(std::string device) : device_(std::move(device)) {}
    void handle(const JsonMessage& message) override {
        std::cout << "[LoggingHandler] Device: " << message.getDevice()
                  << " | Sensor: "               << message.getSensor()
                  << " | Data: "                 << message.getData() << std::endl;
    }
private:
    std::string device_;
};


// =============================================================================
// FIX (2) - CAN-FRAME PARSER PAD
// -----------------------------------------------------------------------------
//  Detecteert of een JSON string het {id,data,target} formaat heeft en
//  verwerkt het direct, los van de oude DeviceDispatcher.
// =============================================================================
struct CanFrameJson {
    uint32_t    id   = 0;
    std::string dataHex;
    std::string target;
};

static bool tryParseCanFrame(const std::string& jsonStr, CanFrameJson& out) {
    cJSON* root = cJSON_Parse(jsonStr.c_str());
    if (!root) return false;

    cJSON* idItem     = cJSON_GetObjectItem(root, cfg::FIELD_CAN_ID);
    cJSON* dataItem   = cJSON_GetObjectItem(root, cfg::FIELD_DATA);
    cJSON* targetItem = cJSON_GetObjectItem(root, cfg::FIELD_TARGET);

    // Minimale eis: een numeriek "id" veld
    if (!idItem || !cJSON_IsNumber(idItem)) {
        cJSON_Delete(root);
        return false;
    }

    out.id = static_cast<uint32_t>(idItem->valuedouble);

    if (dataItem && cJSON_IsString(dataItem) && dataItem->valuestring)
        out.dataHex = dataItem->valuestring;

    if (targetItem && cJSON_IsString(targetItem) && targetItem->valuestring)
        out.target = targetItem->valuestring;

    cJSON_Delete(root);
    return true;
}

// Decodeer hex string "deadbeef" -> bytes[]. Max 8 bytes (CAN DLC limiet).
static uint8_t hexToBytes(const std::string& hex, uint8_t* out, uint8_t maxLen) {
    uint8_t n = 0;
    for (size_t i = 0; i + 1 < hex.size() && n < maxLen; i += 2) {
        auto hexNibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
            if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
            return -1;
        };
        int hi = hexNibble(hex[i]);
        int lo = hexNibble(hex[i + 1]);
        if (hi < 0 || lo < 0) break;
        out[n++] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return n;
}

// Behandel een CAN-frame JSON. Op dit moment loggen we en (optioneel) zetten
// we het op de eigen CAN-bus als het een Pi-owned ID is. Uitbreiding: route
// naar interne logica (alarm, dag/nacht modus, etc.).
static void handleCanFrame(const CanFrameJson& f) {
    std::cout << "[CAN-RX] id=0x" << std::hex << f.id << std::dec
              << " target='" << f.target << "'"
              << " data=" << f.dataHex << std::endl;

    // Voorbeeld doorvertaling: alles wat van een Wemos komt en bij Pi hoort
    // (0x4xx) zou de bridge zelf al op de bus zetten via zijn eigen
    // socketRxLoop. Hier is dus expliciet GEEN tweede pad nodig - dit blok
    // dient als hook voor toekomstige automatisering die Pi A intern wil
    // uitvoeren op basis van inkomende frames.
    //
    // (void) hexToBytes(...) als je hier zelf bytes wilt verwerken.
    (void)hexToBytes;
}


// =============================================================================
// CLIENT HANDLER
// =============================================================================
//  FIX (3): ACK is optioneel - alleen sturen als de afzender erom vraagt.
//           Dit voorkomt dat Pi B's HTTP-thread en upstream-rx-thread elkaar
//           in de wielen rijden bij het lezen van de socket.
// =============================================================================
void handle_rpia_client(int client_fd, DeviceDispatcher* dispatcher)
{
    char buffer[cfg::RECV_BUFFER_SIZE];
    std::cout << "[RPi_A] Client connected\n";

    // Buffer voor regel-gebaseerde framing (\n terminator)
    std::string accum;

    while (true) {
        std::memset(buffer, 0, sizeof(buffer));
        ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) {
            std::cout << "[RPi_A] Client disconnected\n";
            break;
        }
        accum.append(buffer, n);

        // Verwerk volledige regels
        size_t pos;
        while ((pos = accum.find('\n')) != std::string::npos) {
            std::string line = accum.substr(0, pos);
            accum.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            std::cout << "[RPi_A] RX: " << line << std::endl;

            bool ackRequested = false;
            std::string ackPayload;
            bool handled = false;

            // --- FIX (2): probeer eerst het CAN-frame formaat ---
            CanFrameJson frame;
            if (tryParseCanFrame(line, frame)) {
                handleCanFrame(frame);
                ackPayload = "{\"status\":\"OK\",\"id\":" +
                             std::to_string(frame.id) + "}";
                handled = true;
            } else {
                // --- Fallback: klassieke device/sensor/data vorm ---
                JsonMessage msg;
                if (msg.parse(line)) {
                    dispatcher->dispatch(msg);
                    ackPayload = "{\"status\":\"OK\",\"device\":\"" +
                                 msg.getDevice() + "\"}";
                    handled = true;
                } else {
                    std::cerr << "[RPi_A] Parse failed voor regel\n";
                    ackPayload = "{\"status\":\"ERROR\",\"reason\":\"parse\"}";
                }
            }

            // Detecteer of de afzender expliciet een ACK wil
            cJSON* root = cJSON_Parse(line.c_str());
            if (root) {
                cJSON* a = cJSON_GetObjectItem(root, cfg::FIELD_ACK);
                if (a && cJSON_IsBool(a) && cJSON_IsTrue(a)) ackRequested = true;
                cJSON_Delete(root);
            }

            // FIX (3): alleen ACK sturen op verzoek of bij parse error
            if (ackRequested || !handled) {
                std::string out = ackPayload + "\n";
                send(client_fd, out.data(), out.size(), MSG_NOSIGNAL);
            }
        }
    }

    close(client_fd);
}


// =============================================================================
// TCP SERVER (ongewijzigd qua structuur)
// =============================================================================
void tcp_server_thread(int port, DeviceDispatcher* dispatcher)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        std::cerr << "[RPi_A] socket() faalde\n";
        return;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[RPi_A] bind faalde op poort " << port << "\n";
        close(listen_fd);
        return;
    }
    if (listen(listen_fd, 10) < 0) {
        std::cerr << "[RPi_A] listen faalde\n";
        close(listen_fd);
        return;
    }

    std::cout << "[RPi_A] TCP server luistert op poort " << port << "\n";

    while (true) {
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            std::cerr << "[RPi_A] accept faalde\n";
            continue;
        }
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, INET_ADDRSTRLEN);
        std::cout << "[RPi_A] Nieuwe client van " << ip << "\n";

        std::thread(handle_rpia_client, client_fd, dispatcher).detach();
    }
    close(listen_fd);
}


// =============================================================================
// MAIN
// =============================================================================
int main(int argc, char* argv[])
{
    std::cout << "=== RPi_A - TCP Receiver + CAN Bridge ===\n";

    int tcp_port = cfg::DEFAULT_TCP_PORT;
    if (argc >= 2) tcp_port = std::stoi(argv[1]);

    // -------------------------------------------------------------------------
    // Klassieke dispatcher (blijft beschikbaar voor non-CAN device berichten)
    // -------------------------------------------------------------------------
    auto dispatcher      = std::make_unique<DeviceDispatcher>();
    auto wmos_handler    = std::make_unique<WmosHandler>("wmos_handler");
    auto unknown_handler = std::make_unique<UnknownHandler>();
    auto logging_handler = std::make_unique<LoggingHandler>("generic");

    dispatcher->registerHandler("wmos_device_1",      wmos_handler.get());
    dispatcher->registerHandler("wmos_device_2",      wmos_handler.get());
    dispatcher->registerHandler("temperature_sensor", logging_handler.get());
    dispatcher->registerHandler("motion_detector",    logging_handler.get());

    std::cout << "[main] " << dispatcher->getHandlerCount()
              << " device handlers geregistreerd\n";

    // -------------------------------------------------------------------------
    // FIX (1): start de CAN <-> Socket bridge
    // -------------------------------------------------------------------------
    std::cout << "[main] CanSocketBridge starten...\n";
    g_bridge = std::make_unique<pia::CanSocketBridge>();
    if (!g_bridge->start()) {
        std::cerr << "[main] WAARSCHUWING: CAN bridge kon niet starten "
                     "(geen can0?). Pi A draait door zonder CAN forwarding.\n";
    } else {
        std::cout << "[main] CAN bridge actief op " << pia::CAN_INTERFACE
                  << " -> Pi B (" << pia::PI_B_HOST << ":" << pia::PI_B_PORT
                  << ")\n";
    }

    // -------------------------------------------------------------------------
    // TCP server (voor inkomend verkeer van Pi B)
    // -------------------------------------------------------------------------
    std::cout << "[main] TCP server starten op poort " << tcp_port << "\n";
    std::thread(tcp_server_thread, tcp_port, dispatcher.get()).detach();

    std::cout << "[main] Klaar. Ctrl+C om te stoppen.\n";
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
