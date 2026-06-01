#include "JsonMessage.h"
#include <sstream>
#include <iostream>
#include <regex>

namespace protocol {

JsonMessage::JsonMessage()
    : device_(""), sensor_(""), data_(""), raw_("")
{
}

bool JsonMessage::parse(const std::string& text)
{
    raw_ = text;
    
    // Simple JSON parsing: extract values from {"device":"X", "sensor":"Y", "data":"Z"}
    // Or key=value format: device=X&sensor=Y&data=Z
    
    // Try JSON format first
    size_t device_pos = text.find("\"device\"");
    if (device_pos != std::string::npos) {
        // JSON format
        auto extract_value = [&text](const std::string& key) -> std::string {
            std::string search_str = "\"" + key + "\":\"";
            size_t pos = text.find(search_str);
            if (pos != std::string::npos) {
                pos += search_str.length();
                size_t end = text.find("\"", pos);
                if (end != std::string::npos) {
                    return text.substr(pos, end - pos);
                }
            }
            return "";
        };
        
        device_ = extract_value("device");
        sensor_ = extract_value("sensor");
        data_ = extract_value("data");
    } else {
        // Try key=value format: device=X&sensor=Y&data=Z
        std::istringstream iss(text);
        std::string pair;
        
        while (std::getline(iss, pair, '&')) {
            size_t eq = pair.find('=');
            if (eq != std::string::npos) {
                std::string key = pair.substr(0, eq);
                std::string value = pair.substr(eq + 1);
                
                if (key == "device") device_ = value;
                else if (key == "sensor") sensor_ = value;
                else if (key == "data") data_ = value;
            }
        }
    }
    
    if (!device_.empty()) {
        std::cout << "[JsonMessage::parse] Parsed - Device: " << device_ 
                  << " | Sensor: " << sensor_ << " | Data: " << data_ << std::endl;
        return true;
    }
    
    return false;
}

std::string JsonMessage::serialize() const
{
    // Output as JSON format
    std::ostringstream oss;
    oss << "{\"device\":\"" << device_ 
        << "\",\"sensor\":\"" << sensor_ 
        << "\",\"data\":\"" << data_ << "\"}";
    return oss.str();
}

bool JsonMessage::isValid() const
{
    return !device_.empty() && !sensor_.empty() && !data_.empty();
}

} // namespace protocol

