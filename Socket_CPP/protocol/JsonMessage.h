#ifndef PROTOCOL_JSONMESSAGE_H
#define PROTOCOL_JSONMESSAGE_H

#include <string>

namespace protocol {

/**
 * Represents a JSON message exchanged between Pi-A and Pi-B.
 * Encapsulates device name, sensor type, and data payload.
 */
class JsonMessage {
private:
    std::string device_;
    std::string sensor_;
    std::string data_;
    std::string raw_;

public:
    JsonMessage();
    ~JsonMessage() = default;

    /**
     * Parse a JSON string and populate message fields.
     * Returns true on success, false on parse error.
     */
    bool parse(const std::string& text);

    /**
     * Serialize message to JSON string format.
     */
    std::string serialize() const;

    /**
     * Validate message fields (non-empty device, sensor, data).
     */
    bool isValid() const;

    // Getters and setters
    void setDevice(const std::string& device) { device_ = device; }
    void setSensor(const std::string& sensor) { sensor_ = sensor; }
    void setData(const std::string& data) { data_ = data; }
    void setRaw(const std::string& raw) { raw_ = raw; }

    std::string getDevice() const { return device_; }
    std::string getSensor() const { return sensor_; }
    std::string getData() const { return data_; }
    std::string getRaw() const { return raw_; }
};

} // namespace protocol

#endif // PROTOCOL_JSONMESSAGE_H
