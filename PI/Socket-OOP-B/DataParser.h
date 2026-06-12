#pragma once
#include <string>

// Valideert JSON-regels die Pi B doorstuurt. Pi B converteert niets —
// het formaat is in beide richtingen al het Wemos-formaat:
//   {"CAN_ID":"0x001","Data":1}  of  {"CAN_ID":"0x180","Data":"tekst"}
class DataParser {
public:
    // true als de regel een bruikbaar CAN-JSON-bericht is
    bool isValid(const std::string &line) const;
};
