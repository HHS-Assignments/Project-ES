#include "../protocol/JsonMessage.h"
#include "../dispatch/WmosHandler.h"
#include "../dispatch/UnknownHandler.h"
#include "../dispatch/DeviceDispatcher.h"
#include <iostream>
#include <cassert>

using namespace protocol;
using namespace dispatch;

/**
 * Basic unit tests for the Socket C++ architecture.
 */

void testJsonMessage()
{
    std::cout << "[test] JsonMessage..." << std::endl;
    JsonMessage msg;
    
    msg.setDevice("test_device");
    msg.setSensor("temperature");
    msg.setData("25.0");

    assert(msg.isValid());
    assert(msg.getDevice() == "test_device");
    assert(msg.getSensor() == "temperature");
    assert(msg.getData() == "25.0");

    std::string serialized = msg.serialize();
    assert(!serialized.empty());

    std::cout << "  ✓ JsonMessage tests passed" << std::endl;
}

void testWmosHandler()
{
    std::cout << "[test] WmosHandler..." << std::endl;
    WmosHandler handler("wmos_1");
    
    JsonMessage msg;
    msg.setDevice("wmos_1");
    msg.setSensor("motion");
    msg.setData("detected");

    // Should not throw
    handler.handle(msg);

    assert(handler.getDeviceName() == "wmos_1");
    std::cout << "  ✓ WmosHandler tests passed" << std::endl;
}

void testUnknownHandler()
{
    std::cout << "[test] UnknownHandler..." << std::endl;
    UnknownHandler handler;

    JsonMessage msg;
    msg.setDevice("unknown");
    msg.setSensor("unknown");
    msg.setData("unknown");

    // Should not throw
    handler.handle(msg);

    assert(handler.isFallbackMode());
    std::cout << "  ✓ UnknownHandler tests passed" << std::endl;
}

void testDeviceDispatcher()
{
    std::cout << "[test] DeviceDispatcher..." << std::endl;
    DeviceDispatcher dispatcher;

    auto wmosHandler = std::make_unique<WmosHandler>("wmos_device");
    dispatcher.registerHandler("wmos_device", wmosHandler.get());

    assert(dispatcher.getHandlerCount() == 1);

    JsonMessage msg;
    msg.setDevice("wmos_device");
    msg.setSensor("sensor1");
    msg.setData("value1");

    // Should route to registered handler
    dispatcher.dispatch(msg);

    // Should handle unknown
    JsonMessage unknownMsg;
    unknownMsg.setDevice("unknown_device");
    unknownMsg.setSensor("sensor2");
    unknownMsg.setData("value2");
    dispatcher.dispatch(unknownMsg);

    std::cout << "  ✓ DeviceDispatcher tests passed" << std::endl;
}

int main(int argc, char* argv[])
{
    std::cout << "=== Running Basic Unit Tests ===" << std::endl;

    try {
        testJsonMessage();
        testWmosHandler();
        testUnknownHandler();
        testDeviceDispatcher();

        std::cout << "\n=== All tests passed! ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
