# Wemos D1 Mini - Button and HTTP POST to Pi-1

## Overview

This sketch connects the WeMos D1 Mini to WiFi, sends a JSON message to Pi-1
whenever the button is pressed, and listens on a TCP port for inbound commands
from Pi-1.

All JSON/HTTP sender logic is implemented directly inside `Wemos-Wifi.ino`
(no separate SendJsonToPi library files are required).

## Hardware

- Wemos D1 Mini (ESP8266)
- Push button between D2 and GND
- USB cable for upload and Serial Monitor

## Pin assignment

- D2: button input (`INPUT_PULLUP`)

## Configuration

WiFi configuration is split between a local secrets file and in-code constants.

1. Copy the template:

	```bash
	cp secrets.h.example secrets.h
	```

2. Edit `secrets.h` and set:

	- `WIFI_SSID`
	- `WIFI_PASSWORD`
	- `WIFI_MAX_ATTEMPTS`

3. Keep `secrets.h` private (it is ignored by git via repo `.gitignore`).

Network and hardware constants are defined in `Wemos-Wifi.ino`:

- `piHost` (default `172.16.0.80`)
- `piPort` (default `9000`)
- `wemosReceivePort` (default `9001`)
- `BUTTON_PIN` (default `2`, Wemos D2, declared locally in `setup()` and `loop()`)
- `debounceDelay` (default `50 ms`, declared in `loop()`)

If `secrets.h` is missing or required macros are not defined, compilation stops with a clear error message.

## JSON payload

On each button press, the sketch sends:

```json
{"Device":"Wemos","Sensor":"ButtonD2","Data":<press_count>}
```

Inbound messages are read as one line of text per TCP connection. The sketch
prints each message to Serial and replies with `ACK`.

## Serial output

Serial monitor is set to 9600 baud and prints:

- WiFi connection status
- Current POST target (`piHost:piPort`)
- Button press counter
- POST send status (payload too large / connection failed)

Also prints warnings when WiFi connection is lost.

## Button behavior

- Button is wired to D2/GPIO4 using `INPUT_PULLUP`
- Press event is detected on `LOW`
- Debounce delay is `50 ms`
- Each valid press increments `pressCount` and sends JSON

## Build notes

- Board: `WEMOS D1 mini`
- ESP8266 board package required in Arduino IDE
- If upload fails with timeout, verify COM port, USB cable, and boot mode
- If build fails with `Missing secrets.h`, copy `secrets.h.example` to `secrets.h`
