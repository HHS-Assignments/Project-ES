#include "PiAService.h"
#include "../protocol/JsonMessage.h"
#include <iostream>

namespace service {

PiAService::PiAService()
    : peer_(std::make_unique<transport::SocketConnection>()),
      dispatcher_(std::make_unique<dispatch::DeviceDispatcher>())
{
}

void PiAService::start(int port)
{
    std::cout << "[PiAService::start] Starting on port " << port << std::endl;
    // TODO: Set up TCP listener on the specified port
}

void PiAService::acceptPeer()
{
    std::cout << "[PiAService::acceptPeer] Accepting peer connection..." << std::endl;
    // TODO: Accept connection from Pi-B and spawn reader thread
}

void PiAService::processIncoming(const protocol::JsonMessage& message)
{
    std::cout << "[PiAService::processIncoming] Processing message from device: " << message.getDevice() << std::endl;
    dispatcher_->dispatch(message);
}

void PiAService::sendAck(const std::string& status)
{
    std::cout << "[PiAService::sendAck] Sending ACK: " << status << std::endl;
    // TODO: Send acknowledgement to Pi-B
}

} // namespace service
