#!/usr/bin/env bash
# test_socket.sh — End-to-end tests for the Pi-1 / Pi-2 full-duplex pipeline.
#
# The script starts Pi-2 and Pi-1 as background processes, sends synthetic
# HTTP POST requests that mimic the WMos D1 Mini, and verifies that:
#   - Pi-2 prints the expected dispatched output  (Pi-1 → Pi-2 direction).
#   - Pi-1 receives acknowledgements from Pi-2    (Pi-2 → Pi-1 direction).
#
# Pre-built binaries:
#   Set PI1_BINARY and PI2_BINARY environment variables to use pre-compiled
#   binaries (e.g. downloaded from a CI artifact).  When these variables are
#   not set the script compiles its own test copies from source.
#
# Exit codes:
#   0  All tests passed.
#   1  One or more tests failed.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SOCKET_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)/Socket"

# Use pre-built binaries if supplied, otherwise compile locally.
if [ -n "${PI1_BINARY:-}" ] && [ -x "${PI1_BINARY}" ]; then
    PI1_BIN="${PI1_BINARY}"
    COMPILED_PI1=false
else
    PI1_BIN="$SOCKET_DIR/Pi-1-test"
    COMPILED_PI1=true
fi

if [ -n "${PI2_BINARY:-}" ] && [ -x "${PI2_BINARY}" ]; then
    PI2_BIN="${PI2_BINARY}"
    COMPILED_PI2=false
else
    PI2_BIN="$SOCKET_DIR/Pi-2-test"
    COMPILED_PI2=true
fi

PI1_PORT=19000
PI2_PORT=19001

PASS=0
FAIL=0

# ── Helpers ────────────────────────────────────────────────────────────────

log()  { echo "[TEST] $*"; }
pass() { echo "[PASS] $*"; PASS=$((PASS + 1)); }
fail() { echo "[FAIL] $*"; FAIL=$((FAIL + 1)); }

cleanup() {
    [ -n "${PI1_PID:-}" ] && kill "$PI1_PID" 2>/dev/null || true
    [ -n "${PI2_PID:-}" ] && kill "$PI2_PID" 2>/dev/null || true
    # Only delete binaries that we compiled ourselves.
    [ "$COMPILED_PI1" = true ] && rm -f "$PI1_BIN" || true
    [ "$COMPILED_PI2" = true ] && rm -f "$PI2_BIN" || true
    rm -f /tmp/pi2_out.txt /tmp/pi1_out.txt
}
trap cleanup EXIT

# ── Build (only when pre-built binaries were not provided) ─────────────────

if [ "$COMPILED_PI1" = true ]; then
    log "Compiling Pi-1..."
    gcc -O2 -o "$PI1_BIN" "$SOCKET_DIR/Pi-1.c" "$SOCKET_DIR/cJSON.c" \
        -lm -lpthread || { echo "[FAIL] Compilation of Pi-1 failed"; exit 1; }
else
    log "Using pre-built Pi-1: $PI1_BIN"
fi

if [ "$COMPILED_PI2" = true ]; then
    log "Compiling Pi-2..."
    gcc -O2 -o "$PI2_BIN" "$SOCKET_DIR/Pi-2.c" "$SOCKET_DIR/cJSON.c" \
        -lm -lpthread || { echo "[FAIL] Compilation of Pi-2 failed"; exit 1; }
else
    log "Using pre-built Pi-2: $PI2_BIN"
fi

# ── Start servers ──────────────────────────────────────────────────────────

log "Starting Pi-2 on port $PI2_PORT..."
"$PI2_BIN" "$PI2_PORT" > /tmp/pi2_out.txt 2>&1 &
PI2_PID=$!

# Wait until Pi-2's port is in LISTEN state (up to 10 s).
# Using ss avoids actually connecting to Pi-2's single-accept slot.
log "Waiting for Pi-2 to be ready..."
for i in $(seq 1 20); do
    ss -tlnp 2>/dev/null | grep -q ":${PI2_PORT}" && break
    sleep 0.5
done

log "Starting Pi-1 on port $PI1_PORT forwarding to localhost:$PI2_PORT..."
"$PI1_BIN" "$PI1_PORT" localhost "$PI2_PORT" > /tmp/pi1_out.txt 2>&1 &
PI1_PID=$!

# Wait until Pi-1's WMos port is in LISTEN state.
# Pi-1 only reaches listen() after successfully connecting to Pi-2,
# so this also confirms the Pi-1 ↔ Pi-2 connection is up.
log "Waiting for Pi-1 to be ready..."
for i in $(seq 1 20); do
    ss -tlnp 2>/dev/null | grep -q ":${PI1_PORT}" && break
    sleep 0.5
done

# ── Test helpers ───────────────────────────────────────────────────────────

# send_post <json_body>
# Sends an HTTP POST to Pi-1 and returns the response body.
send_post() {
    local body="$1"
    curl -s --max-time 5 \
         -X POST "http://localhost:${PI1_PORT}/" \
         -H "Content-Type: application/json" \
         -d "$body" 2>/dev/null || echo ""
}

# wait_for_output <file> <string> [<timeout_seconds>]
# Polls <file> until <string> appears or the timeout expires.
wait_for_output() {
    local file="$1"
    local needle="$2"
    local timeout="${3:-5}"
    local elapsed=0
    while [ "$elapsed" -lt "$timeout" ]; do
        grep -qF "$needle" "$file" 2>/dev/null && return 0
        sleep 0.5
        elapsed=$((elapsed + 1))
    done
    return 1
}

# ── Test 1: WMos button press (numeric Data) ──────────────────────────────
log "Test 1: WMos button press with numeric Data"

RESP=$(send_post '{"Device":"Wmos","Sensor":"ButtonD2","Data":1}')

if echo "$RESP" | grep -q '"forwarded":true'; then
    pass "Pi-1 acknowledged forward"
else
    fail "Pi-1 did not acknowledge forward. Response: $RESP"
fi

if wait_for_output /tmp/pi2_out.txt "Wmos" 5; then
    pass "Pi-2 printed Device=Wmos"
else
    fail "Pi-2 did not print Device=Wmos"
    cat /tmp/pi2_out.txt
fi

if wait_for_output /tmp/pi2_out.txt "ButtonD2" 5; then
    pass "Pi-2 printed Sensor name"
else
    fail "Pi-2 did not print Sensor name"
fi

if wait_for_output /tmp/pi2_out.txt "1" 5; then
    pass "Pi-2 printed Data value"
else
    fail "Pi-2 did not print Data value"
fi

# ── Test 2: Multiple button presses (Data increments) ─────────────────────
log "Test 2: Second button press with Data=2"

send_post '{"Device":"Wmos","Sensor":"ButtonD2","Data":2}' > /dev/null
sleep 1

if wait_for_output /tmp/pi2_out.txt "2" 5; then
    pass "Pi-2 printed incremented Data value"
else
    fail "Pi-2 did not print incremented Data value"
fi

# ── Test 3: Invalid JSON is handled gracefully ─────────────────────────────
log "Test 3: Invalid JSON body"

RESP=$(send_post 'this is not json')
if echo "$RESP" | grep -qi "error\|invalid"; then
    pass "Pi-1 returned error for invalid JSON"
else
    fail "Pi-1 did not return error for invalid JSON. Response: $RESP"
fi

# ── Test 4: Unknown device falls back to generic handler ──────────────────
log "Test 4: Unknown device dispatches to fallback handler"

PREV_LINES=$(wc -l < /tmp/pi2_out.txt)
send_post '{"Device":"UnknownSensor","Sensor":"TempSensor","Data":"hello"}' > /dev/null
sleep 1

NEW_LINES=$(wc -l < /tmp/pi2_out.txt)
if [ "$NEW_LINES" -gt "$PREV_LINES" ]; then
    pass "Pi-2 handled unknown device (produced output)"
else
    fail "Pi-2 produced no output for unknown device"
fi

# ── Test 5: String Data field ──────────────────────────────────────────────
log "Test 5: WMos with string Data"

send_post '{"Device":"Wmos","Sensor":"ButtonD2","Data":"pressed"}' > /dev/null
sleep 1

if wait_for_output /tmp/pi2_out.txt "pressed" 5; then
    pass "Pi-2 printed string Data value"
else
    fail "Pi-2 did not print string Data value"
fi

# ── Test 6: Full-duplex — Pi-2 ACK reaches Pi-1 ───────────────────────────
log "Test 6: Full-duplex — Pi-2 acknowledgement received by Pi-1"

# Pi-2 sends an ACK for every valid message; at least one should have
# appeared in Pi-1's output by now (tests 1/2/5 each triggered one).
if wait_for_output /tmp/pi1_out.txt "Received from Pi-2" 5; then
    pass "Pi-1 received message from Pi-2 (full-duplex confirmed)"
else
    fail "Pi-1 did not receive any message from Pi-2"
    echo "--- Pi-1 output ---"
    cat /tmp/pi1_out.txt
fi

# ── Test 7: Concurrent WMos connections (multi-threaded accept) ────────────
log "Test 7: Concurrent WMos connections handled in parallel"

PREV_LINES=$(wc -l < /tmp/pi2_out.txt)

# Fire three requests simultaneously; each should be processed independently.
CURL_PIDS=()
send_post '{"Device":"Wmos","Sensor":"ButtonD2","Data":10}' > /dev/null & CURL_PIDS+=($!)
send_post '{"Device":"Wmos","Sensor":"ButtonD2","Data":11}' > /dev/null & CURL_PIDS+=($!)
send_post '{"Device":"Wmos","Sensor":"ButtonD2","Data":12}' > /dev/null & CURL_PIDS+=($!)
wait "${CURL_PIDS[@]}"  # wait only for the curl jobs, not Pi-1/Pi-2

# Allow processing time.
sleep 1

NEW_LINES=$(wc -l < /tmp/pi2_out.txt)
if [ "$NEW_LINES" -gt "$PREV_LINES" ]; then
    pass "Pi-2 processed concurrent WMos connections"
else
    fail "Pi-2 produced no output for concurrent connections"
fi

# ── Summary ────────────────────────────────────────────────────────────────
echo ""
echo "==============================="
echo "Results: $PASS passed, $FAIL failed"
echo "==============================="

[ "$FAIL" -eq 0 ] && exit 0 || exit 1
