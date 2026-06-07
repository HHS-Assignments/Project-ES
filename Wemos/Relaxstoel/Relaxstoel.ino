#include <ESP8266WiFi.h>
#include <stdio.h>
#include <string.h>
#include "secrets.h"

// API init
const char *const piHost        = "10.42.0.1";
const int         piPort        = 9000;
const int         wemosReceivePort = 9001;
const char *const ssid          = WIFI_SSID;
const char *const password      = WIFI_PASSWORD;
const int         max_attempts  = WIFI_MAX_ATTEMPTS;

#define JSON_BUF_SIZE     256
#define INCOMING_BUF_SIZE 256

WiFiServer receiveServer(wemosReceivePort);

const uint16_t MIN_PULSE = 0;
const uint16_t MAX_PULSE = 1000;

// Schermerlamp +  Motor
class Motor {
public:
    Motor(uint8_t pin) : _pin(pin) {}
    void begin() { pinMode(_pin, OUTPUT); }
    void setSpeed(uint8_t pct) {
        if (pct > 100) pct = 100;
        uint16_t pulse = (pct == 0) ? 0 : MIN_PULSE + ((pct * (MAX_PULSE - MIN_PULSE)) / 100);
        analogWrite(_pin, pulse);
    }
private:
    uint8_t _pin;
};

class Schemerlamp {
public:
    Schemerlamp(uint8_t ldrPin, uint8_t ledPin, int drempel = 500)
        : _ldrPin(ldrPin), _ledPin(ledPin), _drempel(drempel),
          _lampAan(false), _ldrWaarde(0) {}
    void begin() { pinMode(_ledPin, OUTPUT); digitalWrite(_ledPin, LOW); }
    void update() {
        _ldrWaarde = analogRead(_ldrPin);
        _lampAan = (_ldrWaarde > _drempel);
        digitalWrite(_ledPin, _lampAan ? HIGH : LOW);
    }
    int getLdrWaarde() { return _ldrWaarde; }
private:
    uint8_t _ldrPin, _ledPin;
    int     _drempel;
    bool    _lampAan;
    int     _ldrWaarde;
};

// Instanties Motor + Schemerlamp
Motor       motor(D2);
Schemerlamp schemerlamp(A0, D5, 100);
bool        motorAan = false;

// leest 1 regel van een TCP-client
static bool read_client_line(WiFiClient *client, char *buffer, size_t bufferSize) {
    size_t length = 0;
    unsigned long timeoutStart = millis();
    while (client->connected() && (millis() - timeoutStart) < 1000) {
        while (client->available()) {
            char c = static_cast<char>(client->read());
            if (c == '\r') continue;
            if (c == '\n') { buffer[length] = '\0'; return length > 0; }
            if (length + 1 < bufferSize) buffer[length++] = c;
            timeoutStart = millis();
        }
        delay(1);
    }
    buffer[length] = '\0';
    return length > 0;
}

// stuurt JSON als TCP-pakket naar de Pi
static bool post_payload(const char *json_payload) {
    WiFiClient client;
    if (!client.connect(piHost, (uint16_t)piPort)) {
        Serial.println(F("[SendToPi] Connection failed"));
        return false;
    }
    client.print(json_payload);
    client.print("\n");
    unsigned long t0 = millis();
    while (client.connected() && (millis() - t0) < 1000) {
        while (client.available()) { (void)client.read(); t0 = millis(); }
        delay(1);
    }
    client.stop();
    return true;
}

// bouwt JSON met CAN_ID formaat en verstuurt via post_payload
bool sendCanJson(const char *can_id, int data) {
    char json[JSON_BUF_SIZE];
    snprintf(json, sizeof(json), "{\"CAN_ID\":\"%s\",\"Data\":%d}", can_id, data);
    return post_payload(json);
}

static void verwerk_commando(const char *incoming) {
    if (strstr(incoming, "0x140") == nullptr) return;

    const char *dataPtr = strstr(incoming, "\"Data\":");
    if (dataPtr == nullptr) return;

    int waarde = atoi(dataPtr + 7);

    if (waarde == 1 && !motorAan) {
        motorAan = true;
        motor.setSpeed(50);
        Serial.println(F("[0x140] Relaxstoel AAN"));
        sendCanJson("0x430", 1);
    } else if (waarde == 0 && motorAan) {
        motorAan = false;
        motor.setSpeed(0);
        Serial.println(F("[0x140] Relaxstoel UIT"));
        sendCanJson("0x430", 0);
    }
}

static bool handle_incoming_command() {
    WiFiClient clientObject = receiveServer.available();
    if (!clientObject) return false;
    WiFiClient *client = &clientObject;
    char incoming[INCOMING_BUF_SIZE];
    if (read_client_line(client, incoming, sizeof(incoming))) {
        Serial.print(F("[ReceiveFromPi] ")); Serial.println(incoming);
        client->println(F("ACK"));
        verwerk_commando(incoming);
    }
    client->stop();
    return true;
}

void setup() {
  Serial.begin(9600);
    delay(100);

    motor.begin();
    schemerlamp.begin();

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < max_attempts) {
        delay(500); Serial.print("."); attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print(F("Verbonden! IP: ")); Serial.println(WiFi.localIP());
        receiveServer.begin();
        Serial.println(F("TCP server gestart op poort 9001"));
    } else {
        Serial.println(F("WiFi verbinding mislukt!"));
    }
}

void loop() {
  static unsigned long vorigeLdrTijd = 0;
    static unsigned long vorigeWifiCheck = 0;

    handle_incoming_command();
    schemerlamp.update();

    unsigned long nu = millis();

    // Stuur LDR elke 500ms
    if (nu - vorigeLdrTijd >= 500) {
        vorigeLdrTijd = nu;
        if (WiFi.status() == WL_CONNECTED) {
            int ldr = schemerlamp.getLdrWaarde();
            Serial.print(F("[0x400] LDR: ")); Serial.println(ldr);
            sendCanJson("0x400", ldr);
        }
    }

    // Controleer WiFi elke 5 seconden
    if (nu - vorigeWifiCheck >= 5000) {
        vorigeWifiCheck = nu;
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println(F("WiFi verloren, opnieuw verbinden..."));
            WiFi.disconnect();
            WiFi.begin(ssid, password);
        }
    }
}
