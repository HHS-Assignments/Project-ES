#include <ESP8266WiFi.h>

// WiFi credentials
const char* ssid = "Project-ES";
const char* password = "********";

void setup() {
  // Initialize serial communication at 9600 baud
  Serial.begin(9600);
  delay(100);
  
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
  
  // Wait for WiFi connection with timeout
  int attempts = 10;
  int max_attempts = 40; // 20 seconds timeout (40 * 500ms)
  
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
    Serial.println("================================");
  } else {
    Serial.println("================================");
    Serial.println("Failed to connect to WiFi!");
    Serial.println("================================");
  }
}

void loop() {
  // Check WiFi connection status
  if (WiFi.status() == WL_CONNECTED) {
    delay(10000); // Check every 10 seconds
  } else {
    Serial.println("WiFi connection lost!");
    delay(5000);
  }
}
