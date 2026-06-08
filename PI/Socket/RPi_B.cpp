/**
 * RPi_B - HTTP Forwarder met DeviceRouter
 * ---------------------------------------------------------------------------
 *  Twee richtingen:
 *
 *    Pi A  --[ TCP socket, JSON line ]-->  Pi B  --[ HTTP POST /can ]-->  Wemos
 *    Wemos --[ HTTP POST :8080 ]-->        Pi B  --[ TCP socket ]-->      Pi A
 *
 *  Pi A stuurt JSON met daarin een "target" veld (bv. "wemos_relaxstoel").
 *  De DeviceRouter mapt die naam naar een IP en Pi B doet de HTTP POST.
 *  Speciale target "broadcast" stuurt naar alle geregistreerde Wemos modules.
 *
 *  Libraries uit de bestaande repo (Socket/) worden hergebruikt:
 *      - transport/SocketConnection  (TCP upstream naar Pi A)
 *      - protocol/cJSON              (parsen van het target veld)
 */

#include "dispatch/IDeviceHandler.h"
#include "dispatch/WmosHandler.h"
#include "dispatch/DeviceDispatcher.h"
#include "protocol/JsonMessage.h"
#include "protocol/cJSON.h"
#include "transport/SocketConnection.h"

#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace protocol;
using namespace transport;
using namespace dispatch;

// =============================================================================
// CONFIGURATIE  (één plek om aan te passen)
// =============================================================================
namespace cfg {

// HTTP server (Wemos -> Pi B)
constexpr int    HTTP_LISTEN_PORT   = 8080;

// Default upstream (Pi B -> Pi A), kan via argv worden overschreven
constexpr const char* DEFAULT_PIA_HOST = "127.0.0.1";
constexpr int         DEFAULT_PIA_PORT = 5000;

// Wemos HTTP endpoint waar Pi B naartoe POST
constexpr const char* WEMOS_HTTP_PATH  = "/can";
constexpr int         WEMOS_HTTP_PORT  = 80;
constexpr int         WEMOS_TIMEOUT_MS = 800;

// JSON veld namen (contract met Pi A + Wemos firmware)
constexpr const char* JSON_FIELD_TARGET = "target";

// Speciale target naam voor broadcast naar alle Wemos modules
constexpr const char* TARGET_BROADCAST  = "broadcast";

} // namespace cfg


// =============================================================================
// DEVICE ROUTER  -  mapping van "device name" -> IP-adres
// -----------------------------------------------------------------------------
//  Statische registry. Pas hier IPs aan op je eigen Pi B AP-netwerk.
//  Voeg nieuwe Wemos modules toe met registerDevice() in de constructor.
// =============================================================================
class DeviceRouter {
public:
    DeviceRouter() {
        // --- Hardcoded mapping (AP subnet 192.168.4.x) ---
        registerDevice("wemos_relaxstoel", "10.42.0.10");
        registerDevice("wemos_lichtkrant", "10.42.0.20");
        // registerDevice("wemos_xxx",     "192.168.4.NN");
    }

    void registerDevice(const std::string& name, const std::string& ip) {
        std::lock_guard<std::mutex> lk(mtx_);
        map_[name] = ip;
        std::cout << "[router] registered '" << name << "' -> " << ip << std::endl;
    }

    // Geeft lege string terug als het device onbekend is.
    std::string resolve(const std::string& name) const {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = map_.find(name);
        return (it != map_.end()) ? it->second : std::string{};
    }

    // Voor broadcast: alle IPs ophalen.
    std::vector<std::string> allIps() const {
        std::lock_guard<std::mutex> lk(mtx_);
        std::vector<std::string> out;
        out.reserve(map_.size());
        for (auto& kv : map_) out.push_back(kv.second);
        return out;
    }

private:
    mutable std::mutex mtx_;
    std::unordered_map<std::string, std::string> map_;
};


// =============================================================================
// GLOBALS
// =============================================================================
std::unique_ptr<SocketConnection> g_upstream;       // verbinding naar Pi A
std::mutex                        g_upstream_mutex;
static DeviceRouter               g_router;
static std::atomic<bool>          g_running{true};


// =============================================================================
// HELPERS
// =============================================================================

// Pak het "target" veld uit een JSON body. Lege string -> niet gevonden / fout.
static std::string extractTarget(const std::string& jsonBody) {
    cJSON* root = cJSON_Parse(jsonBody.c_str());
    if (!root) return {};
    std::string target;
    cJSON* t = cJSON_GetObjectItem(root, cfg::JSON_FIELD_TARGET);
    if (t && cJSON_IsString(t) && t->valuestring) target = t->valuestring;
    cJSON_Delete(root);
    return target;
}

// Stuur jsonBody als HTTP POST naar één Wemos IP. Negeert response inhoud.
static bool httpPostToWemos(const std::string& ip, const std::string& jsonBody) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;

    struct timeval tv{};
    tv.tv_sec  = cfg::WEMOS_TIMEOUT_MS / 1000;
    tv.tv_usec = (cfg::WEMOS_TIMEOUT_MS % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(cfg::WEMOS_HTTP_PORT);
    if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        ::close(fd);
        return false;
    }
    if (::connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[http->wemos] connect naar " << ip << " mislukt\n";
        ::close(fd);
        return false;
    }

    std::ostringstream req;
    req << "POST " << cfg::WEMOS_HTTP_PATH << " HTTP/1.1\r\n"
        << "Host: " << ip << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << jsonBody.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << jsonBody;
    std::string raw = req.str();

    if (::send(fd, raw.data(), raw.size(), 0) < (ssize_t)raw.size()) {
        std::cerr << "[http->wemos] send naar " << ip << " incompleet\n";
        ::close(fd);
        return false;
    }

    char buf[256];
    ::recv(fd, buf, sizeof(buf), 0); // response negeren
    ::close(fd);
    return true;
}

// Routeer JSON: lookup target -> IP(s) en POST.
static void routeJsonToWemos(const std::string& jsonBody) {
    std::string target = extractTarget(jsonBody);
    if (target.empty()) {
        std::cerr << "[route] geen 'target' veld in JSON, drop\n";
        return;
    }

    if (target == cfg::TARGET_BROADCAST) {
        auto ips = g_router.allIps();
        std::cout << "[route] broadcast naar " << ips.size() << " devices\n";
        for (const auto& ip : ips) httpPostToWemos(ip, jsonBody);
        return;
    }

    std::string ip = g_router.resolve(target);
    if (ip.empty()) {
        std::cerr << "[route] onbekend target '" << target << "', drop\n";
        return;
    }
    std::cout << "[route] '" << target << "' -> " << ip << std::endl;
    if (!httpPostToWemos(ip, jsonBody)) {
        std::cerr << "[route] POST naar " << ip << " mislukt\n";
    }
}


// =============================================================================
// RICHTING 1: Pi A -> Pi B  (socket lijn)  -> route naar Wemos
// =============================================================================
void upstream_rx_thread()
{
    while (g_running) {
        std::string line;
        {
            // We mogen NIET de mutex vasthouden tijdens readLine() (zou de
            // HTTP handler blokkeren). Daarom hier kort checken en dan los.
            std::lock_guard<std::mutex> lock(g_upstream_mutex);
            if (!g_upstream || !g_upstream->isConnected()) {
                // upstream weg; even wachten en opnieuw proberen
            }
        }

        if (!g_upstream || !g_upstream->isConnected()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        line = g_upstream->readLine();
        if (line.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        std::cout << "[upstream] <- Pi A: " << line << std::endl;
        routeJsonToWemos(line);
    }
}


// =============================================================================
// RICHTING 2: Wemos -> Pi B (HTTP POST :8080) -> forward naar Pi A
// =============================================================================
void handle_http_client(int client_fd)
{
    char buffer[4096];
    std::memset(buffer, 0, sizeof(buffer));

    ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) { close(client_fd); return; }
    buffer[n] = '\0';
    std::string request(buffer);
    std::cout << "[http<-wemos] received " << n << " bytes\n";

    // Body extractie
    size_t header_end = request.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        header_end = request.find("\n\n");
        if (header_end == std::string::npos) {
            std::cerr << "[http<-wemos] invalid HTTP\n";
            close(client_fd);
            return;
        }
        header_end += 2;
    } else {
        header_end += 4;
    }
    std::string body = request.substr(header_end);
    std::cout << "[http<-wemos] body: " << body << std::endl;

    // Forward naar Pi A
    std::string ack_response;
    {
        std::lock_guard<std::mutex> lock(g_upstream_mutex);
        if (g_upstream && g_upstream->isConnected()) {
            if (g_upstream->sendLine(body)) {
                // Removed the problematic readLine() to avoid race condition
                ack_response = "{\"status\":\"forwarded\"}"; // Static HTTP response
            } else {
                ack_response = "{\"status\":\"error\"}";
            }
        } else {
            ack_response = "{\"status\":\"not connected to Pi A\"}";
        }
    }

    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << ack_response.length() << "\r\n"
        << "\r\n"
        << ack_response;
    std::string response = oss.str();
    send(client_fd, response.c_str(), response.length(), 0);
    close(client_fd);
}

void http_server_thread(int port)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { std::cerr << "[http] socket() failed\n"; return; }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[http] bind failed on " << port << "\n";
        close(listen_fd); return;
    }
    if (listen(listen_fd, 10) < 0) {
        std::cerr << "[http] listen failed\n";
        close(listen_fd); return;
    }
    std::cout << "[http] listening on port " << port << std::endl;

    while (g_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) continue;
        std::thread(handle_http_client, client_fd).detach();
    }
    close(listen_fd);
}


// =============================================================================
// MAIN
// =============================================================================
int main(int argc, char* argv[])
{
    std::cout << "=== RPi_B HTTP forwarder + DeviceRouter ===" << std::endl;

    int         http_port = cfg::HTTP_LISTEN_PORT;
    std::string rpia_host = cfg::DEFAULT_PIA_HOST;
    int         rpia_port = cfg::DEFAULT_PIA_PORT;

    if (argc >= 2) rpia_host = argv[1];
    if (argc >= 3) rpia_port = std::stoi(argv[2]);

    // Upstream connectie naar Pi A
    std::cout << "[main] connecting to Pi A at " << rpia_host << ":" << rpia_port << std::endl;
    g_upstream = std::make_unique<SocketConnection>();
    if (!g_upstream->connect(rpia_host, rpia_port)) {
        std::cerr << "[main] connect Pi A mislukt\n";
        return 1;
    }
    std::cout << "[main] connected to Pi A\n";

    // Beide richtingen starten
    std::thread(http_server_thread, http_port).detach();   // Wemos -> Pi A
    std::thread(upstream_rx_thread).detach();              // Pi A -> Wemos

    std::cout << "[main] running, Ctrl+C to stop\n";
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
