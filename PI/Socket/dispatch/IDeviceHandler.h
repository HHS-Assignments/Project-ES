#ifndef DISPATCH_IDEVICEHANDLER_H
#define DISPATCH_IDEVICEHANDLER_H

namespace protocol {
class JsonMessage;  // Forward declaration
}

namespace dispatch {

/**
 * Abstract interface for device-specific message handlers.
 * Implementations handle dispatch of messages from specific device types.
 */
class IDeviceHandler {
public:
    virtual ~IDeviceHandler() = default;

    /**
     * Handle a message from a device.
     * Pure virtual; must be implemented by subclasses.
     */
    virtual void handle(const protocol::JsonMessage& message) = 0;
};

} // namespace dispatch

#endif // DISPATCH_IDEVICEHANDLER_H
