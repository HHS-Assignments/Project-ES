#ifndef TRANSPORT_HTTPSERVER_H
#define TRANSPORT_HTTPSERVER_H

#include "SocketConnection.h"
#include <vector>

namespace transport {

/**
 * Simple HTTP server for receiving WMos device requests.
 * Spawns thread pool workers to handle incoming connections.
 */
class HttpServer {
private:
    int listenPort_;
    int maxConnections_;
    int listenFd_;

public:
    HttpServer();
    ~HttpServer();

    /**
     * Start listening on the specified port.
     * Does not block; spawns background thread(s).
     */
    void start(int port);

    /**
     * Accept a new connection (blocking).
     * Returns a SocketConnection object.
     */
    SocketConnection acceptConnection();

    /**
     * Extract request body from HTTP POST request.
     */
    std::string extractBody(const std::string& request) const;

    /**
     * Send an HTTP response.
     */
    void sendResponse(SocketConnection& conn, const std::string& body);

    int getListenPort() const { return listenPort_; }
    int getMaxConnections() const { return maxConnections_; }
};

} // namespace transport

#endif // TRANSPORT_HTTPSERVER_H
