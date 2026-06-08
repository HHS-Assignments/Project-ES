# PI Automatisering – CAN ↔ Socket ↔ Wemos

Documentatie voor de routing-laag tussen de **CAN-bus**, **Raspberry Pi A**,
**Raspberry Pi B** en de **Wemos modules** in het ES Smart Office project.

---

## 1. Architectuur overzicht

```
   ┌─────────────────────────────────────────────────────────────────┐
   │                         CAN-BUS  (can0)                         │
   │   Balie MC      Luchtsluis MC      Lichtvoorziening MC          │
   └─────────────────────────────────────────────────────────────────┘
                              ▲   │
                              │   │  (CAN frames)
                              │   ▼
                ┌──────────────────────────────┐
                │       Raspberry Pi A         │
                │  ──────────────────────────  │
                │  CanSocketBridge             │  ← CAN ↔ JSON vertaling
                │  RPi_A (TCP server :5000)    │  ← dispatcher + bridge host
                └──────────────────────────────┘
                              ▲   │
                              │   │  TCP socket, JSON regels (\n)
                              │   ▼
                ┌──────────────────────────────┐
                │       Raspberry Pi B         │   Wi-Fi AP (192.168.4.1)
                │  ──────────────────────────  │
                │  HTTP server   :8080         │  ← Wemos → Pi B → Pi A
                │  Upstream RX thread          │  ← Pi A → Pi B → Wemos
                │  DeviceRouter (name → IP)    │
                └──────────────────────────────┘
                              ▲   │
                              │   │  HTTP POST /can  (JSON body)
                              │   ▼
                ┌──────────────────────────────┐
                │   Wemos modules (192.168.4.x)│
                │   • wemos_relaxstoel  .10    │
                │   • wemos_lichtkrant  .11    │
                └──────────────────────────────┘
```

**Kern idee:** Wemos modules zitten *niet* op de CAN-bus. Pi A vertaalt
relevante CAN-frames naar JSON, Pi B routeert die JSON naar het juiste
Wemos-IP via HTTP. Andere richting werkt analoog.

---

## 2. JSON contract

Eén formaat voor alle berichten tussen Pi A ↔ Pi B ↔ Wemos:

```json
{ "id": 320, "data": "01ff00", "target": "wemos_lichtkrant" }
```

| Veld     | Type    | Betekenis                                          |
|----------|---------|----------------------------------------------------|
| `id`     | number  | CAN message ID (zie busprotocol-documentatie)      |
| `data`   | string  | Hex-encoded payload (max 16 chars = 8 bytes)       |
| `target` | string  | Wemos device name **of** `"broadcast"`             |
| `ack`    | bool    | Optioneel; alleen Pi A stuurt dan een ACK terug    |

Op Pi A wordt per regel afgehandeld (`\n` als framing).

---

## 3. CAN ID → Wemos routing (vanuit busprotocol Kai Diemel)

Alleen IDs met bestemming **Pi** worden door Pi A naar Wemos doorgezet:

| CAN ID  | Bron       | Doel-Wemos          | Functie                        |
|---------|------------|---------------------|--------------------------------|
| `0x001` | Balie      | **broadcast**       | Noodsituatie                   |
| `0x140` | Balie      | wemos_relaxstoel    | Switch relaxstoel              |
| `0x150` | Balie      | wemos_lichtkrant    | Noodtekst deel 1               |
| `0x160` | Balie      | wemos_lichtkrant    | Noodtekst deel 2               |
| `0x170` | Balie      | wemos_lichtkrant    | Noodtekst deel 3               |
| `0x180` | Balie      | wemos_lichtkrant    | Tekst deel 1                   |
| `0x190` | Balie      | wemos_lichtkrant    | Tekst deel 2                   |
| `0x101` | Balie      | wemos_lichtkrant    | Tekst deel 3                   |
| `0x410` | Pi         | **broadcast**       | Dag-modus                      |
| `0x420` | Pi         | **broadcast**       | Nacht-modus                    |

IDs van Balie ↔ Luchtsluis ↔ Lichtvoorziening (`0x100`/`0x110`/`0x120`/
`0x130`/`0x200`/`0x210`/`0x300`) blijven puur op de CAN-bus.

Andere richting: een Wemos mag alleen IDs **van de Pi zelf** (`0x4xx`) op
de bus laten plaatsen — `isPiOwnedId()` borgt dit.

---

## 4. Componenten per device

### 4.1 Raspberry Pi A

**Bestanden:**
- `PI/Socket/RPi_A.cpp` – TCP server + dispatcher + bridge host
- `PI/Automatiseringen/CanSocketBridge.h/.cpp` – CAN ↔ socket vertaling

**Wat het doet:**

| Thread                       | Functie |
|------------------------------|---------|
| `tcp_server_thread`          | Accepteert verbindingen van Pi B op `:5000` |
| `handle_rpia_client`         | Leest JSON regels, parsed eerst als CAN-frame, anders klassieke `JsonMessage` |
| `CanSocketBridge::canRxLoop` | Leest `can0`, filtert, encodeert als JSON, push naar Pi B |
| `CanSocketBridge::socketRxLoop` | Leest JSON van Pi B, decodeert, schrijft CAN-frame |

**Parser-pad in `handle_rpia_client`:**
1. Probeer `tryParseCanFrame()` → `{id,data,target}` herkennen
2. Lukt niet → fallback naar `JsonMessage::parse()` + `DeviceDispatcher`
3. ACK alleen sturen wanneer `"ack":true` in JSON (voorkomt socket-race)

### 4.2 Raspberry Pi B

**Bestand:** `PI/Socket/RPi_B.cpp`

**Wat het doet:**

| Thread                  | Richting           | Functie |
|-------------------------|--------------------|---------|
| `http_server_thread`    | Wemos → Pi A       | HTTP POST :8080 ontvangen, body fire-and-forget over socket naar Pi A |
| `upstream_rx_thread`    | Pi A → Wemos       | JSON regels van Pi A, `target` veld lookup, HTTP POST naar Wemos |
| `DeviceRouter`          | mapping            | Naam → IP, thread-safe, broadcast support |

**Hardcoded mapping** (in `DeviceRouter` constructor):
```cpp
    registerDevice("wemos_relaxstoel", "10.42.0.10");
    registerDevice("wemos_lichtkrant", "10.42.0.20");
```

### 4.3 Wemos module

- HTTP server op poort 80, endpoint `/can`
- Ontvangt JSON, decodeert via CAN-ID-switch (relaxstoel / lichtkrant)
- Kan zelf POSTen naar `http://192.168.4.1:8080/can` om data op de bus
  te laten plaatsen (alleen `0x4xx` IDs worden door Pi A geaccepteerd)

---

## 5. End-to-end voorbeelden

### Voorbeeld A: Balie zet relaxstoel uit

```
Balie MC                Pi A                 Pi B               wemos_relaxstoel
   │                     │                    │                       │
   │── CAN 0x140 ───────▶│                    │                       │
   │    data=[0x00]      │                    │                       │
   │                     │ shouldForward? yes │                       │
   │                     │ target=relaxstoel  │                       │
   │                     │── JSON line ──────▶│                       │
   │                     │  {id:320,          │ resolve "relaxstoel"  │
   │                     │   data:"00",       │  → 192.168.4.10       │
   │                     │   target:"wemos_   │                       │
   │                     │    relaxstoel"}    │── POST /can ─────────▶│
   │                     │                    │                       │ → uit
```

### Voorbeeld B: Wemos meldt LDR-waarde aan Balie

```
wemos_lichtkrant        Pi B                 Pi A             Balie MC
   │                     │                    │                  │
   │── POST /can ───────▶│                    │                  │
   │ {id:1024,           │── JSON line ──────▶│                  │
   │  data:"7A",         │                    │ isPiOwnedId? yes │
   │  target:"pi"}       │                    │ (0x400)          │
   │                     │                    │── CAN 0x400 ────▶│
```

### Voorbeeld C: Nood (broadcast)

```
Balie ── CAN 0x001 ──▶ Pi A ── JSON target:"broadcast" ──▶ Pi B
                                                            │
                                          ┌─────────────────┼─────────────────┐
                                          ▼                 ▼                 ▼
                                  wemos_relaxstoel   wemos_lichtkrant    (alle)
```

---

## 6. Bouwen & draaien

### 6.1 Dependencies

- C++17 compiler (g++ ≥ 9)
- Linux met SocketCAN (`can-utils` voor debug)
- cJSON (zit in `PI/Socket/protocol/`)

### 6.2 CAN interface activeren (Pi A)

```bash
sudo ip link set can0 type can bitrate 500000
sudo ip link set up can0
candump can0          # debug: zie alle frames
```

### 6.3 Build

```bash
cd PI/Socket
mkdir -p build && cd build
cmake ..
make
```

`CMakeLists.txt` moet `CanSocketBridge.cpp` meenemen:
```cmake
add_executable(RPi_A
    RPi_A.cpp
    ../Automatiseringen/CanSocketBridge.cpp
    protocol/cJSON.c
    protocol/JsonMessage.cpp
    dispatch/DeviceDispatcher.cpp
    dispatch/WmosHandler.cpp
    dispatch/UnknownHandler.cpp
    transport/SocketConnection.cpp
)
target_link_libraries(RPi_A pthread)
```

### 6.4 Starten

**Op Pi A:**
```bash
./RPi_A 5000
```

**Op Pi B** (met Pi A op 192.168.4.2):
```bash
./RPi_B 192.168.4.2 5000
```

---

## 7. Configuratie (alle constanten op één plek)

| Bestand                  | Symbool                  | Default            |
|--------------------------|--------------------------|--------------------|
| `CanSocketBridge.h`      | `CAN_INTERFACE`          | `can0`             |
| `CanSocketBridge.h`      | `PI_B_HOST`              | `192.168.4.1`      |
| `CanSocketBridge.h`      | `PI_B_PORT`              | `5000`             |
| `RPi_A.cpp` (cfg ns)     | `DEFAULT_TCP_PORT`       | `5000`             |
| `RPi_B.cpp` (cfg ns)     | `HTTP_LISTEN_PORT`       | `8080`             |
| `RPi_B.cpp` (cfg ns)     | `WEMOS_HTTP_PORT`        | `80`               |
| `RPi_B.cpp` (cfg ns)     | `WEMOS_HTTP_PATH`        | `/can`             |
| `RPi_B.cpp` (cfg ns)     | `WEMOS_TIMEOUT_MS`       | `800`              |

---

## 8. Een nieuwe Wemos toevoegen

1. **CAN ID** definiëren in `CanSocketBridge.h` (`enum CanId`).
2. **Filter** uitbreiden in `CanSocketBridge::shouldForward()` → case toevoegen die `true` returnt.
3. **Routing** uitbreiden in `CanSocketBridge::routeCanIdToTarget()` → return de target-naam.
4. **Pi B mapping** uitbreiden in `DeviceRouter` constructor: `registerDevice("wemos_xxx", "192.168.4.NN");`
5. **(Optioneel)** Pi-owned IDs uit Wemos toelaten via `isPiOwnedId()` + case in `handleIncomingJson()`.

---

## 9. Debug tips

| Wat                  | Commando / actie |
|----------------------|------------------|
| CAN traffic mee lezen | `candump can0`  |
| Test-frame zenden     | `cansend can0 140#00` |
| Pi B → Wemos POST testen | `curl -X POST -d '{"id":320,"data":"01","target":"wemos_lichtkrant"}' http://192.168.4.1:8080/` (gaat naar Pi A!) |
| Pi A logging          | `[CAN-RX]` / `[bridge]` / `[RPi_A]` prefixes |
| Pi B logging          | `[router]` / `[route]` / `[http->wemos]` prefixes |

---

## 10. Bekende beperkingen

- **Geen retry** bij HTTP POST timeout naar offline Wemos (frame wordt gedropt).
- **Multi-frame tekst** (lichtkrant 3 delen, byte 7 = vervolg-bit) wordt
  transparant doorgegeven; de Wemos zelf moet de delen samenvoegen.
- **ACK** is fire-and-forget tenzij `"ack":true` in JSON; gebruik dit
  alleen voor debug, niet in productie (race-vrij).
- **DeviceRouter** mapping is hardcoded; bij nieuwe Wemos modules
  hercompileren. Toekomstige uitbreiding: dynamisch `/register` endpoint.

---

*Project ES – Smart Office – HHS*
