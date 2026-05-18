# Bedrading Bewegingssensor Deur

## SG90 Servo 1
| Draad | Kleur | Nucleo Pin |
|-------|-------|------------|
| Signaal | Oranje | PA8 (TIM1 CH1) |
| Voeding | Rood | 5V |
| Massa | Bruin/Zwart | GND |

## SG90 Servo 2
| Draad | Kleur | Nucleo Pin |
|-------|-------|------------|
| Signaal | Oranje | PA9 (TIM1 CH2) |
| Voeding | Rood | 5V |
| Massa | Bruin/Zwart | GND |

## PIR Sensor
| PIR Pin | Nucleo Pin | Opmerking |
|---------|------------|-----------|
| VCC | 3.3V | |
| GND | GND | |
| OUT | PA1 (Motion_Input) | Geen pull, actief HIGH |

## Noodknop
| Pin | Nucleo Pin | Opmerking |
|-----|------------|-----------|
| Knop | PA10 (NoodButton_Input) | Pull-up, actief LOW |
| GND | GND | |

## Gewone Knop (Deur openen)
| Pin | Nucleo Pin | Opmerking |
|-----|------------|-----------|
| Knop | PA11 (Button_Input) | Pull-up, actief LOW |
| GND | GND | |

## MAX7219 LED-matrix
| MAX7219 Pin | Nucleo Pin | Opmerking |
|-------------|------------|-----------|
| VCC | 5V | |
| GND | GND | |
| DIN | PB5 (SPI3 MOSI) | |
| CLK | PB3 (SPI3 SCK) | |
| CS/LOAD | PA3 (MAX_CS) | GPIO output |

## MFRC522 RFID-lezer
| MFRC522 Pin | Nucleo Pin | Opmerking |
|-------------|------------|-----------|
| VCC | 3.3V | **Niet 5V!** |
| GND | GND | |
| MOSI | PA7 (SPI1 MOSI) | |
| MISO | PA6 (SPI1 MISO) | |
| SCK | PA5 (SPI1 SCK) | |
| SDA/CS | PA4 (CS) | GPIO output |
| RST | PB0 (RESET) | GPIO output |
