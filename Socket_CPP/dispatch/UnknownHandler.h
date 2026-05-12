#ifndef DISPATCH_UNKNOWNHANDLER_H
#define DISPATCH_UNKNOWNHANDLER_H

#include "IDeviceHandler.h"

namespace dispatch {

/**
 * Fallback handler for unknown device types.
 * Provides default behavior when no specific handler is registered.
 */
class UnknownHandler : public IDeviceHandler {
private:
    bool fallbackMode_;

public:
    UnknownHandler(bool fallbackMode = true);
    ~UnknownHandler() override = default;

    void handle(const protocol::JsonMessage& message) override;

    bool isFallbackMode() const { return fallbackMode_; }
};

} // namespace dispatch

#endif // DISPATCH_UNKNOWNHANDLER_H
