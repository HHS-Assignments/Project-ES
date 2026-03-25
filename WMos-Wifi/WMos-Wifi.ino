#include <ESP8266WiFi.h>

#include <stdio.h>
#include <string.h>

#define JSON_BUF_SIZE 256
#define HTTP_BUF_SIZE 512

// POST target used by the local JSON sender functions below.
const char *piHost = "10.0.42.1";
int piPort = 9000;

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

void SendJsonToPi_str(const char *device, const char *sensor,
                      const char *data) {
  char json[JSON_BUF_SIZE];
  snprintf(json, sizeof(json),
           "{\"Device\":\"%s\",\"Sensor\":\"%s\",\"Data\":\"%s\"}",
           device, sensor, data);
  post_payload(json);
}

void SendJsonToPi_int(const char *device, const char *sensor, int data) {
  char json[JSON_BUF_SIZE];
  snprintf(json, sizeof(json),
           "{\"Device\":\"%s\",\"Sensor\":\"%s\",\"Data\":%d}",
           device, sensor, data);
  post_payload(json);
}

// WiFi credentials
const char* ssid     = "Project-ES";
const char* password = "********";

// Pi-1 connection — piHost and piPort are defined in this sketch.
// Defaults are overridden in setup() if needed, e.g.:
//   piHost = "192.168.1.10";
//   piPort = 9001;

// Button configuration: WeMos D1 Mini D2 = GPIO4
const int BUTTON_PIN = 2;

// Debounce state
unsigned long lastDebounceTime = 0;
/* 50 ms debounce: typical mechanical buttons bounce for 10-20 ms;
 * 50 ms provides a comfortable margin for most push-button hardware. */
const unsigned long debounceDelay = 50;
int lastButtonState = HIGH;
int buttonState     = HIGH;

// Press counter used as Data payload
int pressCount = 0;

void setup() {
  // Initialize serial communication at 9600 baud
  Serial.begin(9600);
  delay(100);

  // Configure POST JSON destination (Pi-1 HTTP receiver)
  piHost = "172.16.0.80";
  piPort = 9000;

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

  // Wait for WiFi connection with timeout (20 s)
  int attempts     = 0;
  int max_attempts = 40;

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

void loop() {
  // Read button state with debounce
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;

      // Button pressed — LOW because INPUT_PULLUP
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

  // Warn on WiFi loss
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost!");
    delay(5000);
  }

  delay(10);
}
