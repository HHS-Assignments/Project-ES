#include "DeviceDispatcher.h"
#include "UnknownHandler.h"
#include "../protocol/JsonMessage.h"
#include <iostream>

namespace dispatch {

DeviceDispatcher::DeviceDispatcher()
{
}

DeviceDispatcher::~DeviceDispatcher()
{
    // Note: We don't own the handlers, so we don't delete them
    handlers_.clear();
}

void DeviceDispatcher::registerHandler(const std::string& device, IDeviceHandler* handler)
{
    if (handler) {
        handlers_[device] = handler;
        std::cout << "[DeviceDispatcher::registerHandler] Registered handler for device: " << device << std::endl;
    }
}

void DeviceDispatcher::dispatch(const protocol::JsonMessage& message)
{
    std::string device = message.getDevice();
    auto it = handlers_.find(device);

    if (it != handlers_.end()) {
        it->second->handle(message);
    } else {
        handleUnknown(message);
    }
}

void DeviceDispatcher::handleUnknown(const protocol::JsonMessage& message)
{
    std::cout << "[DeviceDispatcher::handleUnknown] No handler for device: " << message.getDevice() << std::endl;
    // TODO: Could use a default UnknownHandler here
}

} // namespace dispatch
