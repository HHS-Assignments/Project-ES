#include "HttpServer.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

namespace transport {

HttpServer::HttpServer()
    : listenPort_(0), maxConnections_(10), listenFd_(-1)
{
}

HttpServer::~HttpServer()
{
    if (listenFd_ >= 0) {
        ::close(listenFd_);
    }
}

void HttpServer::start(int port)
{
    listenPort_ = port;

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        std::cerr << "[HttpServer::start] Failed to create socket" << std::endl;
        return;
    }

    int opt = 1;
    if (setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "[HttpServer::start] setsockopt failed" << std::endl;
        return;
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(listenFd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[HttpServer::start] Bind failed on port " << port << std::endl;
        ::close(listenFd_);
        listenFd_ = -1;
        return;
    }

    if (listen(listenFd_, maxConnections_) < 0) {
        std::cerr << "[HttpServer::start] Listen failed" << std::endl;
        ::close(listenFd_);
        listenFd_ = -1;
        return;
    }

    std::cout << "[HttpServer::start] Listening on port " << port << std::endl;
}

SocketConnection HttpServer::acceptConnection()
{
    SocketConnection conn;
    // TODO: Accept connection and wrap in SocketConnection
    std::cout << "[HttpServer::acceptConnection] Accepting..." << std::endl;
    return conn;
}

std::string HttpServer::extractBody(const std::string& request) const
{
    // TODO: Parse HTTP request and extract body
    // Simple implementation: split by \n\n
    size_t pos = request.find("\n\n");
    if (pos != std::string::npos) {
        return request.substr(pos + 2);
    }
    return "";
}

void HttpServer::sendResponse(SocketConnection& conn, const std::string& body)
{
    // TODO: Format and send HTTP response
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\nContent-Length: " << body.length() << "\r\n\r\n" << body;
    conn.sendLine(oss.str());
}

} // namespace transport
