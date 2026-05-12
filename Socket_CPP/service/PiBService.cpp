#include "PiBService.h"
#include "../protocol/JsonMessage.h"
#include <iostream>

namespace service {

PiBService::PiBService()
    : wmosServer_(std::make_unique<transport::HttpServer>()),
      upstream_(std::make_unique<transport::SocketConnection>()),
      dispatcher_(std::make_unique<dispatch::DeviceDispatcher>())
{
}

void PiBService::start(int wmosPort, const std::string& aHost, int aPort)
{
    std::cout << "[PiBService::start] Starting HTTP server on port " << wmosPort << std::endl;
    wmosServer_->start(wmosPort);

    std::cout << "[PiBService::start] Connecting to Pi-A at " << aHost << ":" << aPort << std::endl;
    if (upstream_->connect(aHost, aPort)) {
        std::cout << "[PiBService::start] Connected to Pi-A successfully" << std::endl;
    } else {
        std::cerr << "[PiBService::start] Failed to connect to Pi-A" << std::endl;
    }

    // TODO: Spawn worker threads to handle HTTP requests
    std::cout << "[PiBService::start] Service started" << std::endl;
}

void PiBService::forwardToPiA(const protocol::JsonMessage& message)
{
    std::cout << "[PiBService::forwardToPiA] Forwarding message: " << message.getDevice() << std::endl;
    std::string serialized = message.serialize();
    if (upstream_->isConnected()) {
        upstream_->sendLine(serialized);
    } else {
        std::cerr << "[PiBService::forwardToPiA] Not connected to Pi-A" << std::endl;
    }
}

void PiBService::readFromPiA()
{
    std::cout << "[PiBService::readFromPiA] Waiting for acknowledgements from Pi-A..." << std::endl;
    // TODO: Read ACK messages in a loop
}

void PiBService::handleWmosRequest(const std::string& request)
{
    std::cout << "[PiBService::handleWmosRequest] Processing WMos request: " << request << std::endl;
    // TODO: Parse HTTP body, create JsonMessage, forward to Pi-A
    protocol::JsonMessage msg;
    // ... parse request into msg ...
    // forwardToPiA(msg);
}

} // namespace service
