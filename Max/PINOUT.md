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
| PIR Pin | Nucleo Pin |
|---------|------------|
| VCC | 3.3V |
| GND | GND |
| OUT | PA1 |

## Noodknop
| Pin | Nucleo Pin | Opmerking |
|-----|------------|-----------|
| Knop | PA (NoodButton_Input_Pin) | Pull-up, actief LOW |
| GND | GND | |

## Gewone Knop (Deur openen)
| Pin | Nucleo Pin | Opmerking |
|-----|------------|-----------|
| Knop | PA (Button_Input_Pin) | Pull-up, actief LOW |
| GND | GND | |

## MAX7219 LED-matrix
| MAX7219 Pin | Nucleo Pin | Opmerking |
|-------------|------------|-----------|
| VCC | 5V | |
| GND | GND | |
| DIN | PB5 (SPI3 MOSI) | |
| CLK | PB3 (SPI3 CLK) | |
| CS/LOAD | MAX_CS_Pin | GPIO output |
