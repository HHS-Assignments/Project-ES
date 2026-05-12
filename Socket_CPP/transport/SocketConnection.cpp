#include "SocketConnection.h"
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>

namespace transport {

SocketConnection::SocketConnection()
    : fd_(-1), host_(""), port_(0)
{
}

SocketConnection::~SocketConnection()
{
    close();
}

bool SocketConnection::connect(const std::string& host, int port)
{
    host_ = host;
    port_ = port;

    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        std::cerr << "[SocketConnection::connect] Failed to create socket" << std::endl;
        return false;
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "[SocketConnection::connect] Invalid host: " << host << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    if (::connect(fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[SocketConnection::connect] Connection failed to " << host << ":" << port << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    std::cout << "[SocketConnection::connect] Connected to " << host << ":" << port << std::endl;
    return true;
}

bool SocketConnection::sendLine(const std::string& data)
{
    if (fd_ < 0) return false;

    std::string msg = data + "\n";
    ssize_t sent = send(fd_, msg.c_str(), msg.length(), 0);
    return sent == (ssize_t)msg.length();
}

std::string SocketConnection::readLine()
{
    // TODO: Implement line reading with buffering
    // Placeholder: simple recv until newline
    if (fd_ < 0) return "";
    
    char buffer[1024];
    ssize_t n = recv(fd_, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) return "";
    
    buffer[n] = '\0';
    std::string line(buffer);
    // Remove trailing newline if present
    if (!line.empty() && line.back() == '\n') {
        line.pop_back();
    }
    return line;
}

void SocketConnection::close()
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

} // namespace transport
