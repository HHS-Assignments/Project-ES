#ifndef DISPATCH_DEVICEDISPATCHER_H
#define DISPATCH_DEVICEDISPATCHER_H

#include "IDeviceHandler.h"
#include <map>
#include <string>
#include <memory>

namespace protocol {
class JsonMessage;  // Forward declaration
}

namespace dispatch {

/**
 * Central dispatcher that routes messages to device-specific handlers.
 * Maintains a registry of handlers keyed by device name.
 */
class DeviceDispatcher {
private:
    std::map<std::string, IDeviceHandler*> handlers_;

public:
    DeviceDispatcher();
    ~DeviceDispatcher();

    /**
     * Register a handler for a device type.
     */
    void registerHandler(const std::string& device, IDeviceHandler* handler);

    /**
     * Dispatch a message to the appropriate handler.
     */
    void dispatch(const protocol::JsonMessage& message);

    /**
     * Handle message when no handler is registered (fallback).
     */
    void handleUnknown(const protocol::JsonMessage& message);

    size_t getHandlerCount() const { return handlers_.size(); }
};

} // namespace dispatch

#endif // DISPATCH_DEVICEDISPATCHER_H
