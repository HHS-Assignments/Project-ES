#include "PiSocketClient.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <chrono>
#include <thread>
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

PiSocketClient::PiSocketClient(const std::string &host, uint16_t port)
    : _host(host), _port(port) {}

bool PiSocketClient::begin() {
    _stopped = false;
    return true;  // verbinding wordt door receive() opgezet (met retry)
}

bool PiSocketClient::isConnected() {
    std::lock_guard<std::mutex> lk(_mu);
    return _fd >= 0;
}

void PiSocketClient::stop() {
    _stopped = true;
    _drop();
}

void PiSocketClient::_drop() {
    std::lock_guard<std::mutex> lk(_mu);
    if (_fd >= 0) { ::close(_fd); _fd = -1; }
    _acc.clear();
}

bool PiSocketClient::_connect() {
    int fd = connectTo(_host.c_str(), _port, 2000);
    if (fd < 0) {
        std::cerr << "[B] Pi A " << _host << ":" << _port << " onbereikbaar, opnieuw...\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return false;
    }
    std::cout << "[B] Verbonden met Pi A " << _host << ":" << _port << "\n";
    std::lock_guard<std::mutex> lk(_mu);
    _fd = fd;
    _acc.clear();
    return true;
}

bool PiSocketClient::send(const std::string &line) {
    std::lock_guard<std::mutex> lk(_mu);
    if (_fd < 0) return false;

    std::string out = line + "\n";
    const char *p = out.data();
    size_t left = out.size();
    while (left > 0) {
        ssize_t n = ::send(_fd, p, left, MSG_NOSIGNAL);
        if (n <= 0) {
            ::close(_fd);
            _fd = -1;
            return false;
        }
        p += n;
        left -= (size_t)n;
    }
    return true;
}

bool PiSocketClient::receive(std::string &line) {
    while (!_stopped) {
        // Eerst: complete regel in de buffer?
        size_t pos = _acc.find('\n');
        if (pos != std::string::npos) {
            line = _acc.substr(0, pos);
            _acc.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            return true;
        }

        int fd;
        { std::lock_guard<std::mutex> lk(_mu); fd = _fd; }
        if (fd < 0) {
            if (!_connect()) continue;
            { std::lock_guard<std::mutex> lk(_mu); fd = _fd; }
            if (fd < 0) continue;
        }

        char buf[2048];
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) {
            std::cerr << "[B] Verbinding met Pi A verbroken, herverbinden...\n";
            _drop();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        _acc.append(buf, buf + n);
    }
    return false;  // gestopt
}
