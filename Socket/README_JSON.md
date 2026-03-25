# Socket – JSON Communication between WMos, Pi 1 and Pi 2

## Overview

The socket layer implements a three-node pipeline:

```
WMos D1 Mini  ──HTTP POST──▶  Pi 1 (Pi-1s)  ──TCP──▶  Pi 2 (Pi-2s)
```

1. **WMos D1 Mini** (`WMos-Wifi/WMos-Wifi.ino`) — when the button on pin D2 is
   pressed it sends an HTTP POST to Pi 1 on port 9000 with a JSON body.
2. **Pi 1** (`Pi-1s.c`) — listens on port 9000, parses the incoming JSON, and
   forwards it to Pi 2 over a TCP socket.
3. **Pi 2** (`Pi-2s.c`) — listens for forwarded JSON from Pi 1 and prints the
   Device, Button and Data fields to standard output.

`Pi-2c.c` is kept as a legacy client example (it sends a generic JSON message
to a server).

---

## JSON Message Format

### WMos → Pi 1 (HTTP POST body)

```json
{"Device": "Wmos", "Button": "Buttons, servos, temp", "Data": 1}
```

| Field    | Type          | Description                              |
|----------|---------------|------------------------------------------|
| Device   | string        | Identifies the sender (`"Wmos"`)         |
| Button   | string        | Category / label of the input            |
| Data     | number/string | Payload – button press count for WMos    |

### Pi 1 → Pi 2 (raw TCP, compact JSON)

Pi 1 forwards the same JSON (serialised without whitespace) over a direct TCP
connection.

---

## Compilation

```bash
# x86-64
gcc -O2 -o Pi-1s Pi-1s.c cJSON.c -lm
gcc -O2 -o Pi-2s Pi-2s.c cJSON.c -lm
gcc -O2 -o Pi-2c Pi-2c.c cJSON.c -lm   # legacy client

# ARM64 (Raspberry Pi)
aarch64-linux-gnu-gcc -O2 -o Pi-1s Pi-1s.c cJSON.c -lm
aarch64-linux-gnu-gcc -O2 -o Pi-2s Pi-2s.c cJSON.c -lm
```

---

## Usage

### 1. Start Pi 2 (terminal 1)

```bash
./Pi-2s 9001
```

### 2. Start Pi 1 (terminal 2)

```bash
./Pi-1s 9000 <pi2-hostname> 9001
```

Replace `<pi2-hostname>` with the hostname or IP of the Pi 2 machine (e.g.
`10.0.42.2` or `localhost` for local testing).

### 3. Trigger from WMos

Flash `WMos-Wifi/WMos-Wifi.ino`, connect to the `Project-ES` WiFi, then press
the button wired to pin **D2**.  The WMos sends:

```
POST http://10.0.42.1:9000/
Content-Type: application/json

{"Device":"Wmos","Button":"Buttons, servos, temp","Data":1}
```

Pi 1 parses the request and forwards to Pi 2, which prints:

```
--- Received message from Pi-1 (57 bytes) ---
  Device: Wmos
  [WMos] Button category : Buttons, servos, temp
  [WMos] Data            : 1
---------------------------------------------
```

---

## Adding a New Device

1. Implement a handler function in `Pi-2s.c` with the `DeviceHandlerFn`
   signature:

   ```c
   static void handle_my_device(const cJSON *json) {
       /* extract and print the fields you need */
   }
   ```

2. Add a row to the `device_handlers` table:

   ```c
   static const DeviceHandler device_handlers[] = {
       { "Wmos",     handle_wmos      },
       { "MyDevice", handle_my_device }, /* ← new entry */
   };
   ```

No other changes are required.  Unknown devices are handled automatically by
the `handle_unknown` fallback which dumps all JSON fields.

---

## Automated Tests

```bash
bash Socket/tests/test_socket.sh
```

The script compiles the binaries, starts Pi-1s and Pi-2s as background
processes, sends synthetic HTTP POST requests (mimicking the WMos) via
`curl`, and verifies that Pi-2s produces the expected output.

Tests covered:

| # | Scenario                              |
|---|---------------------------------------|
| 1 | WMos button press, numeric Data       |
| 2 | Second press – incremented Data value |
| 3 | Invalid JSON body → error response    |
| 4 | Unknown device → fallback handler     |
| 5 | String Data field                     |

---

## Dependencies

- Standard C library (libc)
- Math library (`-lm` flag)
- Unix socket APIs (`sys/socket.h`, `netinet/in.h`)
- Bundled [cJSON](https://github.com/DaveGamble/cJSON) (`cJSON.c` / `cJSON.h`)

