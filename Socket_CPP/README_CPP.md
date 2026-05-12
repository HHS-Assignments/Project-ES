# Socket C++ Architecture — Proof of Concept

## Overview

This is a **proof-of-concept** demonstrating real socket communication between Raspberry Pi A and Pi B with JSON message passing. The PoC showcases:

- **RPi_B**: HTTP server that receives POST requests with JSON, forwards to RPi-A via TCP
- **RPi_A**: TCP server that receives JSON messages, parses them, dispatches to handlers, and sends ACKs

The architecture follows a formal **UML class diagram** (see [design.uxf](design.uxf) for the visual representation) and implements strict UML semantics for composition, aggregation, interface realization, and multiplicity.

### Key Features

✅ **HTTP Ingress** (RPi-B): Simple HTTP POST receiver with JSON body extraction  
✅ **Persistent TCP Connection**: RPi-B ↔ RPi-A bidirectional socket  
✅ **JSON Parsing**: Support for both JSON and key-value formats  
✅ **Device Dispatching**: Route messages to type-specific handlers  
✅ **ACK Protocol**: Bidirectional acknowledgements  
✅ **Thread-Safe**: Multithreaded server handling multiple clients  
✅ **RAII**: C++17 smart pointers for resource management

## Quick Start

### Build

```bash
cd /home/admtom/Git/Project-ES/Socket_CPP
mkdir -p build && cd build
cmake ..
cmake --build .
```

### Run PoC Test

```bash
cd /home/admtom/Git/Project-ES/Socket_CPP
./test_poc.sh
```

This will:
1. Start RPi_A (TCP server on port 5000)
2. Start RPi_B (HTTP server on port 8080)
3. Send 4 test messages via HTTP POST
4. Show full message flow with JSON parsing and dispatch

**Expected Output:**
```
=== TEST 1: WMos Device Message ===
[RPi_B] HTTP client: {"device":"wmos_device_1","sensor":"temperature","data":"22.5"}
[RPi_A] Parsed message - Device: wmos_device_1
[WmosHandler::handle] Device: wmos_device_1 Sensor: temperature Data: 22.5
[RPi_B] ACK from RPi-A: {"status":"OK","device":"wmos_device_1"}
< HTTP/1.1 200 OK
```

## Manual Testing

### Terminal 1: Start RPi_A (TCP Server)

```bash
./build/RPi_A 5000
# Output:
# [RPi_A] TCP server listening on port 5000
# [RPi_A] Registered 4 handlers
```

### Terminal 2: Start RPi_B (HTTP Forwarder)

```bash
./build/RPi_B 127.0.0.1 5000
# Output:
# [RPi_B] Connected to RPi-A successfully
# [RPi_B] HTTP server listening on port 8080
```

### Terminal 3: Send JSON Message via curl

```bash
curl -X POST http://localhost:8080/data \
  -H "Content-Type: application/json" \
  -d '{"device":"wmos_device_1","sensor":"temperature","data":"22.5"}'

# Returns: {"status":"OK","device":"wmos_device_1"}
```

## Project Structure

```
Socket_CPP/
├── protocol/
│   ├── JsonMessage.h
│   └── JsonMessage.cpp
├── transport/
│   ├── SocketConnection.h
│   ├── SocketConnection.cpp
│   ├── HttpServer.h
│   ├── HttpServer.cpp
│   ├── ThreadPoolWorker.h
│   └── ThreadPoolWorker.cpp
├── dispatch/
│   ├── IDeviceHandler.h
│   ├── WmosHandler.h
│   ├── WmosHandler.cpp
│   ├── UnknownHandler.h
│   ├── UnknownHandler.cpp
│   ├── DeviceDispatcher.h
│   └── DeviceDispatcher.cpp
├── service/
│   ├── PiAService.h
│   ├── PiAService.cpp
│   ├── PiBService.h
│   └── PiBService.cpp
├── tests/
│   └── test_basic.cpp
├── CMakeLists.txt
├── RPi_A.cpp          ← PoC: TCP Receiver/Dispatcher
├── RPi_B.cpp          ← PoC: HTTP Forwarder
├── test_poc.sh        ← Integration test script
├── design.txt
├── design.uxf
└── README_CPP.md
```

## Class Relationships (UML Semantics)

### Key Associations

- **Composition** (`*--`): Strong ownership; owned object destroyed with owner
  - `PiBService *-- HttpServer`
  - `PiBService *-- DeviceDispatcher`
  - `PiAService *-- SocketConnection`

- **Aggregation** (`o--`): Weaker ownership; shared relationship
  - `PiBService o-- SocketConnection` (upstream)
  - `DeviceDispatcher o-- IDeviceHandler` (handlers)

- **Realization** (`...>`): Interface implementation
  - `WmosHandler` implements `IDeviceHandler`
  - `UnknownHandler` implements `IDeviceHandler`

- **Dependency** (`-->`): Transient usage
  - Services depend on `JsonMessage` for data exchange

## Message Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                           HTTP Client                           │
│                   (e.g., WMos WiFi Device)                     │
└────────────────────────────┬──────────────────────────────────┘
                             │
                    POST /data with JSON
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                     RPi_B (HTTP Server)                         │
│              Listening on port 8080                            │
│  ┌────────────────────────────────────────────────────────┐   │
│  │ 1. Accept HTTP POST request                           │   │
│  │ 2. Extract JSON body from request                     │   │
│  │ 3. Parse JSON (device, sensor, data fields)           │   │
│  │ 4. Forward via persistent TCP socket to RPi-A        │   │
│  │ 5. Read ACK from RPi-A                               │   │
│  │ 6. Send HTTP 200 OK response with ACK to client       │   │
│  └────────────────────────────────────────────────────────┘   │
└────────────────────────────┬──────────────────────────────────┘
                             │
               TCP Connection (persistent)
               JSON message + ACK response
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                     RPi_A (TCP Server)                          │
│              Listening on port 5000                            │
│  ┌────────────────────────────────────────────────────────┐   │
│  │ 1. Accept TCP connection from RPi-B                   │   │
│  │ 2. Read JSON message line-by-line                     │   │
│  │ 3. Parse JSON (extract device, sensor, data)          │   │
│  │ 4. Dispatch to device-specific handler                │   │
│  │    ├─ WmosHandler (for WMos devices)                  │   │
│  │    ├─ TemperatureHandler (for temp sensors)           │   │
│  │    └─ UnknownHandler (fallback)                       │   │
│  │ 5. Send ACK back to RPi-B: {"status":"OK"}            │   │
│  └────────────────────────────────────────────────────────┘   │
│                                                                 │
│  Device Handlers (process messages)                           │
│  ├─ WmosHandler: Process WiFi device data                    │
│  ├─ LoggingHandler: Log sensor readings                      │
│  └─ UnknownHandler: Handle unsupported devices               │
└─────────────────────────────────────────────────────────────────┘
```

## Supported Message Formats

### JSON Format
```json
{
  "device": "wmos_device_1",
  "sensor": "temperature",
  "data": "22.5"
}
```

### Key-Value Format
```
device=wmos_device_1&sensor=temperature&data=22.5
```

## Handler Architecture

The `DeviceDispatcher` routes messages based on device name:

```cpp
// RPi_A registration (see RPi_A.cpp)
dispatcher->registerHandler("wmos_device_1", wmos_handler.get());
dispatcher->registerHandler("temperature_sensor", logging_handler.get());
dispatcher->registerHandler("motion_detector", logging_handler.get());
```

When a message arrives for "wmos_device_1", it's routed to `WmosHandler::handle()`:

```cpp
[WmosHandler::handle] Device: wmos_device_1 Sensor: temperature Data: 22.5
```

## Performance Notes

- **Single-threaded per handler**: Each message is dispatched synchronously
- **Multi-client HTTP**: RPi_B spawns a thread per HTTP client
- **Multi-client TCP**: RPi_A spawns a thread per TCP client
- **Mutex protection**: Upstream socket is protected with mutex in RPi_B

## Implementation Status

### Fully Implemented (PoC)

✅ JsonMessage parsing (JSON and key-value formats)  
✅ SocketConnection (TCP client, send/read lines)  
✅ HttpServer basic structure (listen, accept, thread-per-client)  
✅ DeviceDispatcher (handler registry, routing)  
✅ WmosHandler, UnknownHandler (handler implementations)  
✅ PiAService, PiBService (service classes)  
✅ Test script with 4 integration tests  

### TODO (Production)

- 🔄 Robust HTTP parsing (Content-Length, chunked encoding)
- 🔄 Full cJSON integration for JSON serialization
- 🔄 Connection pooling and reconnection logic
- 🔄 Comprehensive error handling and logging
- 🔄 Rate limiting and backpressure
- 🔄 Configuration file support
- 🔄 Unit tests for each component
- 🔄 Performance benchmarking

## Building & Testing

### Prerequisites

- C++17 compiler (gcc, clang)
- CMake 3.10+
- POSIX OS (Linux, macOS)
- curl (for manual testing)

### Build Commands

```bash
# Configure
cd Socket_CPP && mkdir -p build && cd build
cmake ..

# Compile
cmake --build .

# Run PoC test
cd .. && ./test_poc.sh

# Run unit tests
cd build && ctest --output-on-failure
```

### Test Coverage

| Test | RPi_A | RPi_B | JSON | Dispatch | ACK |
|------|-------|-------|------|----------|-----|
| WMos Device | ✅ | ✅ | ✅ | ✅ | ✅ |
| Temperature Sensor | ✅ | ✅ | ✅ | ✅ | ✅ |
| Motion Detector | ✅ | ✅ | ✅ | ✅ | ✅ |
| Key-Value Format | ✅ | ✅ | ✅ | ✅ | ✅ |

## Design Principles

1. **Separation of Concerns**: HTTP, transport, dispatch, service layers
2. **Interface-Based Design**: `IDeviceHandler` polymorphism
3. **RAII**: Smart pointers, exception safety
4. **Thread Safety**: Mutex-protected shared resources
5. **Extensibility**: Easy to add new handlers or message formats

## UML Diagram

Open the visual UML diagram in UMLet:

```bash
code design.uxf
```

Formal UML specification (Wikipedia rules):

```bash
cat design.txt
```

## Integration with Real Hardware

To run on actual Raspberry Pi A and Pi B:

1. **On RPi-A (Receiver):**
   ```bash
   ./build/RPi_A 5000  # Listen on port 5000
   ```

2. **On RPi-B (Forwarder):**
   ```bash
   ./build/RPi_B <RPi-A-IP> 5000  # Connect to RPi-A
   ```

3. **From WMos Device:**
   ```bash
   curl -X POST http://<RPi-B-IP>:8080/data \
     -H "Content-Type: application/json" \
     -d '{"device":"my_sensor","sensor":"temp","data":"23.0"}'
   ```

## Troubleshooting

### "Address already in use" error
```bash
# Kill processes on ports 5000 and 8080
lsof -ti:5000,8080 | xargs kill -9
```

### Messages not forwarded
- Check RPi-A is running and listening
- Check firewall allows TCP port 5000
- Verify IP address in RPi_B connection string

### JSON parsing fails
- Ensure JSON is properly formatted
- Both JSON and key-value formats are supported
- Check console output for parse errors

## References

- **UML Class Diagrams**: https://en.wikipedia.org/wiki/Class_diagram
- **C++ Best Practices**: https://isocpp.github.io/CppCoreGuidelines
- **Socket Programming**: https://man7.org/linux/man-pages/man2/socket.2.html
- **HTTP/1.1**: https://tools.ietf.org/html/rfc7231

---

**Status**: ✅ **Proof of Concept Complete** — Ready for production implementation

**Last Updated**: May 2026  
**Author**: GitHub Copilot

