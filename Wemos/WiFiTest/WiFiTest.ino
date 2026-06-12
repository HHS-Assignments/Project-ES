// WiFiTest — ESP8266 & XIAO ESP32C6
// Test WiFi verbinding + TCP server op poort 9001.
// Gebruik: upload, open Serial Monitor (115200 baud), controleer output.
// Stuurt elke 10s een testbericht naar de Pi (poort 9000).
// Luistert op poort 9001 en print alles wat binnenkomt.

#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif

 #if __has_include("secrets.h")
 #include "secrets.h"
 #else
 #error "Missing secrets.h. Create Wemos/WiFiTest/secrets.h (or copy from an example) and set WIFI_SSID/WIFI_PASSWORD."
 #endif

// ---- Instellingen ----
static const char *PI_HOST   = "10.42.0.103";
static const uint16_t PI_PORT_OUT = 9000;
static const uint16_t MY_PORT_IN  = 9001;

// Statisch IP — false = DHCP (test dit eerst!), true = 10.42.0.101 (Relaxstoel)
#define USE_STATIC_IP false

WiFiServer server(MY_PORT_IN);

unsigned long vorigeSend  = 0;
unsigned long vorigeCheck = 0;
int pingTeller = 0;

// ----------------------------------------------------------------
void printStatus() {
    Serial.print(F("[WiFi] Status: "));
    switch (WiFi.status()) {
        case WL_IDLE_STATUS:     Serial.println(F("IDLE (0) — bezig te verbinden")); break;
        case WL_NO_SSID_AVAIL:   Serial.println(F("NO_SSID (1) — netwerk niet gevonden")); break;
        case WL_SCAN_COMPLETED:  Serial.println(F("SCAN_DONE (2)")); break;
        case WL_CONNECTED:       Serial.print(F("CONNECTED (3) IP="));
                                 Serial.print(WiFi.localIP());
                                 Serial.print(F("  RSSI="));
                                 Serial.print(WiFi.RSSI());
                                 Serial.println(F(" dBm")); break;
        case WL_CONNECT_FAILED:  Serial.println(F("FAILED (4) — verkeerd wachtwoord?")); break;
        case WL_CONNECTION_LOST: Serial.println(F("LOST (5)")); break;
        case WL_DISCONNECTED:    Serial.println(F("DISCONNECTED (6)")); break;
        default:                 Serial.println(WiFi.status()); break;
    }
}

// ----------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println(F("\n\n=== WiFiTest ==="));

#if USE_STATIC_IP
    Serial.println(F("[WiFi] Statisch IP: 10.42.0.101"));
    IPAddress ip(10, 42, 0, 101);
    IPAddress gw(10, 42, 0, 1);
    IPAddress sn(255, 255, 255, 0);
    IPAddress dns(10, 42, 0, 1);
    WiFi.config(ip, gw, sn, dns);
#else
    Serial.println(F("[WiFi] DHCP"));
#endif

    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print(F("[WiFi] Verbinden met "));
    Serial.println(WIFI_SSID);

    int pogingen = 0;
    while (WiFi.status() != WL_CONNECTED && pogingen < 20) {
        delay(500);
        Serial.print(".");
        pogingen++;
    }
    Serial.println();
    printStatus();

    if (WiFi.status() == WL_CONNECTED) {
        server.begin();
        Serial.print(F("[TCP] Server luistert op poort "));
        Serial.println(MY_PORT_IN);
    }
}

// ----------------------------------------------------------------
void loop() {
    unsigned long nu = millis();

    // --- Elke seconde: toon WiFi-status ---
    if (nu - vorigeCheck >= 1000) {
        vorigeCheck = nu;
        printStatus();
    }

    // --- Elke 10 seconden: stuur testbericht naar Pi ---
    if (nu - vorigeSend >= 10000 && WiFi.status() == WL_CONNECTED) {
        vorigeSend = nu;
        pingTeller++;
        WiFiClient client;
        Serial.print(F("[TCP] Stuur ping #"));
        Serial.print(pingTeller);
        Serial.print(F(" naar "));
        Serial.print(PI_HOST);
        Serial.print(F(":"));
        Serial.println(PI_PORT_OUT);
        if (client.connect(PI_HOST, PI_PORT_OUT)) {
            char buf[64];
            snprintf(buf, sizeof(buf), "{\"CAN_ID\":\"0x400\",\"Data\":%d}", pingTeller);
            client.print(buf);
            client.print("\n");
            delay(200);
            client.stop();
            Serial.println(F("[TCP] Ping verzonden OK"));
        } else {
            Serial.println(F("[TCP] Ping mislukt — Pi bereikbaar?"));
        }
    }

    // --- Luister op poort 9001 ---
    if (WiFi.status() == WL_CONNECTED) {
        WiFiClient incoming = server.available();
        if (incoming) {
            Serial.println(F("[TCP] Inkomende verbinding!"));
            unsigned long t0 = millis();
            String regel = "";
            while (incoming.connected() && millis() - t0 < 2000) {
                while (incoming.available()) {
                    char c = (char)incoming.read();
                    if (c == '\n') {
                        regel.trim();
                        Serial.print(F("[TCP] Ontvangen: "));
                        Serial.println(regel);
                        incoming.print(F("ACK\n"));
                        regel = "";
                    } else if (c != '\r') {
                        regel += c;
                    }
                    t0 = millis();
                }
                delay(1);
            }
            incoming.stop();
        }
    }
}
