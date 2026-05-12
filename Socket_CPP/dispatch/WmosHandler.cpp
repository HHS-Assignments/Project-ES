#include "WmosHandler.h"
#include "../protocol/JsonMessage.h"
#include <iostream>

namespace dispatch {

WmosHandler::WmosHandler(const std::string& deviceName)
    : deviceName_(deviceName)
{
}

void WmosHandler::handle(const protocol::JsonMessage& message)
{
    std::cout << "[WmosHandler::handle] Device: " << message.getDevice()
              << " Sensor: " << message.getSensor()
              << " Data: " << message.getData() << std::endl;
    // TODO: Implement WMos-specific handling logic
}

} // namespace dispatch
