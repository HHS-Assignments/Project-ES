# Socket - JSON Communication between WMos, Pi-B and Pi-A

## Overview

The socket layer implements a three-node, full-duplex pipeline:

```
WMos D1 Mini  --HTTP POST-->  Pi-B  <==full-duplex TCP==>  Pi-A
```

1. **WMos D1 Mini** (`WMos-Wifi/WMos-Wifi.ino`) — when the button on pin D2 is
   pressed it sends an HTTP POST to Pi 1 on port 9000 with a JSON body.
2. **Pi-B** (`Pi-B.c`) — listens on port 9000 with a **multi-threaded accept
   loop** (one worker thread per WMos connection), parses the incoming JSON,
   and forwards it to Pi-A over a persistent full-duplex TCP socket.  A
   background thread simultaneously receives messages (acknowledgements,
   commands) sent back by Pi-A.
3. **Pi-A** (`Pi-A.c`) — accepts one persistent connection from Pi-B.  A reader
   thread dispatches incoming JSON to the appropriate device handler and sends
   an acknowledgement back to Pi-B.  The main thread can forward stdin commands
   to Pi-B, demonstrating the Pi-A -> Pi-B direction independently.

---

## JSON Message Format

### WMos → Pi 1 (HTTP POST body)

```json
{"Device": "Wmos", "Sensor": "ButtonD2", "Data": 1}
```

| Field    | Type          | Description                                       |
|----------|---------------|---------------------------------------------------|
| Device   | string        | Identifies the sending device (`"Wmos"`)          |
| Sensor   | string        | Sensor or output name (e.g. `"ButtonD2"`)         |
| Data     | number/string | Reading or state (integer press counter for WMos) |

### Pi-B ↔ Pi-A (persistent TCP, newline-delimited JSON)

Pi-B forwards the same JSON (serialised without whitespace, followed by `\n`)
over the persistent socket.  Pi-A sends acknowledgements back over the same
connection using the same framing:

```json
{"status":"ack","Device":"A"}
```

---

## Compilation

```bash
# x86-64
gcc -O2 -o Pi-B Pi-B.c cJSON.c -lm -lpthread
gcc -O2 -o Pi-A Pi-A.c cJSON.c -lm -lpthread

# ARM64 (Raspberry Pi)
aarch64-linux-gnu-gcc -O2 -o Pi-B Pi-B.c cJSON.c -lm -lpthread
aarch64-linux-gnu-gcc -O2 -o Pi-A Pi-A.c cJSON.c -lm -lpthread
```

---

## Usage

### 1. Start Pi-A (terminal 1)

```bash
./Pi-A 9001
```

### 2. Start Pi-B (terminal 2)

```bash
./Pi-B 9000 <pi-a-hostname> 9001
```

Replace `<pi-a-hostname>` with the hostname or IP of the Pi-A machine (e.g.
`10.0.42.2` or `localhost` for local testing).

### 3. Trigger from WMos

Flash `WMos-Wifi/WMos-Wifi.ino`, connect to the `Project-ES` WiFi, then press
the button wired to pin **D2**.  The WMos calls
`SendJsonToPi_int("Wmos", "ButtonD2", pressCount)` which sends:

```
POST http://<PI_HOST>:<PI_PORT>/
Content-Type: application/json

{"Device":"Wmos","Sensor":"ButtonD2","Data":1}
```

`PI_HOST` is configured in `WMos-Wifi/secrets.h` (copied from
`WMos-Wifi/secrets.h.example`), and `PI_PORT` defaults to `9000` in the
current sketch configuration.

Pi-B parses the request and forwards to Pi-A, which prints:

```
--- Received from Pi-B (46 bytes) ---
  Device: Wmos
  [WMos] Sensor : ButtonD2
  [WMos] Data   : 1
-------------------------------------
```

Pi-A then sends an acknowledgement back to Pi-B over the same socket, and
Pi-B prints:

```
[B] Received from A: {"status":"ack","Device":"A"}
```

---

## Adding a New Device

1. Implement a handler function in `Pi-A.c` with the `DeviceHandlerFn`
   signature:

   ```c
   static void handle_my_device(cJSON *json) {
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
bash Tests/Socket/test_socket.sh
```

The script compiles the binaries, starts Pi-B and Pi-A as background
processes, sends synthetic HTTP POST requests (mimicking the WMos) via
`curl`, and verifies that both directions of the full-duplex channel work.

Tests covered:

| # | Scenario                                        |
|---|-------------------------------------------------|
| 1 | WMos button press, numeric Data                 |
| 2 | Second press – incremented Data value           |
| 3 | Invalid JSON body → error response              |
| 4 | Unknown device → fallback handler               |
| 5 | String Data field                               |
| 6 | Full-duplex: Pi-A ACK received by Pi-B          |

---

## Dependencies

- Standard C library (libc)
- Math library (`-lm` flag)
- POSIX threads (`-lpthread` flag)
- Unix socket APIs (`sys/socket.h`, `netinet/in.h`)
- Bundled [cJSON](https://github.com/DaveGamble/cJSON) (`cJSON.c` / `cJSON.h`)

