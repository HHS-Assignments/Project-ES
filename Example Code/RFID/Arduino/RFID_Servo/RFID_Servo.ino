#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

#define SS_PIN  D8
#define RST_PIN D3
#define SERVO_PIN D4

#define LED_GREEN D1
#define LED_RED   D2

MFRC522 rfid(SS_PIN, RST_PIN);
Servo myServo;

// ======= Zet hier jouw toegestane UID's =======
// voorbeeld UID: 04 A3 7F 22
byte allowedUID[][4] = {
  {0x91, 0xE0, 0x76, 0x1D} // Blauwe RFID tag
};

const int allowedCount = sizeof(allowedUID) / sizeof(allowedUID[0]);

// =============================================

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

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  myServo.attach(SERVO_PIN);
  myServo.write(0);

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);


  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, LOW);

  Serial.println("RFID + Servo +LEDs ready");
}

void loop() {

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  Serial.print("UID:");

  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(" ");
    Serial.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.println();

  if (isAuthorized(rfid.uid.uidByte, rfid.uid.size)) {
    Serial.println("Access granted");

    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, LOW);

    myServo.write(90);
    delay(1000);
    myServo.write(0);

  } else {
    Serial.println("Access denied");

    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, HIGH);

    delay(2000);
  }

  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, LOW);
  
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  delay(500);
}