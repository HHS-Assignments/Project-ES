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
    WiFi.disconnect();   // ← reset vorige sessie
    delay(100);
    WiFi.mode(WIFI_STA);

    // Statisch IP instellen vóór WiFi.begin()
    IPAddress ip(10, 42, 0, 100);       // gewenst IP van de Wemos
    IPAddress gateway(10, 42, 0, 1);   // IP van de router/Pi
    IPAddress subnet(255, 255, 255, 0);
    IPAddress dns(10, 42, 0, 1);
    WiFi.config(ip, gateway, subnet, dns);

    Serial.print(F("[WiFi] SSID: ")); Serial.println(ssid);
    Serial.print(F("[WiFi] Vrij geheugen voor verbinden: "));
    Serial.print(ESP.getFreeHeap()); Serial.println(F(" bytes"));

#if defined(WIFI_DEBUG_SCAN)
    Serial.println(F("[WiFi] Netwerken scannen..."));
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
        Serial.print(F("  Gevonden: "));
        Serial.print(WiFi.SSID(i));
        Serial.print(F("  Kanaal: "));
        Serial.print(WiFi.channel(i));
        Serial.print(F("  RSSI: "));
        Serial.println(WiFi.RSSI(i));
    }
    WiFi.scanDelete();
 #endif

    WiFi.begin(ssid, pass);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        Serial.print(WiFi.status());
        attempts++;
    }
    Serial.println();
    if (isConnected()) {
        Serial.print(F("[WiFi] Verbonden! IP: "));
        Serial.println(WiFi.localIP());
        return true;
    }
    Serial.print(F("[WiFi] Verbinding mislukt! Eindstatus: "));
    Serial.println(WiFi.status());
    return false;
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
