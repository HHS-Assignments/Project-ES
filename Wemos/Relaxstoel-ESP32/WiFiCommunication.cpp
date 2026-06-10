#include "WiFiCommunication.h"
#include <WiFi.h>
#include <stdio.h>
#include <string.h>
#include <Arduino.h>

WiFiCommunication::WiFiCommunication(const char *piHost, int piPort, int receivePort)
    : _piHost(piHost), _piPort(piPort), _server(receivePort) {}

void WiFiCommunication::begin() {
    _server.begin();
    Serial.println(F("[WiFi] TCP server gestart"));
}

bool WiFiCommunication::connect(const char *ssid, const char *pass) {
    _ssid = ssid;   // bewaar voor reconnect()
    _pass = pass;
    WiFi.persistent(false);       // geen credentials naar NVS schrijven
    WiFi.setAutoReconnect(false); // eigen reconnect() gebruiken; auto-reconnect gooit statisch IP weg
    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_STA);

    IPAddress ip(10, 42, 0, 10);
    IPAddress gateway(10, 42, 0, 1);
    IPAddress subnet(255, 255, 255, 0);
    IPAddress dns(10, 42, 0, 1);
    WiFi.config(ip, gateway, subnet, dns);

    WiFi.begin(ssid, pass);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    if (isConnected()) {
        Serial.print(F("[WiFi] Verbonden! IP: "));
        Serial.println(WiFi.localIP());
        return true;
    }
    Serial.println(F("[WiFi] Verbinding mislukt!"));
    return false;
}

// Non-blocking herverbinding: start WiFi opnieuw zonder te wachten.
// update() blijft doorlopen; isConnected() signaleert wanneer verbonden.
void WiFiCommunication::reconnect() {
    if (_ssid == nullptr) return;
    // Wacht 30 seconden tussen pogingen zodat de stack tijd krijgt te verbinden
    if (millis() - _laatsHerverbind < 30000) return;
    _laatsHerverbind = millis();
    Serial.println(F("[WiFi] Herverbinden (non-blocking)..."));
    WiFi.begin(_ssid, _pass);
}

bool WiFiCommunication::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool WiFiCommunication::send(const char *data) {
    WiFiClient client;
    if (!client.connect(_piHost, (uint16_t)_piPort)) {
        Serial.println(F("[WiFi] send: verbinding mislukt"));
        return false;
    }
    client.print(data);
    client.print("\n");
    unsigned long t0 = millis();
    while (client.connected() && (millis() - t0) < 1000) {
        while (client.available()) { (void)client.read(); t0 = millis(); }
        delay(1);
    }
    client.stop();
    return true;
}

bool WiFiCommunication::receive(char *buf, size_t len) {
    WiFiClient client = _server.available();
    if (!client) return false;
    bool ok = _readClientLine(&client, buf, len);
    if (ok) {
        Serial.print(F("[WiFi] Ontvangen: "));
        Serial.println(buf);
        client.println(F("ACK"));
    }
    client.stop();
    return ok;
}

bool WiFiCommunication::sendCanJson(const char *canId, int data) {
    char json[256];
    snprintf(json, sizeof(json), "{\"CAN_ID\":\"%s\",\"Data\":%d}", canId, data);
    return send(json);
}

bool WiFiCommunication::_readClientLine(WiFiClient *client, char *buf, size_t len) {
    size_t n = 0;
    unsigned long t0 = millis();
    while (client->connected() && (millis() - t0) < 1000) {
        while (client->available()) {
            char c = (char)client->read();
            if (c == '\r') continue;
            if (c == '\n') { buf[n] = '\0'; return n > 0; }
            if (n + 1 < len) buf[n++] = c;
            t0 = millis();
        }
        delay(1);
    }
    buf[n] = '\0';
    return n > 0;
}
