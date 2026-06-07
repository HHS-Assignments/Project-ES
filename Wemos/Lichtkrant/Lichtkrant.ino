#include <ESP8266WiFi.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <string.h>
#include "secrets.h"

// Display hardware
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES   4
#define CS_PIN        D8

// Netwerk
const char *const piHost           = "10.42.0.1";
const int         piPort           = 9000;
const int         wemosReceivePort = 9001;
const char *const ssid             = WIFI_SSID;
const char *const password         = WIFI_PASSWORD;
const int         max_attempts     = WIFI_MAX_ATTEMPTS;

#define INCOMING_BUF_SIZE 256

MD_Parola display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
WiFiServer receiveServer(wemosReceivePort);

// Belangrijke variabelen
bool noodModus = false;

char normaleTekst[128] = "Welkom bij L&B";
char noodTekst[128]    = "NOOD: volg de vluchtrouteverlichting!";

char normaalBuffer[128] = "";  // bouwt meerdelige normale tekst op
char noodBuffer[128]    = "";  // bouwt meerdelige noodtekst op

void setModus(bool nood) {
    noodModus = nood;
    display.displayClear();
    display.displayScroll(
        noodModus ? noodTekst : normaleTekst,
        PA_LEFT, PA_SCROLL_LEFT, 50
    );
}

void verwerk_tekst_deel(const char *incoming, char *doelBuffer, char *doelTekst, bool isNood) {
    // Zoek de tekstwaarde na "Data":"
    const char *dataPtr = strstr(incoming, "\"Data\":\"");
    if (dataPtr == nullptr) return;
    dataPtr += 8; // sla "Data":" over

    // Zoek het sluitende aanhalingsteken
    const char *einde = strchr(dataPtr, '"');
    if (einde == nullptr) return;

    // Plak dit deel aan de buffer
    size_t len = einde - dataPtr;
    strncat(doelBuffer, dataPtr, len);

    // Controleer of er een vervolg komt
    const char *morePtr = strstr(incoming, "\"More\":");
    int more = (morePtr != nullptr) ? atoi(morePtr + 7) : 0;

    if (more == 0) {
        // Laatste deel: kopieer buffer naar de weergavetekst en refresh display
        strncpy(doelTekst, doelBuffer, 127);
        doelTekst[127] = '\0';
        doelBuffer[0]  = '\0'; // reset buffer voor volgende keer
        if (isNood == noodModus) setModus(noodModus); // herlaad alleen als actieve modus
    }
}

static bool read_client_line(WiFiClient *client, char *buffer, size_t bufferSize) {
  size_t length = 0;
  unsigned long timeoutStart = millis();

  while (client->connected() && (millis() - timeoutStart) < 1000) {
    while (client->available()) {
      char c = static_cast<char>(client->read());

      if (c == '\r') {
        continue;
      }

      if (c == '\n') {
        buffer[length] = '\0';
        return length > 0;
      }

      if (length + 1 < bufferSize) {
        buffer[length++] = c;
      }

      timeoutStart = millis();
    }

    delay(1);
  }

  buffer[length] = '\0';
  return length > 0;
}

static void verwerk_commando(const char *incoming) {

    // Noodsituatie activeren/deactiveren (0x001 of 0x210)
    if (strstr(incoming, "0x001") || strstr(incoming, "0x210")) {
        const char *dataPtr = strstr(incoming, "\"Data\":");
        int waarde = (dataPtr != nullptr) ? atoi(dataPtr + 7) : 1;
        setModus(waarde == 1);
        return;
    }

    // Noodtekst ontvangen in delen (0x150 = deel 1, 0x160 = deel 2, 0x170 = deel 3)
    if (strstr(incoming, "0x150")) {
        noodBuffer[0] = '\0'; // reset buffer bij eerste deel
        verwerk_tekst_deel(incoming, noodBuffer, noodTekst, true);
        return;
    }
    if (strstr(incoming, "0x160") || strstr(incoming, "0x170")) {
        verwerk_tekst_deel(incoming, noodBuffer, noodTekst, true);
        return;
    }

    // Normale tekst ontvangen in delen (0x180 = deel 1, 0x190 = deel 2, 0x101 = deel 3)
    if (strstr(incoming, "0x180")) {
        normaalBuffer[0] = '\0'; // reset buffer bij eerste deel
        verwerk_tekst_deel(incoming, normaalBuffer, normaleTekst, false);
        return;
    }
    if (strstr(incoming, "0x190") || strstr(incoming, "0x101")) {
        verwerk_tekst_deel(incoming, normaalBuffer, normaleTekst, false);
        return;
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

    display.begin();
    display.setIntensity(5);
    display.setPause(0);
    display.displayClear();

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

    // Laad starttekst op display
    setModus(false);
}

void loop() {
    static unsigned long vorigeWifiCheck = 0;

    handle_incoming_command();

    // Laat de display animatie doorlopen
    if (display.displayAnimate()) {
        display.displayReset();
    }

    // Controleer WiFi elke 5 seconden
    if (millis() - vorigeWifiCheck >= 5000) {
        vorigeWifiCheck = millis();
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println(F("WiFi verloren, opnieuw verbinden..."));
            WiFi.disconnect();
            WiFi.begin(ssid, password);
        }
    }
}