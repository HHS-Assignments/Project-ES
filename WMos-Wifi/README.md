# WMos D1 Mini - Button and HTTP POST to Pi-1

## Overview

This sketch connects the WeMos D1 Mini to WiFi and sends a JSON HTTP POST to
Pi-1 whenever the button is pressed.

All JSON/HTTP sender logic is implemented directly inside `WMos-Wifi.ino`
(no separate SendJsonToPi library files are required).

## Hardware

- WeMos D1 Mini (ESP8266)
- Push button between D2 and GND
- USB cable for upload and Serial Monitor

## Pin assignment

- D2: button input (`INPUT_PULLUP`)

## Configuration

Edit these values in `WMos-Wifi.ino`:

- `ssid`
- `password`
- `piHost`
- `piPort`

## JSON payload

On each button press, the sketch sends:

```json
{"Device":"Wmos","Sensor":"ButtonD2","Data":<press_count>}
```

## Serial output

Serial monitor is set to 9600 baud and prints:

- WiFi connection status
- Current POST target (`piHost:piPort`)
- Button press counter
- POST send status

## Build notes

- Board: `WEMOS D1 mini`
- ESP8266 board package required in Arduino IDE
- If upload fails with timeout, verify COM port, USB cable, and boot mode
