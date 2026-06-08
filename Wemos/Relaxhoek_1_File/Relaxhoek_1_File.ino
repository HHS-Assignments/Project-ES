#include <Arduino.h>

const uint16_t MIN_PULSE = 0;
const uint16_t MAX_PULSE = 1000;

class Motor {
public:
    Motor(uint8_t pin) : _pin(pin) {}

    void begin() {
        pinMode(_pin, OUTPUT);
    }

    void setSpeed(uint8_t pct) {
        if (pct > 100) pct = 100;
        uint16_t pulse = (pct == 0) ? 0 : MIN_PULSE + ((pct * (MAX_PULSE - MIN_PULSE)) / 100);
        analogWrite(_pin, pulse);
    }

private:
    uint8_t _pin;
};

class Schemerlamp {
public:
    Schemerlamp(uint8_t ldrPin, uint8_t ledPin, int drempel = 500)
        : _ldrPin(ldrPin), _ledPin(ledPin), _drempel(drempel),
          _lampAan(false), _ldrWaarde(0) {}

    void begin() {
        pinMode(_ledPin, OUTPUT);
        digitalWrite(_ledPin, LOW);
    }

    void update() {
        _ldrWaarde = analogRead(_ldrPin);
        if (_ldrWaarde < _drempel) {
            _lampAan = true;
            digitalWrite(_ledPin, HIGH);
        } else {
            _lampAan = false;
            digitalWrite(_ledPin, LOW);
        }
    }

    bool isAan(){ 
      return _lampAan;   
    }
    int  getLdrWaarde() { return _ldrWaarde; }

private:
    uint8_t _ldrPin;
    uint8_t _ledPin;
    int     _drempel;
    bool    _lampAan;
    int     _ldrWaarde;
};

Motor       motor(D2);
Schemerlamp schemerlamp(A0, D5, 100);

const uint8_t START_BTN = D7;
const uint8_t ESTOP_BTN = D6;

bool running = false;

void runStep(uint8_t speed, unsigned long ms) {
    motor.setSpeed(speed);
    unsigned long start = millis();
    while (millis() - start < ms) {
        if (digitalRead(ESTOP_BTN) == LOW) {
            motor.setSpeed(0);
            running = false;
            while (digitalRead(ESTOP_BTN) == LOW) delay(10);
            return;
        }
        schemerlamp.update();
        delay(50);
    }
}

void setup() {
    Serial.begin(9600);
    motor.begin();
    schemerlamp.begin();
    pinMode(START_BTN, INPUT_PULLUP);
    pinMode(ESTOP_BTN, INPUT_PULLUP);
}

void loop() {
    schemerlamp.update();

    if (digitalRead(ESTOP_BTN) == LOW) {
        running = false;
        motor.setSpeed(0);
        while (digitalRead(ESTOP_BTN) == LOW) delay(10);
        return;
    }

    if (digitalRead(START_BTN) == LOW && !running) {
        delay(50);
        if (digitalRead(START_BTN) == LOW) {
            running = true;
            while (digitalRead(START_BTN) == LOW) delay(10);
        }
    }

    if (!running) return;

    runStep(10,  2500);
    if (!running) return;
    runStep(50,  2500);
    if (!running) return;
    runStep(100, 2500);
    if (!running) return;
}