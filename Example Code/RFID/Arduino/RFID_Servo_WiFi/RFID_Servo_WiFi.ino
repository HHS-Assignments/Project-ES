#include <ESP8266WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

#include <stdio.h>
#include <string.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing secrets.h. Copy secrets.h.example to secrets.h and set WIFI_SSID/WIFI_PASSWORD/WIFI_MAX_ATTEMPTS."
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

#define SS_PIN    D8
#define RST_PIN   D3
#define SERVO_PIN D4
#define LED_GREEN D1
#define LED_RED   D2

#define JSON_BUF_SIZE 256

const char *const piHost       = "10.42.0.1";
const int         piPort       = 9000;
const char *const ssid         = WIFI_SSID;
const char *const password     = WIFI_PASSWORD;
const int         max_attempts = WIFI_MAX_ATTEMPTS;
const char *const Device_Name  = "access reader";

MFRC522 rfid(SS_PIN, RST_PIN);
Servo   myServo;

byte allowedUID[][4] = {
  {0x91, 0xE0, 0x76, 0x1D}
};
const int allowedCount = sizeof(allowedUID) / sizeof(allowedUID[0]);

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

bool SendJsonToPi_str(const char *device, const char *sensor,
                      const char *data) {
  char json[JSON_BUF_SIZE];
  snprintf(json, sizeof(json),
           "{\"Device\":\"%s\",\"Sensor\":\"%s\",\"Data\":\"%s\"}",
           device, sensor, data);
  return post_payload(json);
}

bool isAuthorized(byte *uid, byte uidSize) {
  for (int i = 0; i < allowedCount; i++) {
    bool match = true;
    for (byte j = 0; j < uidSize; j++) {
      if (uid[j] != allowedUID[i][j]) {
        match = false;
        break;
      }
    }
    if (match) return true;
  }
  return false;
}

void uidToString(byte *uid, byte uidSize, char *buf) {
  buf[0] = '\0';
  char hex[4];
  for (byte i = 0; i < uidSize; i++) {
    if (i > 0) strcat(buf, ":");
    snprintf(hex, sizeof(hex), "%02X", uid[i]);
    strcat(buf, hex);
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  SPI.begin();
  rfid.PCD_Init();

  myServo.attach(SERVO_PIN);
  myServo.write(0);

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED,   OUTPUT);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED,   LOW);

  Serial.println("\n\n================================");
  Serial.println("WiFi Connection Starting...");
  Serial.println("================================");

  WiFi.mode(WIFI_STA);
  Serial.print("Connecting to: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < max_attempts) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("================================");
    Serial.println("WiFi Connected!");
    Serial.print  ("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print  ("Reporting to: ");
    Serial.print  (piHost);
    Serial.print  (":");
    Serial.println(piPort);
  } else {
    Serial.println("WiFi connection FAILED — reporting disabled.");
  }

  Serial.println("================================");
  Serial.println("RFID + Servo + LEDs ready.");
  Serial.println("================================");
}

void loop() {
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial())   return;

  char uidStr[16];
  uidToString(rfid.uid.uidByte, rfid.uid.size, uidStr);

  Serial.print("UID: ");
  Serial.println(uidStr);

  char jsonData[JSON_BUF_SIZE];

  if (isAuthorized(rfid.uid.uidByte, rfid.uid.size)) {
    Serial.println("Access granted");
    snprintf(jsonData, sizeof(jsonData), "ACCESS GRANTED - UID: %s", uidStr);

    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED,   LOW);
    myServo.write(90);
    delay(1000);
    myServo.write(0);

  } else {
    Serial.println("Access denied");
    snprintf(jsonData, sizeof(jsonData), "ACCESS DENIED - UID: %s", uidStr);

    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED,   HIGH);
    delay(2000);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Sending: ");
    Serial.println(jsonData);

    if (!SendJsonToPi_str(Device_Name, "RFID", jsonData)) {
      Serial.println("[SendJsonToPi] Send failed");
    }
  } else {
    Serial.println("WiFi not connected — event not reported.");
  }

  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED,   LOW);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(500);
}