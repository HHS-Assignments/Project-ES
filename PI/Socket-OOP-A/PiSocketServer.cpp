#include "PiSocketServer.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <iostream>

PiSocketServer::PiSocketServer(uint16_t port) : _port(port) {}

bool PiSocketServer::begin() {
    _srvFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (_srvFd < 0) { perror("socket"); return false; }

    int yes = 1;
    ::setsockopt(_srvFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in a{};
    a.sin_family      = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port        = htons(_port);
    if (::bind(_srvFd, (sockaddr*)&a, sizeof(a)) < 0) {
        perror("bind");
        stop();
        return false;
    }
    if (::listen(_srvFd, 1) < 0) {
        perror("listen");
        stop();
        return false;
    }
    std::cout << "[Server] Luistert op :" << _port << "\n";
    return true;
}

bool PiSocketServer::isConnected() {
    std::lock_guard<std::mutex> lk(_mu);
    return _peerFd >= 0;
}

void PiSocketServer::stop() {
    _dropPeer();
    if (_srvFd >= 0) { ::close(_srvFd); _srvFd = -1; }
}

void PiSocketServer::_dropPeer() {
    std::lock_guard<std::mutex> lk(_mu);
    if (_peerFd >= 0) { ::close(_peerFd); _peerFd = -1; }
    _acc.clear();
}

bool PiSocketServer::_accept() {
    sockaddr_in cli{};
    socklen_t cl = sizeof(cli);
    int fd = ::accept(_srvFd, (sockaddr*)&cli, &cl);
    if (fd < 0) {
        if (errno == EINTR) return false;
        perror("accept");
        return false;
    }

    char ip[64];
    inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
    std::cout << "[Server] Peer verbonden vanaf " << ip << ":" << ntohs(cli.sin_port) << "\n";

    std::lock_guard<std::mutex> lk(_mu);
    if (_peerFd >= 0) ::close(_peerFd);  // oude peer vervangen
    _peerFd = fd;
    _acc.clear();
    return true;
}

bool PiSocketServer::send(const std::string &line) {
    std::lock_guard<std::mutex> lk(_mu);
    if (_peerFd < 0) return false;

    std::string out = line + "\n";
    const char *p = out.data();
    size_t left = out.size();
    while (left > 0) {
        ssize_t n = ::send(_peerFd, p, left, MSG_NOSIGNAL);
        if (n <= 0) {
            ::close(_peerFd);
            _peerFd = -1;
            return false;
        }
        p += n;
        left -= (size_t)n;
    }
    return true;
}

bool PiSocketServer::receive(std::string &line) {
    while (_srvFd >= 0) {
        // Eerst: complete regel in de buffer?
        size_t pos = _acc.find('\n');
        if (pos != std::string::npos) {
            line = _acc.substr(0, pos);
            _acc.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            return true;
        }

        // Peer nodig? Blokkeer op accept.
        int fd;
        { std::lock_guard<std::mutex> lk(_mu); fd = _peerFd; }
        if (fd < 0) {
            if (!_accept()) continue;
            { std::lock_guard<std::mutex> lk(_mu); fd = _peerFd; }
            if (fd < 0) continue;
        }

        char buf[1024];
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) {
            std::cout << "[Server] Peer verbroken\n";
            _dropPeer();
            continue;  // wacht op nieuwe verbinding
        }
        _acc.append(buf, buf + n);
    }
    return false;  // server gestopt
}
