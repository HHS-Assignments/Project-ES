#include "WiFiCommunication.h"
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
    _ssid = ssid;
    _pass = pass;

    // Zelfde volgorde als de werkende WiFiTest sketch:
    // config -> persistent -> autoReconnect -> mode -> begin
    // (geen WiFi.disconnect() vooraf; die verstoort de verbinding)
    IPAddress ip(10, 42, 0, 10);
    IPAddress gateway(10, 42, 0, 1);
    IPAddress subnet(255, 255, 255, 0);
    IPAddress dns(10, 42, 0, 1);
    WiFi.config(ip, gateway, subnet, dns);

    WiFi.persistent(false);       // geen credentials naar flash schrijven
    WiFi.setAutoReconnect(false); // eigen reconnect() gebruiken
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    Serial.print(F("[WiFi] Verbinden met "));
    Serial.println(ssid);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        Serial.print(WiFi.status());   // status per poging voor debugging
        attempts++;
    }
    Serial.println();
    if (isConnected()) {
        Serial.print(F("[WiFi] Verbonden! IP: "));
        Serial.print(WiFi.localIP());
        Serial.print(F("  RSSI: "));
        Serial.print(WiFi.RSSI());
        Serial.println(F(" dBm"));
        return true;
    }
    Serial.print(F("[WiFi] Verbinding mislukt! Eindstatus: "));
    Serial.println(WiFi.status());
    return false;
}

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
