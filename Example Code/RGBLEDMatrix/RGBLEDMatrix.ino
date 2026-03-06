const int BLUE = D4;
const int GREEN = D2;
const int RED = D1;



void setup() {
  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE, OUTPUT);
}

void loop() {
  digitalWrite(BLUE, LOW);
  digitalWrite(GREEN, LOW);
  digitalWrite(RED, LOW);
  delay(1000);

  for (int i = 3; i > 0; i--) {
    digitalWrite(BLUE, i == 3);
    digitalWrite(GREEN, i == 2);
    digitalWrite(RED, i == 1);
    delay(1000);
  }

  
}
