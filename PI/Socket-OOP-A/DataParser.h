#pragma once
#include <string>
#include "BusCommunication.h"

// Converteert CAN-berichten van/naar het Wemos JSON-formaat:
//   numeriek: {"CAN_ID":"0x001","Data":1}
//   tekst:    {"CAN_ID":"0x180","Data":"Hallo"}   (alle 8 bytes ASCII, nul-gepad)
class DataParser {
public:
    // CAN -> JSON-regel
    std::string messageToJson(const CanMessage &msg) const;

    // JSON-regel -> CAN. Geeft false bij ongeldige JSON.
    bool jsonToMessage(const std::string &line, CanMessage &msg) const;

    // Tekst-IDs: lichtkrant nood- (0x150/0x160/0x170) en normale tekst (0x180/0x190/0x191)
    static bool isTextId(uint32_t id);
};
