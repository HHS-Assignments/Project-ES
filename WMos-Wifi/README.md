# WMOS D1 WiFi Configuration

## Overview
This project configures the WMOS D1 mini board to connect to a WiFi network and transmit its IP address over serial communication.

## Hardware Requirements
- WMOS D1 mini board (ESP8266-based)
- USB cable for programming and serial monitoring
- WiFi network access

## Configuration

### WiFi Network Details
- **SSID (Network Name):** Project-ES

### Serial Communication
- **Baud Rate:** 9600
- **Used for:** Connection status messages and IP address output

## How It Works

1. **Initialization**: The board initializes serial communication at 9600 baud
2. **WiFi Connection**: Attempts to connect to the "Project-ES" network with the provided password
3. **Status Feedback**: Displays connection progress with dots in the serial monitor
4. **IP Address Display**: Once connected, sends the assigned IP address to serial
5. **Connection Monitoring**: Continuously monitors WiFi status and reports disconnections

## Setup Instructions

### Arduino IDE Configuration
1. Install ESP8266 board package:
   - File → Preferences → Additional Boards Manager URLs
   - Add: `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
   - Tools → Board Manager → Search "esp8266" → Install
   
2. Select the correct board:
   - Tools → Board → ESP8266 Boards → WMOS D1 mini

3. Upload the code to the board

### Monitoring Connection
1. Open Serial Monitor (Tools → Serial Monitor)
2. Set baud rate to **9600**
3. Watch for connection status and IP address output

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
```