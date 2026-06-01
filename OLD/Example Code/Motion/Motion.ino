#define PIR_PIN 13  // Verander dit naar jouw GPIO pin

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);

  Serial.println("HC-SR501 opstarten...");
  delay(30000); // Wacht 30 seconden - sensor kalibreert zichzelf op
  Serial.println("Klaar! Bewegingsdetectie actief.");
}

void loop() {
  int beweging = digitalRead(PIR_PIN);

  if (beweging == HIGH) {
    Serial.println("Beweging gedetecteerd!");
    delay(2000); // Kleine vertraging om spam te voorkomen
  }
}
