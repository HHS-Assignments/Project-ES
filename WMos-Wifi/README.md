# WMos D1 Mini – Button & HTTP POST to Pi 1

## Overview

This sketch connects the WeMos D1 Mini to the `Project-ES` WiFi network and
sends a JSON HTTP POST to Pi 1 every time the button is pressed.  The HTTP
logic is encapsulated in the `SendJsonToPi` library; the sketch only calls a
single function.

## Hardware Requirements

- WeMos D1 Mini (ESP8266-based)
- Push button connected between pin **D2** and **GND**
- USB cable for programming and serial monitoring
- Pi 1 running `Pi-1` on `10.0.42.1:9000`

## Pin Assignment

| Pin | Role                      |
|-----|---------------------------|
| D2  | Button input (INPUT_PULLUP) |

Wire one leg of the button to D2 and the other to GND.  The internal pull-up
resistor keeps the pin HIGH; pressing the button pulls it LOW.

## Configuration

Connection settings live in the `SendJsonToPi` library and have sensible
defaults.  Override them in `setup()` if needed:

| Variable  | Default       | Description                 |
|-----------|---------------|-----------------------------|
| `piHost`  | `"10.0.42.1"` | Pi-1 IP address / hostname  |
| `piPort`  | `9000`        | Pi-1 TCP port               |

WiFi credentials are set at the top of `WMos-Wifi.ino`:

| Variable   | Default        |
|------------|----------------|
| `ssid`     | `"Project-ES"` |
| `password` | `"********"`   |

## JSON Payload

Each button press calls `SendJsonToPi("Wmos", "ButtonD2", pressCount)` which
sends:

```json
{"Device":"Wmos","Sensor":"ButtonD2","Data":<press_count>}
```

| Field    | Type          | Description                                       |
|----------|---------------|---------------------------------------------------|
| Device   | string        | Identifies the sending device (`"Wmos"`)          |
| Sensor   | string        | Sensor or output name (e.g. `"ButtonD2"`)         |
| Data     | number/string | Reading or state (integer press counter for WMos) |

## SendJsonToPi Library

The library is located in `WMos-Wifi/SendJsonToPi/`.

### Files

| File               | Language | Description                          |
|--------------------|----------|--------------------------------------|
| `SendJsonToPi.h`   | C/C++    | Public API header (include this)     |
| `SendJsonToPi.c`   | Pure C   | Implementation (lwIP BSD sockets)    |

### API

```c
/* C callers — use the SendJsonToPi() macro (C11 _Generic dispatch) */
SendJsonToPi(device, sensor, data);   /* data = const char* or int */

/* Underlying C functions (called automatically by the macro / C++ overloads) */
void SendJsonToPi_str(const char *device, const char *sensor, const char *data);
void SendJsonToPi_int(const char *device, const char *sensor, int data);
```

C++ callers (including `.ino` sketches) use inline overloads — the same
`SendJsonToPi(device, sensor, data)` call works transparently.

### Connection variables

```c
extern const char *piHost;  /* default "10.0.42.1" — reassign in setup() */
extern int         piPort;  /* default 9000         — reassign in setup() */
```

Override in `setup()` before the first call:

```cpp
piHost = "192.168.1.10";
piPort = 9001;
```

## How It Works

1. **Initialization** – serial at 9600 baud, D2 as INPUT_PULLUP.
2. **WiFi Connection** – connects to `Project-ES` (20 s timeout).
3. **Button Loop** – reads D2 with 50 ms debounce.
4. **HTTP POST** – on button press, calls `SendJsonToPi("Wmos","ButtonD2", pressCount)`.
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

- `ESP8266WiFi` (bundled with the ESP8266 board package — used by the sketch)
- `lwIP` BSD socket API (bundled with the ESP8266 board package — used by the C library)
- `SendJsonToPi` (local library in `WMos-Wifi/SendJsonToPi/`, written in pure C)

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
[SendJsonToPi] Connecting to 10.0.42.1:9000
[SendJsonToPi] Sending: {"Device":"Wmos","Sensor":"ButtonD2","Data":1}
[SendJsonToPi] Response: HTTP/1.1 200 OK ...{"status":"ok","forwarded":true}
```