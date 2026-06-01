# Bedrading Bewegingssensor Deur

## SG90 Servo
| Draad | Kleur | Nucleo Pin |
|-------|-------|------------|
| Signaal | Oranje | PA8 (TIM1 CH1) |
| Voeding | Rood | 5V |
| Massa | Bruin/Zwart | GND |

## Buzzer
| Buzzer Pin | Nucleo Pin | Opmerking |
|------------|------------|-----------|
| Signaal | PA0 (Buzzer_Pin, TIM2 CH1) | PWM output |
| GND | GND | |

## PIR Sensor
| PIR Pin | Nucleo Pin | Opmerking |
|---------|------------|-----------|
| VCC | 3.3V | |
| GND | GND | |
| OUT | PA1 (Motion_Input) | Geen pull, actief HIGH |

## Knop (Buzzer aan/uit)
| Pin | Nucleo Pin | Opmerking |
|-----|------------|-----------|
| Knop | PA4 (Button_Input) | Pull-up, actief LOW |
| GND | GND | |

## RGB LED
| LED Pin | Nucleo Pin | Opmerking |
|---------|------------|-----------|
| Rood | PA11 (RGB_Rood) | GPIO output |
| Groen | PA10 (RGB_Groen) | GPIO output |
| Blauw | PA9 (RGB_Blauw) | GPIO output |
| GND | GND | Gemeenschappelijke kathode |
