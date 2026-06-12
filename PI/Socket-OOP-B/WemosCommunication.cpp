#include "WemosCommunication.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <iostream>

// Non-blocking connect met timeout; geeft fd terug of -1.
static int connectTo(const char *ip, uint16_t port, int timeoutMs) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int fl = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);

    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port   = htons(port);
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

    fcntl(fd, F_SETFL, fl);
    int yes = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    return fd;
}

static bool sendAll(int fd, const char *p, size_t len) {
    while (len > 0) {
        ssize_t n = ::send(fd, p, len, MSG_NOSIGNAL);
        if (n <= 0) return false;
        p += n;
        len -= (size_t)n;
    }
    return true;
}

WemosCommunication::WemosCommunication(uint16_t receivePort, std::vector<WemosTarget> targets)
    : _receivePort(receivePort), _targets(std::move(targets)) {}

bool WemosCommunication::begin() {
    _srvFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (_srvFd < 0) { perror("socket"); return false; }

    int yes = 1;
    setsockopt(_srvFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in a{};
    a.sin_family      = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port        = htons(_receivePort);
    if (::bind(_srvFd, (sockaddr*)&a, sizeof(a)) < 0) {
        perror("bind");
        stop();
        return false;
    }
    if (::listen(_srvFd, 8) < 0) {
        perror("listen");
        stop();
        return false;
    }
    std::cout << "[B] Wemos-server luistert op :" << _receivePort << "\n";
    return true;
}

bool WemosCommunication::isConnected() {
    return _srvFd >= 0;
}

void WemosCommunication::stop() {
    if (_srvFd >= 0) { ::close(_srvFd); _srvFd = -1; }
}

// Fan-out: één korte TCP-verbinding per Wemos.
bool WemosCommunication::send(const std::string &line) {
    bool iets = false;
    for (const auto &w : _targets) {
        int fd = connectTo(w.ip.c_str(), w.port, 500);
        if (fd < 0) {
            std::cerr << "[B] " << w.name << " (" << w.ip << ":" << w.port << ") onbereikbaar\n";
            continue;
        }
        std::string out = line + "\n";
        if (!sendAll(fd, out.data(), out.size())) {
            std::cerr << "[B] " << w.name << " verzenden mislukt\n";
        } else {
            std::cout << "[B] ->" << w.name << " " << line << "\n";
            iets = true;
        }
        // ACK van de Wemos kort leegtrekken
        char tmp[64];
        timeval tv{0, 50 * 1000};
        fd_set rf; FD_ZERO(&rf); FD_SET(fd, &rf);
        if (select(fd + 1, &rf, nullptr, nullptr, &tv) > 0) (void)::recv(fd, tmp, sizeof(tmp), 0);
        ::close(fd);
    }
    return iets;
}

// Accepteert één Wemos-verbinding, leest één regel, ACK't en sluit.
bool WemosCommunication::receive(std::string &line) {
    while (_srvFd >= 0) {
        sockaddr_in cli{};
        socklen_t cl = sizeof(cli);
        int cfd = ::accept(_srvFd, (sockaddr*)&cli, &cl);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            return false;  // server gesloten
        }

        char ip[64];
        inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));

        // Eén regel lezen met timeout (Wemos stuurt één JSON per verbinding)
        std::string acc;
        char buf[1024];
        bool gevonden = false;
        for (int pogingen = 0; pogingen < 2 && !gevonden; pogingen++) {
            timeval tv{1, 0};
            fd_set rf; FD_ZERO(&rf); FD_SET(cfd, &rf);
            if (select(cfd + 1, &rf, nullptr, nullptr, &tv) <= 0) break;
            ssize_t n = ::recv(cfd, buf, sizeof(buf), 0);
            if (n <= 0) break;
            acc.append(buf, buf + n);

            size_t pos = acc.find('\n');
            if (pos != std::string::npos) {
                line = acc.substr(0, pos);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                gevonden = !line.empty();
            }
        }

        if (gevonden) {
            std::cout << "[B] wemos@" << ip << " -> " << line << "\n";
            sendAll(cfd, "ACK\n", 4);
            ::close(cfd);
            return true;
        }
        ::close(cfd);
        // niets bruikbaars ontvangen: wacht op de volgende verbinding
    }
    return false;
}
