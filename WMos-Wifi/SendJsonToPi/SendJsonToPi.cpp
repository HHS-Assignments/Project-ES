/**
 * @file SendJsonToPi.cpp
 * @brief Implementation of the SendJsonToPi library.
 *
 * Builds a JSON payload and sends it to Pi-1 as an HTTP POST request.
 * Requires the ESP8266WiFi and ESP8266HTTPClient libraries (bundled with the
 * ESP8266 Arduino board package).
 */

#include "SendJsonToPi.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

/* ── Default connection configuration ──────────────────────────────────── */

/** Default Pi-1 hostname / IP. Reassign in setup() to change. */
const char *piHost = "10.0.42.1";

/** Default Pi-1 TCP port. Reassign in setup() to change. */
int piPort = 9000;

/* ── Internal helper ────────────────────────────────────────────────────── */

/**
 * @brief Perform the actual HTTP POST with the given JSON payload.
 *
 * Checks WiFi connectivity before attempting the request and prints the
 * response (or error) to Serial.
 *
 * @param payload  Complete JSON string to POST.
 */
static void postPayload(const String &payload)
{
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[SendJsonToPi] WiFi not connected, cannot send.");
        return;
    }

    WiFiClient client;
    HTTPClient http;

    String url = "http://" + String(piHost) + ":" + String(piPort) + "/";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");

    Serial.print("[SendJsonToPi] Sending: ");
    Serial.println(payload);

    int httpCode = http.POST(payload);
    if (httpCode > 0) {
        Serial.printf("[SendJsonToPi] Response code: %d\n", httpCode);
        Serial.println(http.getString());
    } else {
        Serial.printf("[SendJsonToPi] Failed: %s\n",
                      http.errorToString(httpCode).c_str());
    }

    http.end();
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void SendJsonToPi(const String &device, const String &sensor,
                  const String &data)
{
    /* Data is a quoted JSON string. */
    String payload = "{\"Device\":\"" + device + "\","
                     "\"Sensor\":\"" + sensor + "\","
                     "\"Data\":\"" + data + "\"}";
    postPayload(payload);
}

void SendJsonToPi(const String &device, const String &sensor, int data)
{
    /* Data is an unquoted JSON number. */
    String payload = "{\"Device\":\"" + device + "\","
                     "\"Sensor\":\"" + sensor + "\","
                     "\"Data\":" + String(data) + "}";
    postPayload(payload);
}
