# Socket with JSON Parsing - Build Instructions

## Overview
The socket code has been enhanced with JSON parsing capabilities using the **cJSON** library.

### Modified Files:
- **Pi-1s.c** - TCP Server with JSON parsing support
- **Pi-2c.c** - TCP Client with JSON message creation

## Features Added

### Server (Pi-1s.c):
- Receives JSON messages from clients
- Parses JSON data
- Extracts fields like "name" and "value"
- Sends back a JSON response with status information

### Client (Pi-2c.c):
- Creates JSON messages with sensor data
- Sends JSON to the server
- Parses JSON responses

## Compilation

### Option 1: Using system cJSON library (Linux/Mac)
```bash
gcc -o Pi-1s Pi-1s.c cJSON.c -lm
gcc -o Pi-2c Pi-2c.c cJSON.c -lm
```

### Option 2: Download cJSON source
1. Get cJSON from: https://github.com/DaveGamble/cJSON
2. Copy `cJSON.c` and `cJSON.h` to the Socket directory
3. Compile as shown in Option 1

### For Windows with MinGW:
```cmd
gcc -o Pi-1s.exe Pi-1s.c cJSON.c
gcc -o Pi-2c.exe Pi-2c.c cJSON.c
```

## Usage Example

### 1. Start the server (in terminal 1):
```bash
./Pi-1s 9000
```

### 2. Run the client (in terminal 2):
```bash
./Pi-2c localhost 9000
```

### Example JSON Messages:

**Client sends to server:**
```json
{
  "name": "temperature_sensor",
  "value": 23.5,
  "unit": "celsius"
}
```

**Server responds with:**
```json
{
  "status": "success",
  "message": "I got your message",
  "received_bytes": 72
}
```

## Code Features

### Parsing JSON (Server):
```c
cJSON *json = cJSON_Parse(buffer);
if (json) {
    cJSON *name = cJSON_GetObjectItemCaseSensitive(json, "name");
    if (name && name->valuestring) {
        printf("Name: %s\n", name->valuestring);
    }
    cJSON_Delete(json);
}
```

### Creating JSON (Client):
```c
cJSON *json = cJSON_CreateObject();
cJSON_AddStringToObject(json, "name", "temperature_sensor");
cJSON_AddNumberToObject(json, "value", 23.5);
char *jsonString = cJSON_Print(json);
write(sockfd, jsonString, strlen(jsonString));
cJSON_Delete(json);
```

## Notes for STM32 Integration
- cJSON is lightweight and suitable for embedded systems
- For STM32 microcontrollers, ensure you have enough memory for JSON parsing
- The simplified version in `cJSON.c` (if provided) uses standard malloc/free
- Consider using `cJSON_InitHooks()` to provide custom memory allocation for embedded systems

## Dependencies
- Standard C library (libc)
- Math library (-lm flag during compilation)
- Unix socket APIs (sys/socket.h, netinet/in.h)
