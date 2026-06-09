// pi_a.cpp
// Bridge between a SocketCAN interface (default can0) and a single TCP peer (Pi B).
// Usage:  ./pi_a <listen_port> [can_iface]
// Wire protocol (line-delimited JSON):
//   {"id": <canID_decimal_or_0xHEX>, "data": "<hex bytes, e.g. DEADBEEF>"}
//
// Build:  g++ -O2 -std=c++17 -pthread pi_a.cpp -o pi_a

#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>

static std::atomic<bool> g_run{true};

// ---------- helpers ----------
static std::string bytesToHex(const uint8_t* d, size_t n) {
    static const char* H = "0123456789ABCDEF";
    std::string s; s.resize(n * 2);
    for (size_t i = 0; i < n; ++i) { s[2*i] = H[(d[i]>>4)&0xF]; s[2*i+1] = H[d[i]&0xF]; }
    return s;
}
static int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}
static size_t hexToBytes(const std::string& s, uint8_t* out, size_t maxLen) {
    size_t n = 0;
    for (size_t i = 0; i + 1 < s.size() && n < maxLen; i += 2) {
        int h = hexNibble(s[i]); int l = hexNibble(s[i+1]);
        if (h < 0 || l < 0) break;
        out[n++] = uint8_t((h << 4) | l);
    }
    return n;
}

// Minimal JSON extractor for our fixed shape: {"id":..,"data":"..."}.
// Returns true on success; sets canId and dataHex.
static bool parseJson(const std::string& line, uint32_t& canId, std::string& dataHex) {
    auto findKey = [&](const char* key) -> size_t {
        std::string pat = std::string("\"") + key + "\"";
        return line.find(pat);
    };
    size_t pId = findKey("id");
    size_t pDt = findKey("data");
    if (pId == std::string::npos || pDt == std::string::npos) return false;

    // id value
    size_t c = line.find(':', pId); if (c == std::string::npos) return false;
    ++c;
    while (c < line.size() && (line[c]==' '||line[c]=='"')) ++c;
    size_t e = c;
    while (e < line.size() && line[e] != ',' && line[e] != '}' && line[e] != '"') ++e;
    std::string idStr = line.substr(c, e - c);
    try {
        canId = (idStr.rfind("0x",0)==0 || idStr.rfind("0X",0)==0)
              ? (uint32_t)std::stoul(idStr, nullptr, 16)
              : (uint32_t)std::stoul(idStr, nullptr, 10);
    } catch (...) { return false; }

    // data value (string)
    size_t c2 = line.find(':', pDt); if (c2 == std::string::npos) return false;
    size_t q1 = line.find('"', c2); if (q1 == std::string::npos) return false;
    size_t q2 = line.find('"', q1 + 1); if (q2 == std::string::npos) return false;
    dataHex = line.substr(q1 + 1, q2 - q1 - 1);
    return true;
}

// ---------- CAN ----------
static int openCan(const char* iface) {
    int s = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) { perror("socket(CAN)"); return -1; }
    ifreq ifr{}; std::strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) { perror("ioctl SIOCGIFINDEX"); ::close(s); return -1; }
    sockaddr_can addr{}; addr.can_family = AF_CAN; addr.can_ifindex = ifr.ifr_ifindex;
    if (::bind(s, (sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind(CAN)"); ::close(s); return -1; }
    return s;
}

// ---------- TCP server (single peer = Pi B) ----------
static int listenTcp(uint16_t port) {
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }
    int yes = 1; ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons(port);
    if (::bind(s, (sockaddr*)&a, sizeof(a)) < 0) { perror("bind"); ::close(s); return -1; }
    if (::listen(s, 1) < 0) { perror("listen"); ::close(s); return -1; }
    return s;
}

static std::mutex g_txMu;
static int g_peerFd = -1;

static bool sendLine(int fd, const std::string& line) {
    std::lock_guard<std::mutex> lk(g_txMu);
    std::string out = line + "\n";
    const char* p = out.data(); size_t left = out.size();
    while (left > 0) {
        ssize_t n = ::send(fd, p, left, MSG_NOSIGNAL);
        if (n <= 0) return false;
        p += n; left -= (size_t)n;
    }
    return true;
}

// CAN -> TCP
static void canReader(int canFd) {
    while (g_run) {
        can_frame f{};
        ssize_t n = ::read(canFd, &f, sizeof(f));
        if (n <= 0) { if (errno == EINTR) continue; perror("read(CAN)"); break; }
        if (n < (ssize_t)sizeof(can_frame)) continue;

        uint32_t id = f.can_id & (f.can_id & CAN_EFF_FLAG ? CAN_EFF_MASK : CAN_SFF_MASK);
        std::string hex = bytesToHex(f.data, f.can_dlc);
        char buf[128];
        std::snprintf(buf, sizeof(buf), "{\"id\":%u,\"data\":\"%s\"}", id, hex.c_str());

        int fd;
        { std::lock_guard<std::mutex> lk(g_txMu); fd = g_peerFd; }
        if (fd >= 0) {
            if (!sendLine(fd, buf)) {
                std::cerr << "[A] peer write failed; dropping link\n";
                std::lock_guard<std::mutex> lk(g_txMu);
                if (g_peerFd == fd) { ::close(g_peerFd); g_peerFd = -1; }
            } else {
                std::cout << "[A] CAN->B " << buf << "\n";
            }
        }
    }
}

// TCP -> CAN
static void peerReader(int peerFd, int canFd) {
    std::string acc;
    char buf[1024];
    while (g_run) {
        ssize_t n = ::recv(peerFd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        acc.append(buf, buf + n);
        size_t pos;
        while ((pos = acc.find('\n')) != std::string::npos) {
            std::string line = acc.substr(0, pos);
            acc.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            uint32_t id = 0; std::string hex;
            if (!parseJson(line, id, hex)) { std::cerr << "[A] bad JSON: " << line << "\n"; continue; }

            can_frame f{};
            f.can_id = id;
            if (id > 0x7FF) f.can_id |= CAN_EFF_FLAG;
            uint8_t bytes[8]{};
            size_t dl = hexToBytes(hex, bytes, 8);
            f.can_dlc = (uint8_t)dl;
            std::memcpy(f.data, bytes, dl);
            if (::write(canFd, &f, sizeof(f)) != (ssize_t)sizeof(f)) {
                perror("write(CAN)");
            } else {
                std::cout << "[A] B->CAN id=" << id << " data=" << hex << "\n";
            }
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <listen_port> [can_iface=can0]\n", argv[0]);
        return 1;
    }
    uint16_t port = (uint16_t)std::atoi(argv[1]);
    const char* iface = (argc >= 3) ? argv[2] : "can0";

    int canFd = openCan(iface);
    if (canFd < 0) return 1;
    int srv = listenTcp(port);
    if (srv < 0) { ::close(canFd); return 1; }

    std::cout << "[A] CAN=" << iface << "  listening on :" << port << "\n";

    std::thread tCan(canReader, canFd);

    while (g_run) {
        sockaddr_in cli{}; socklen_t cl = sizeof(cli);
        int fd = ::accept(srv, (sockaddr*)&cli, &cl);
        if (fd < 0) { if (errno == EINTR) continue; perror("accept"); break; }

        char ip[64]; inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
        std::cout << "[A] Pi B connected from " << ip << ":" << ntohs(cli.sin_port) << "\n";

        { std::lock_guard<std::mutex> lk(g_txMu);
          if (g_peerFd >= 0) ::close(g_peerFd);
          g_peerFd = fd; }

        peerReader(fd, canFd); // blocks until peer disconnects

        std::lock_guard<std::mutex> lk(g_txMu);
        if (g_peerFd >= 0) { ::close(g_peerFd); g_peerFd = -1; }
        std::cout << "[A] Pi B disconnected\n";
    }

    g_run = false;
    ::close(srv);
    ::close(canFd);
    if (tCan.joinable()) tCan.join();
    return 0;
}
