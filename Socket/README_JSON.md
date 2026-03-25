# Socket – JSON Communication between WMos, Pi 1 and Pi 2

## Overview

The socket layer implements a three-node, full-duplex pipeline:

```
WMos D1 Mini  ──HTTP POST──▶  Pi-1  ◀══full-duplex TCP══▶  Pi-2
```

1. **WMos D1 Mini** (`WMos-Wifi/WMos-Wifi.ino`) — when the button on pin D2 is
   pressed it sends an HTTP POST to Pi 1 on port 9000 with a JSON body.
2. **Pi 1** (`Pi-1.c`) — listens on port 9000, parses the incoming JSON, and
   forwards it to Pi 2 over a persistent full-duplex TCP socket.  A background
   thread simultaneously receives messages (acknowledgements, commands) sent
   back by Pi 2.
3. **Pi 2** (`Pi-2.c`) — accepts one persistent connection from Pi 1.  A reader
   thread dispatches incoming JSON to the appropriate device handler and sends
   an acknowledgement back to Pi 1.  The main thread can forward stdin commands
   to Pi 1, demonstrating the Pi-2 → Pi-1 direction independently.

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

### Pi 1 ↔ Pi 2 (persistent TCP, newline-delimited JSON)

Pi 1 forwards the same JSON (serialised without whitespace, followed by `\n`)
over the persistent socket.  Pi 2 sends acknowledgements back over the same
connection using the same framing:

```json
{"status":"ack","Device":"Pi2"}
```

---

## Compilation

```bash
# x86-64
gcc -O2 -o Pi-1 Pi-1.c cJSON.c -lm -lpthread
gcc -O2 -o Pi-2 Pi-2.c cJSON.c -lm -lpthread

# ARM64 (Raspberry Pi)
aarch64-linux-gnu-gcc -O2 -o Pi-1 Pi-1.c cJSON.c -lm -lpthread
aarch64-linux-gnu-gcc -O2 -o Pi-2 Pi-2.c cJSON.c -lm -lpthread
```

---

## Usage

### 1. Start Pi 2 (terminal 1)

```bash
./Pi-2 9001
```

### 2. Start Pi 1 (terminal 2)

```bash
./Pi-1 9000 <pi2-hostname> 9001
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
--- Received from Pi-1 (57 bytes) ---
  Device: Wmos
  [WMos] Button : Buttons, servos, temp
  [WMos] Data   : 1
-------------------------------------
```

Pi 2 then sends an acknowledgement back to Pi 1 over the same socket, and
Pi 1 prints:

```
[Pi-1] Received from Pi-2: {"status":"ack","Device":"Pi2"}
```

---

## Adding a New Device

1. Implement a handler function in `Pi-2.c` with the `DeviceHandlerFn`
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
bash Socket/tests/test_socket.sh
```

The script compiles the binaries, starts Pi-1 and Pi-2 as background
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
| 6 | Full-duplex: Pi-2 ACK received by Pi-1          |

---

## Dependencies

- Standard C library (libc)
- Math library (`-lm` flag)
- POSIX threads (`-lpthread` flag)
- Unix socket APIs (`sys/socket.h`, `netinet/in.h`)
- Bundled [cJSON](https://github.com/DaveGamble/cJSON) (`cJSON.c` / `cJSON.h`)

