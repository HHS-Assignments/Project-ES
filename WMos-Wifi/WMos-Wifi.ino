/**
 * @file WMos-Wifi.ino
 * @brief WeMos D1 Mini WiFi Button Controller
 * @details Connects a button on WeMos D1 Mini GPIO4 (D2) to WiFi and sends
 *          JSON POST requests to Pi-1 HTTP receiver when button is pressed.
 *          Uses debouncing to handle mechanical button bounce.
 * @author Project-ES
 * @date 2026
 * @version 1.0
 */

#include <ESP8266WiFi.h>

#include <stdio.h>
#include <string.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing secrets.h. Copy WMos-Wifi/secrets.h.example to WMos-Wifi/secrets.h and set WIFI_SSID/WIFI_PASSWORD/WIFI_MAX_ATTEMPTS."
#endif

#ifndef WIFI_SSID
#error "WIFI_SSID is not defined in secrets.h"
#endif

#ifndef WIFI_PASSWORD
#error "WIFI_PASSWORD is not defined in secrets.h"
#endif

#ifndef WIFI_MAX_ATTEMPTS
#error "WIFI_MAX_ATTEMPTS is not defined in secrets.h"
#endif

/** @brief JSON payload buffer size (bytes) */
#define JSON_BUF_SIZE 256
/** @brief HTTP request buffer size (bytes) */
#define HTTP_BUF_SIZE 512

/** @brief Pi-1 HTTP receiver hostname (updated to match project network) */
const char *const piHost = "172.16.0.80";
/** @brief Pi-1 HTTP receiver port number */
const int piPort = 9000;
/** @brief WiFi SSID loaded from local secrets.h */
const char* const ssid = WIFI_SSID;
/** @brief WiFi password loaded from local secrets.h */
const char* const password = WIFI_PASSWORD;
/** @brief Maximum WiFi connection attempts (500ms per attempt) */
const int max_attempts = WIFI_MAX_ATTEMPTS;


/**
 * @brief Sends HTTP POST request with JSON payload to Pi-1 receiver
 * @param[in] json_payload Pointer to null-terminated JSON string to send
 * @details Constructs HTTP/1.1 POST request with proper headers and sends
 *          to piHost:piPort. Waits up to 1 second for response before closing.
 *          On error, prints debug message to Serial.
 * @return void
 */
static void post_payload(const char *json_payload) {
  WiFiClient client;
  char request[HTTP_BUF_SIZE];
  int body_len = (int)strlen(json_payload);

  int n = snprintf(request, sizeof(request),
                   "POST / HTTP/1.1\r\n"
                   "Host: %s:%d\r\n" 
                   "Content-Type: application/json\r\n"
                   "Content-Length: %d\r\n"
                   "Connection: close\r\n"
                   "\r\n"
                   "%s",
                   piHost, piPort, body_len, json_payload);

  if (n < 0 || n >= (int)sizeof(request)) {
    Serial.println("[SendJsonToPi] Payload too large");
    return;
  }

  if (!client.connect(piHost, (uint16_t)piPort)) {
    Serial.println("[SendJsonToPi] Connection failed");
    return;
  }

  client.write((const uint8_t *)request, (size_t)n);

  unsigned long t0 = millis();
  while (client.connected() && (millis() - t0) < 1000) {
    while (client.available()) {
      (void)client.read();
      t0 = millis();
    }
    delay(1);
  }

  client.stop();
}

/**
 * @brief Sends JSON POST with string data to Pi-1 receiver
 * @param[in] device Device identifier string (e.g., "Wmos")
 * @param[in] sensor Sensor/component name (e.g., "ButtonD2")
 * @param[in] data String data value to transmit
 * @details Constructs JSON object with string data field and sends via HTTP POST
 * @return void
 */
void SendJsonToPi_str(const char *device, const char *sensor,
                      const char *data) {
  char json[JSON_BUF_SIZE];
  snprintf(json, sizeof(json),
           "{\"Device\":\"%s\",\"Sensor\":\"%s\",\"Data\":\"%s\"}",
           device, sensor, data);
  post_payload(json);
}

/**
 * @brief Sends JSON POST with integer data to Pi-1 receiver
 * @param[in] device Device identifier string (e.g., "Wmos")
 * @param[in] sensor Sensor/component name (e.g., "ButtonD2")
 * @param[in] data Integer data value to transmit
 * @details Constructs JSON object with numeric data field and sends via HTTP POST
 * @return void
 */
void SendJsonToPi_int(const char *device, const char *sensor, int data) {
  char json[JSON_BUF_SIZE];
  snprintf(json, sizeof(json),
           "{\"Device\":\"%s\",\"Sensor\":\"%s\",\"Data\":%d}",
           device, sensor, data);
  post_payload(json);
}

/**
 * @brief Initialize system: Serial, WiFi, button pin
 * @details Sets up serial debugging, WiFi connection using credentials from
 *          secrets.h, configures button pin as INPUT_PULLUP, and prints
 *          connection status and current POST target to Serial.
 * @return void
 */
void setup() {
  /** @brief Button pin number (WeMos D1 Mini D2 = GPIO4) */
  const int BUTTON_PIN = 2;

  // Initialize serial communication at 9600 baud
  Serial.begin(9600);
  delay(100);

  // Configure button pin with internal pull-up resistor
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println("\n\n");
  Serial.println("================================");
  Serial.println("WiFi Connection Starting...");
  Serial.println("================================");

  // Set WiFi mode to station
  WiFi.mode(WIFI_STA);

  // Start WiFi connection
  Serial.print("Connecting to WiFi network: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // Wait for WiFi connection with timeout (~7.5 seconds at 500ms intervals)
  int attempts     = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < max_attempts) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println();

  // Check if connection was successful
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("================================");
    Serial.println("WiFi Connected Successfully!");
    Serial.println("================================");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("POST target: ");
    Serial.print(piHost);
    Serial.print(":");
    Serial.println(piPort);
    Serial.println("================================");
  } else {
    Serial.println("================================");
    Serial.println("Failed to connect to WiFi!");
    Serial.println("================================");
  }
}

/**
 * @brief Main loop: read button with debouncing and send JSON on press
 * @details Implements debounce logic to properly handle mechanical button bounce.
 *          On button press (LOW state), increments press counter and sends JSON
 *          POST to Pi-1 HTTP receiver. Uses function-local static variables to
 *          persist button debounce state between loop iterations. Monitors WiFi
 *          connection and alerts on loss.
 *          Loop frequency: ~100 Hz (10ms base delay)
 * @return void
 */
void loop() {
  /** @brief Button pin number (WeMos D1 Mini D2 = GPIO4) */
  const int BUTTON_PIN = 2;
  /** @brief Debounce delay in milliseconds */
  const unsigned long debounceDelay = 50;
  /** @brief Persistent debounce timestamp */
  static unsigned long lastDebounceTime = 0;
  /** @brief Previous stable button state */
  static int lastButtonState = HIGH;
  /** @brief Current stable button state */
  static int buttonState = HIGH;
  /** @brief Button press counter sent as JSON data */
  static int pressCount = 0;

  // Read button state with debounce algorithm
  int reading = digitalRead(BUTTON_PIN);

  // Detect state change: restart debounce timer
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  // Check if debounce delay elapsed
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // If state has stabilized and changed
    if (reading != buttonState) {
      buttonState = reading;

      // Button pressed: state LOW (INPUT_PULLUP logic)
      if (buttonState == LOW) {
        pressCount++;
        Serial.print("Button pressed! Count=");
        Serial.println(pressCount);
        Serial.println("Sending JSON POST...");
        SendJsonToPi_int("Wmos", "ButtonD2", pressCount);
      }
    }
  }

  lastButtonState = reading;

  // Monitor WiFi connection and alert on loss
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost!");
    delay(5000);
  }

  // Main loop delay for non-blocking operation
  delay(10);
}
