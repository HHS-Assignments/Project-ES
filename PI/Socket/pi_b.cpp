// pi_b.cpp
// Bridges Pi A (TCP client) <-> Wemos boards (TCP).
// - Connects (as client) to Pi A at <piA_ip>:<piA_port>.
// - Listens (as server) on <web_port> (default 9001) for inbound Wemos messages.
// - For each line received from Pi A, forwards it to every known Wemos (TCP push).
// - For each line received from a Wemos, forwards it to Pi A.
//
// Usage:  ./pi_b <piA_ip> <piA_port> <web_port>
//
// Build:  g++ -O2 -std=c++17 -pthread pi_b.cpp -o pi_b

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <iostream>

// ---- Known Wemos targets (IP, web port). Web port matches what the Wemos `_server` listens on. ----
struct Wemos { const char* name; const char* ip; uint16_t port; };
static const std::vector<Wemos> kWemos = {
    {"wemos_relaxstoel",  "10.42.0.10", 9001},
    {"wemos_lichtkrant",  "10.42.0.20", 9001},
};

static std::atomic<bool> g_run{true};
static std::mutex g_piMu;
static int g_piFd = -1;     // socket to Pi A

// ---- TCP helpers ----
static bool sendAll(int fd, const char* p, size_t len) {
    while (len > 0) {
        ssize_t n = ::send(fd, p, len, MSG_NOSIGNAL);
        if (n <= 0) return false;
        p += n; len -= (size_t)n;
    }
    return true;
}
static bool sendLine(int fd, const std::string& line) {
    std::string out = line; out.push_back('\n');
    return sendAll(fd, out.data(), out.size());
}

static int connectTo(const char* ip, uint16_t port, int timeoutMs = 2000) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    // Non-blocking connect with timeout
    int fl = fcntl(fd, F_GETFL, 0); fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(port);
    inet_pton(AF_INET, ip, &a.sin_addr);
    int r = ::connect(fd, (sockaddr*)&a, sizeof(a));
    if (r < 0 && errno != EINPROGRESS) { ::close(fd); return -1; }

    fd_set wf; FD_ZERO(&wf); FD_SET(fd, &wf);
    timeval tv{ timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
    r = select(fd + 1, nullptr, &wf, nullptr, &tv);
    if (r <= 0) { ::close(fd); return -1; }
    int err = 0; socklen_t el = sizeof(err);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &el);
    if (err) { ::close(fd); return -1; }

    fcntl(fd, F_SETFL, fl); // back to blocking
    int yes = 1; setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    return fd;
}

// ---- Fan-out to all Wemos boards (opens a short-lived TCP connection per send,
// mirroring what the Wemos `send()` method does on its side). ----
static void fanoutToWemos(const std::string& line) {
    for (const auto& w : kWemos) {
        int fd = connectTo(w.ip, w.port, 500);
        if (fd < 0) {
            std::cerr << "[B] " << w.name << " (" << w.ip << ":" << w.port << ") unreachable\n";
            continue;
        }
        if (!sendLine(fd, line)) {
            std::cerr << "[B] " << w.name << " send failed\n";
        } else {
            std::cout << "[B] ->" << w.name << " " << line << "\n";
        }
        // briefly drain any ACK
        char tmp[64];
        timeval tv{0, 50 * 1000};
        fd_set rf; FD_ZERO(&rf); FD_SET(fd, &rf);
        if (select(fd + 1, &rf, nullptr, nullptr, &tv) > 0) (void)::recv(fd, tmp, sizeof(tmp), 0);
        ::close(fd);
    }
}

// ---- Reader: Pi A -> (parse lines) -> fanout to Wemos ----
static void piAReader(int fd) {
    std::string acc; char buf[2048];
    while (g_run) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        acc.append(buf, buf + n);
        size_t pos;
        while ((pos = acc.find('\n')) != std::string::npos) {
            std::string line = acc.substr(0, pos);
            acc.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            std::cout << "[B] A-> " << line << "\n";
            fanoutToWemos(line);
        }
    }
}

// ---- Per-connection handler for inbound Wemos message ----
static void handleWemosConn(int cfd, sockaddr_in cli) {
    char ip[64]; inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
    std::string acc; char buf[1024];
    // Read a single line (Wemos sends one JSON per connection)
    auto t0 = std::chrono::steady_clock::now();
    while (g_run) {
        timeval tv{1, 0};
        fd_set rf; FD_ZERO(&rf); FD_SET(cfd, &rf);
        int r = select(cfd + 1, &rf, nullptr, nullptr, &tv);
        if (r <= 0) break;
        ssize_t n = ::recv(cfd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        acc.append(buf, buf + n);
        size_t pos;
        while ((pos = acc.find('\n')) != std::string::npos) {
            std::string line = acc.substr(0, pos);
            acc.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            std::cout << "[B] wemos@" << ip << " -> " << line << "\n";
            // Forward to Pi A
            int pi;
            { std::lock_guard<std::mutex> lk(g_piMu); pi = g_piFd; }
            if (pi >= 0) {
                if (!sendLine(pi, line)) std::cerr << "[B] Pi A write failed\n";
            } else {
                std::cerr << "[B] Pi A not connected; dropping\n";
            }
            // ACK back to wemos (mirrors wemos's expectation)
            sendAll(cfd, "ACK\n", 4);
        }
        if (std::chrono::steady_clock::now() - t0 > std::chrono::seconds(2)) break;
    }
    ::close(cfd);
}

// ---- Maintains TCP connection to Pi A with auto-reconnect ----
static void piALink(const std::string& ip, uint16_t port) {
    while (g_run) {
        int fd = connectTo(ip.c_str(), port, 2000);
        if (fd < 0) {
            std::cerr << "[B] Pi A " << ip << ":" << port << " unreachable, retrying...\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }
        std::cout << "[B] Connected to Pi A " << ip << ":" << port << "\n";
        { std::lock_guard<std::mutex> lk(g_piMu); g_piFd = fd; }
        piAReader(fd);
        { std::lock_guard<std::mutex> lk(g_piMu); g_piFd = -1; }
        ::close(fd);
        std::cerr << "[B] Pi A link lost, reconnecting...\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "Usage: %s <piA_ip> <piA_port> <web_port>\n", argv[0]);
        return 1;
    }
    std::string piIp = argv[1];
    uint16_t piPort  = (uint16_t)std::atoi(argv[2]);
    uint16_t webPort = (uint16_t)std::atoi(argv[3]);

    // Wemos-facing TCP server
    int srv = ::socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1; setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons(webPort);
    if (::bind(srv, (sockaddr*)&a, sizeof(a)) < 0) { perror("bind"); return 1; }
    if (::listen(srv, 8) < 0) { perror("listen"); return 1; }
    std::cout << "[B] Wemos server listening on :" << webPort << "\n";

    std::thread tLink(piALink, piIp, piPort);

    while (g_run) {
        sockaddr_in cli{}; socklen_t cl = sizeof(cli);
        int cfd = ::accept(srv, (sockaddr*)&cli, &cl);
        if (cfd < 0) { if (errno == EINTR) continue; perror("accept"); break; }
        std::thread(handleWemosConn, cfd, cli).detach();
    }

    g_run = false;
    ::close(srv);
    if (tLink.joinable()) tLink.join();
    return 0;
}
