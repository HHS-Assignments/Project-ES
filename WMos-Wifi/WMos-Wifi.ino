/**
 * @file WMos-Wifi.ino
 * @brief WeMos D1 Mini WiFi Button Controller
 * @details Connects a button on WeMos D1 Mini GPIO4 (D2) to WiFi and sends
 *          raw JSON messages over TCP to Pi-1 receiver when button is pressed.
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

/** @brief Pi-1 TCP receiver hostname (updated to match project network) */
const char *const piHost = "10.42.0.1";
/** @brief Pi-1 TCP receiver port number */
const int piPort = 9000;
/** @brief WiFi SSID loaded from local secrets.h */
const char* const ssid = WIFI_SSID;
/** @brief WiFi password loaded from local secrets.h */
const char* const password = WIFI_PASSWORD;
/** @brief Maximum WiFi connection attempts (500ms per attempt) */
const int max_attempts = WIFI_MAX_ATTEMPTS;
/** @brief Wmos Device Name */
const char* const Device_Name = "Wmos-1";


/**
 * @brief Sends raw JSON payload to Pi-1 receiver over TCP
 * @param[in] json_payload Pointer to null-terminated JSON string to send
 * @details Opens a TCP connection to piHost:piPort and sends one JSON message
 *          terminated by '\n'. Waits up to 1 second for optional response data
 *          before closing. On error, prints debug message to Serial.
 * @return true when payload was written to socket, false on connection/send failure
 */
static bool post_payload(const char *json_payload) {
  WiFiClient client;

  if (!client.connect(piHost, (uint16_t)piPort)) {
    Serial.println("[SendJsonToPi] Connection failed");
    return false;
  }

  size_t payload_written = client.print(json_payload);
  size_t newline_written = client.print("\n");

  if (payload_written != strlen(json_payload) || newline_written != 1) {
    Serial.println("[SendJsonToPi] Write failed");
    client.stop();
    return false;
  }

  unsigned long t0 = millis();
  while (client.connected() && (millis() - t0) < 1000) {
    while (client.available()) {
      (void)client.read();
      t0 = millis();
    }
    delay(1);
  }

  client.stop();
  return true;
}

/**
 * @brief Sends JSON with string data to Pi-1 receiver
 * @param[in] device Device identifier string (e.g., "Wmos")
 * @param[in] sensor Sensor/component name (e.g., "ButtonD2")
 * @param[in] data String data value to transmit
 * @details Constructs JSON object with string data field and sends via TCP
 * @return true when JSON payload was sent successfully, false otherwise
 */
bool SendJsonToPi_str(const char *device, const char *sensor,
                      const char *data) {
  char json[JSON_BUF_SIZE];
  snprintf(json, sizeof(json),
           "{\"Device\":\"%s\",\"Sensor\":\"%s\",\"Data\":\"%s\"}",
           device, sensor, data);
  return post_payload(json);
}

/**
 * @brief Sends JSON with integer data to Pi-1 receiver
 * @param[in] device Device identifier string (e.g., "Wmos")
 * @param[in] sensor Sensor/component name (e.g., "ButtonD2")
 * @param[in] data Integer data value to transmit
 * @details Constructs JSON object with numeric data field and sends via TCP
 * @return true when JSON payload was sent successfully, false otherwise
 */
bool SendJsonToPi_int(const char *device, const char *sensor, int data) {
  char json[JSON_BUF_SIZE];
  snprintf(json, sizeof(json),
           "{\"Device\":\"%s\",\"Sensor\":\"%s\",\"Data\":%d}",
           device, sensor, data);
  return post_payload(json);
}

/**
 * @brief Initialize system: Serial, WiFi, button pin
 * @details Sets up serial debugging, WiFi connection using credentials from
 *          secrets.h, configures button pin as INPUT_PULLUP, and prints
 *          connection status and current send target to Serial.
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
    Serial.print("Send target: ");
    Serial.print(piHost);
    Serial.print(":");
    Serial.println(piPort);
    Serial.println("Startup complete. Waiting for button events...");
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
 *          to Pi-1 TCP receiver. Uses function-local static variables to
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
  /** @brief Last button count confirmed as sent */
  static int sentPressCount = 0;
  /** @brief Timestamp of last send attempt (rate limiter) */
  static unsigned long lastSendAttemptMs = 0;

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
      }
    }
  }

  lastButtonState = reading;

  // Send unsent button events in order with throttled retries.
  if (WiFi.status() == WL_CONNECTED && sentPressCount < pressCount
      && (millis() - lastSendAttemptMs) >= 1000) {
    int nextCount = sentPressCount + 1;
    lastSendAttemptMs = millis();
    Serial.print("Sending JSON count=");
    Serial.println(nextCount);

    if (SendJsonToPi_int(Device_Name, "ButtonD2", nextCount)) {
      sentPressCount = nextCount;
    } else {
      Serial.println("[SendJsonToPi] Send failed, will retry later");
    }
  }

  // Monitor WiFi connection and alert on loss
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost!");
    delay(5000);
  }

  // Main loop delay for non-blocking operation
  delay(10);
}
