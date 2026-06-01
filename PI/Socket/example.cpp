#include "protocol/JsonMessage.h"
#include "transport/SocketConnection.h"
#include "dispatch/WmosHandler.h"
#include "dispatch/UnknownHandler.h"
#include "dispatch/DeviceDispatcher.h"
#include "service/PiAService.h"
#include "service/PiBService.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

using namespace protocol;
using namespace transport;
using namespace dispatch;
using namespace service;

/**
 * Simple example demonstrating the C++ Socket architecture.
 * Shows how to create services, register handlers, and process messages.
 */
int main(int argc, char* argv[])
{
    std::cout << "=== Socket C++ Architecture Example ===" << std::endl;

    // Create a Pi-A service (receiver/dispatcher)
    std::cout << "\n[main] Creating PiAService..." << std::endl;
    PiAService piA;

    // Create a dispatcher and register handlers
    std::cout << "[main] Registering handlers..." << std::endl;
    auto wmosHandler = std::make_unique<WmosHandler>("wmos_device_1");
    auto unknownHandler = std::make_unique<UnknownHandler>();

    // Note: In this example, we can't directly access piA's dispatcher from outside
    // This would normally be done through the service itself or injected at startup

    // Create a Pi-B service (forwarder/HTTP gateway)
    std::cout << "\n[main] Creating PiBService..." << std::endl;
    PiBService piB;

    // Create sample messages
    std::cout << "\n[main] Creating sample messages..." << std::endl;
    JsonMessage msg1;
    msg1.setDevice("wmos_device_1");
    msg1.setSensor("temperature");
    msg1.setData("22.5");

    JsonMessage msg2;
    msg2.setDevice("unknown_device");
    msg2.setSensor("humidity");
    msg2.setData("45.0");

    // Demonstrate message serialization
    std::cout << "\n[main] Serializing messages..." << std::endl;
    std::cout << "Message 1: " << msg1.serialize() << std::endl;
    std::cout << "Message 2: " << msg2.serialize() << std::endl;

    // Demonstrate dispatcher
    std::cout << "\n[main] Demonstrating DeviceDispatcher..." << std::endl;
    DeviceDispatcher dispatcher;
    dispatcher.registerHandler("wmos_device_1", wmosHandler.get());
    dispatcher.dispatch(msg1);
    dispatcher.dispatch(msg2);  // Will be handled as unknown

    // Example of starting services (normally would run in threads)
    std::cout << "\n[main] Example service startup (not actually running)..." << std::endl;
    std::cout << "piA.start(5000);" << std::endl;
    std::cout << "piB.start(8080, \"127.0.0.1\", 5000);" << std::endl;

    std::cout << "\n=== Example Complete ===" << std::endl;
    return 0;
}
