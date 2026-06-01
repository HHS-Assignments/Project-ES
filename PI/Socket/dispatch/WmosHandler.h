#ifndef DISPATCH_WMOSHANDLER_H
#define DISPATCH_WMOSHANDLER_H

#include "IDeviceHandler.h"
#include <string>

namespace dispatch {

/**
 * Handler for WMos WiFi-based device messages.
 * Processes sensor data from wireless modules.
 */
class WmosHandler : public IDeviceHandler {
private:
    std::string deviceName_;

public:
    WmosHandler(const std::string& deviceName = "wmos");
    ~WmosHandler() override = default;

    void handle(const protocol::JsonMessage& message) override;

    std::string getDeviceName() const { return deviceName_; }
};

} // namespace dispatch

#endif // DISPATCH_WMOSHANDLER_H
