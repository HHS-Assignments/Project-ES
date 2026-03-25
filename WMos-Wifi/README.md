# WMos D1 Mini – Button & HTTP POST to Pi 1

## Overview

This sketch connects the WeMos D1 Mini to the `Project-ES` WiFi network and
sends a JSON HTTP POST to Pi 1 every time the button is pressed.

## Hardware Requirements

- WeMos D1 Mini (ESP8266-based)
- Push button connected between pin **D2** and **GND**
- USB cable for programming and serial monitoring
- Pi 1 running `Pi-1s` on `10.0.42.1:9000`

## Pin Assignment

| Pin | Role                      |
|-----|---------------------------|
| D2  | Button input (INPUT_PULLUP) |

Wire one leg of the button to D2 and the other to GND.  The internal pull-up
resistor keeps the pin HIGH; pressing the button pulls it LOW.

## Configuration

| Setting  | Value          | Location in sketch |
|----------|----------------|--------------------|
| SSID     | `Project-ES`   | `ssid`             |
| Password | `********`     | `password`         |
| Pi 1 IP  | `10.0.42.1`    | `serverIP`         |
| Pi 1 Port| `9000`         | `serverPort`       |
| Button   | `D2`           | `BUTTON_PIN`       |

## JSON Payload

Each button press sends:

```json
{"Device":"Wmos","Button":"Buttons, servos, temp","Data":<press_count>}
```

`Data` is an integer that increments with every press.

## How It Works

1. **Initialization** – serial at 9600 baud, D2 as INPUT_PULLUP.
2. **WiFi Connection** – connects to `Project-ES` (20 s timeout).
3. **Button Loop** – reads D2 with 50 ms debounce.
4. **HTTP POST** – on button press, sends the JSON payload to
   `http://10.0.42.1:9000/`.
5. **Response** – prints the HTTP response code and body to Serial.

## Setup Instructions

### Arduino IDE

1. Install ESP8266 board package:
   - File → Preferences → Additional Boards Manager URLs
   - Add: `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
   - Tools → Board Manager → Search "esp8266" → Install
2. Select **Tools → Board → ESP8266 Boards → WEMOS D1 mini**.
3. Upload the sketch.

### Required Libraries

- `ESP8266WiFi` (bundled with the ESP8266 board package)
- `ESP8266HTTPClient` (bundled with the ESP8266 board package)

### Serial Monitor

Open at **9600 baud** to see connection status and POST results.

## Serial Output Example

```
================================
WiFi Connection Starting...
================================
Connecting to WiFi network: Project-ES
....
================================
WiFi Connected Successfully!
================================
IP Address: 10.42.0.xxx
================================
Button pressed! Sending JSON...
Sending JSON: {"Device":"Wmos","Button":"Buttons, servos, temp","Data":1}
HTTP POST response code: 200
Response: {"status":"ok","forwarded":true}
```