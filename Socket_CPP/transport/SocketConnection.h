#ifndef TRANSPORT_SOCKETCONNECTION_H
#define TRANSPORT_SOCKETCONNECTION_H

#include <string>
#include <mutex>

namespace transport {

/**
 * Wraps BSD socket operations for TCP communication.
 * Provides thread-safe send/receive operations with line buffering.
 */
class SocketConnection {
private:
    int fd_;
    std::string host_;
    int port_;

public:
    SocketConnection();
    ~SocketConnection();

    /**
     * Connect to remote host:port.
     * Returns true on success, false on failure.
     */
    bool connect(const std::string& host, int port);

    /**
     * Send a line of text (automatically appends newline).
     * Returns true on success, false on send failure.
     */
    bool sendLine(const std::string& data);

    /**
     * Read a line from socket (blocking).
     * Returns the line without newline, or empty string on error/EOF.
     */
    std::string readLine();

    /**
     * Close the socket.
     */
    void close();

    // Getters
    int getFd() const { return fd_; }
    std::string getHost() const { return host_; }
    int getPort() const { return port_; }
    bool isConnected() const { return fd_ >= 0; }
};

} // namespace transport

#endif // TRANSPORT_SOCKETCONNECTION_H
