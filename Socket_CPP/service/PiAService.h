#ifndef SERVICE_PIASERVICE_H
#define SERVICE_PIASERVICE_H

#include "../transport/SocketConnection.h"
#include "../dispatch/DeviceDispatcher.h"
#include <memory>

namespace protocol {
class JsonMessage;  // Forward declaration
}

namespace service {

/**
 * Service for Pi-A (receiver/dispatcher side).
 * Listens for TCP connections from Pi-B, receives messages, and dispatches them.
 */
class PiAService {
private:
    std::unique_ptr<transport::SocketConnection> peer_;
    std::unique_ptr<dispatch::DeviceDispatcher> dispatcher_;

public:
    PiAService();
    ~PiAService() = default;

    /**
     * Start listening on the specified port for incoming connections from Pi-B.
     */
    void start(int port);

    /**
     * Accept a peer connection from Pi-B.
     */
    void acceptPeer();

    /**
     * Process an incoming message and dispatch to handlers.
     */
    void processIncoming(const protocol::JsonMessage& message);

    /**
     * Send an acknowledgement message back to Pi-B.
     */
    void sendAck(const std::string& status);

    dispatch::DeviceDispatcher* getDispatcher() { return dispatcher_.get(); }
};

} // namespace service

#endif // SERVICE_PIASERVICE_H
