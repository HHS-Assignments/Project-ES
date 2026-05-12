#ifndef SERVICE_PIBSERVICE_H
#define SERVICE_PIBSERVICE_H

#include "../transport/SocketConnection.h"
#include "../transport/HttpServer.h"
#include "../transport/ThreadPoolWorker.h"
#include "../dispatch/DeviceDispatcher.h"
#include <memory>
#include <vector>

namespace protocol {
class JsonMessage;  // Forward declaration
}

namespace service {

/**
 * Service for Pi-B (forwarder/HTTP gateway side).
 * Runs an HTTP server for WMos devices and forwards messages to Pi-A via persistent TCP.
 */
class PiBService {
private:
    std::unique_ptr<transport::HttpServer> wmosServer_;
    std::unique_ptr<transport::SocketConnection> upstream_;
    std::unique_ptr<dispatch::DeviceDispatcher> dispatcher_;
    std::vector<std::unique_ptr<transport::ThreadPoolWorker>> workers_;

public:
    PiBService();
    ~PiBService() = default;

    /**
     * Start HTTP server on wmosPort and connect to Pi-A at aHost:aPort.
     */
    void start(int wmosPort, const std::string& aHost, int aPort);

    /**
     * Forward a message to Pi-A via the persistent TCP connection.
     */
    void forwardToPiA(const protocol::JsonMessage& message);

    /**
     * Read incoming acknowledgements from Pi-A.
     */
    void readFromPiA();

    /**
     * Handle an HTTP request from a WMos device.
     */
    void handleWmosRequest(const std::string& request);

    transport::HttpServer* getHttpServer() { return wmosServer_.get(); }
    dispatch::DeviceDispatcher* getDispatcher() { return dispatcher_.get(); }
};

} // namespace service

#endif // SERVICE_PIBSERVICE_H
