#include "UnknownHandler.h"
#include "../protocol/JsonMessage.h"
#include <iostream>

namespace dispatch {

UnknownHandler::UnknownHandler(bool fallbackMode)
    : fallbackMode_(fallbackMode)
{
}

void UnknownHandler::handle(const protocol::JsonMessage& message)
{
    std::cout << "[UnknownHandler::handle] Unknown device: " << message.getDevice()
              << " Sensor: " << message.getSensor()
              << " Data: " << message.getData() << std::endl;
    // TODO: Implement fallback/logging logic
}

} // namespace dispatch
