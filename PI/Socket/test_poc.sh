#!/bin/bash

# PoC Test Script: RPi_A and RPi_B Communication
# Demonstrates: HTTP -> Socket -> Dispatch -> ACK flow

set -e

# Change to Socket_CPP directory (script location)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

BUILD_DIR="./build"
RPI_A_PORT=5000
RPI_B_PORT=8080
RPI_A_HOST="127.0.0.1"

echo "=========================================="
echo "Socket PoC Test: RPi_A ↔ RPi_B"
echo "=========================================="
echo "Working directory: $(pwd)"

# Cleanup on exit
cleanup() {
    echo ""
    echo "[cleanup] Killing background processes..."
    pkill -f RPi_A || true
    pkill -f RPi_B || true
    sleep 1
}
trap cleanup EXIT

# Start RPi_A (TCP Receiver/Dispatcher)
echo ""
echo "[test] Starting RPi_A (TCP server on port $RPI_A_PORT)..."
$BUILD_DIR/RPi_A $RPI_A_PORT &
sleep 2

# Start RPi_B (HTTP Forwarder)
echo ""
echo "[test] Starting RPi_B (HTTP server on port $RPI_B_PORT, connecting to RPi_A)..."
$BUILD_DIR/RPi_B $RPI_A_HOST $RPI_A_PORT &
sleep 2

# Test 1: Send WMos device message
echo ""
echo "=== TEST 1: WMos Device Message ==="
echo "[test] Sending JSON message via HTTP POST to RPi_B..."
curl -X POST http://localhost:$RPI_B_PORT/data \
  -H "Content-Type: application/json" \
  -d '{"device":"wmos_device_1","sensor":"temperature","data":"22.5"}' \
  -v 2>&1 | grep -E "(< HTTP|\"status\"|{.*})" || true

sleep 1

# Test 2: Send temperature sensor message
echo ""
echo "=== TEST 2: Temperature Sensor Message ==="
echo "[test] Sending temperature sensor message..."
curl -X POST http://localhost:$RPI_B_PORT/data \
  -H "Content-Type: application/json" \
  -d '{"device":"temperature_sensor","sensor":"temp","data":"19.8"}' \
  -v 2>&1 | grep -E "(< HTTP|\"status\"|{.*})" || true

sleep 1

# Test 3: Send motion detector message
echo ""
echo "=== TEST 3: Motion Detector Message ==="
echo "[test] Sending motion detector message..."
curl -X POST http://localhost:$RPI_B_PORT/data \
  -H "Content-Type: application/json" \
  -d '{"device":"motion_detector","sensor":"pir","data":"motion_detected"}' \
  -v 2>&1 | grep -E "(< HTTP|\"status\"|{.*})" || true

sleep 1

# Test 4: Send key-value format message
echo ""
echo "=== TEST 4: Key-Value Format Message ==="
echo "[test] Sending key-value format message..."
curl -X POST http://localhost:$RPI_B_PORT/data \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d 'device=wmos_device_2&sensor=humidity&data=55.0' \
  -v 2>&1 | grep -E "(< HTTP|\"status\"|{.*})" || true

sleep 2

echo ""
echo "=========================================="
echo "PoC Test Complete"
echo "=========================================="
echo ""
echo "Process output:"
echo "- Check above for HTTP 200 OK responses"
echo "- RPi_A should show dispatched messages to handlers"
echo "- RPi_B should show forwarding and ACK reception"
echo ""
