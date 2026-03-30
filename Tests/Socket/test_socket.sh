#!/usr/bin/env bash
# test_socket.sh — End-to-end tests for the Pi-B / Pi-A full-duplex pipeline.
#
# The script starts Pi-A and Pi-B as background processes, sends synthetic
# HTTP POST requests that mimic the WMos D1 Mini, and verifies that:
#   - Pi-A prints the expected dispatched output  (Pi-B → Pi-A direction).
#   - Pi-B receives acknowledgements from Pi-A   (Pi-A → Pi-B direction).
#
# Pre-built binaries:
#   Set PI_B_BINARY and PI_A_BINARY environment variables to use pre-compiled
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
# Backward compatible env vars are still supported:
#   PI_B_BINARY preferred over B_BINARY/PI1_BINARY
#   PI_A_BINARY preferred over A_BINARY/PI2_BINARY
if [ -n "${PI_B_BINARY:-}" ] && [ -x "${PI_B_BINARY}" ]; then
    B_BIN="${PI_BINARY}"
    COMPILED_B=false
elif [ -n "${B_BINARY:-}" ] && [ -x "${B_BINARY}" ]; then
    B_BIN="${B_BINARY}"
    COMPILED_B=false
elif [ -n "${PI1_BINARY:-}" ] && [ -x "${PI1_BINARY}" ]; then
    B_BIN="${PI1_BINARY}"
    COMPILED_B=false
else
    B_BIN="/tmp/Pi-B-test"
    COMPILED_B=true
fi

if [ -n "${PI_A_BINARY:-}" ] && [ -x "${PI_A_BINARY}" ]; then
    A_BIN="${PI_A_BINARY}"
    COMPILED_A=false
elif [ -n "${A_BINARY:-}" ] && [ -x "${A_BINARY}" ]; then
    A_BIN="${A_BINARY}"
    COMPILED_A=false
elif [ -n "${PI2_BINARY:-}" ] && [ -x "${PI2_BINARY}" ]; then
    A_BIN="${PI2_BINARY}"
    COMPILED_A=false
else
    A_BIN="/tmp/Pi-A-test"
    COMPILED_A=true
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
    [ -n "${B_PID:-}" ] && kill "$B_PID" 2>/dev/null || true
    [ -n "${A_PID:-}" ] && kill "$A_PID" 2>/dev/null || true
    [ -n "${TAILA:-}" ] && kill "$TAILA" 2>/dev/null || true
    [ -n "${TAILB:-}" ] && kill "$TAILB" 2>/dev/null || true
    # Only delete binaries that we compiled ourselves.
    [ "$COMPILED_B" = true ] && rm -f "$B_BIN" || true
    [ "$COMPILED_A" = true ] && rm -f "$A_BIN" || true
    rm -f /tmp/pi2_out.txt /tmp/pi1_out.txt
}
trap cleanup EXIT

# Named pipes used to simulate typing into Pi-A and Pi-B stdin.
PIA_FIFO=/tmp/piA_in
PIB_FIFO=/tmp/piB_in

cleanup_fifos() {
    rm -f "$PIA_FIFO" "$PIB_FIFO"
}
trap cleanup_fifos EXIT

# ── Build (only when pre-built binaries were not provided) ─────────────────

if [ "$COMPILED_B" = true ]; then
    log "Compiling Pi-B..."
    gcc -O2 -o "$B_BIN" "$SOCKET_DIR/Pi-B.c" "$SOCKET_DIR/cJSON.c" \
        -lm -lpthread || { echo "[FAIL] Compilation of Pi-B failed"; exit 1; }
else
    log "Using pre-built Pi-B: $B_BIN"
fi

if [ "$COMPILED_A" = true ]; then
    log "Compiling Pi-A..."
    gcc -O2 -o "$A_BIN" "$SOCKET_DIR/Pi-A.c" "$SOCKET_DIR/cJSON.c" \
        -lm -lpthread || { echo "[FAIL] Compilation of Pi-A failed"; exit 1; }
else
    log "Using pre-built Pi-A: $A_BIN"
fi

# ── Start servers ──────────────────────────────────────────────────────────

log "Creating FIFOs for simulated stdin: $PIA_FIFO, $PIB_FIFO"
rm -f "$PIA_FIFO" "$PIB_FIFO"
mkfifo "$PIA_FIFO" "$PIB_FIFO"
tail -f /dev/null > "$PIA_FIFO" &
TAILA=$!
tail -f /dev/null > "$PIB_FIFO" &
TAILB=$!

log "Starting Pi-A on port $PI2_PORT..."
"$A_BIN" "$PI2_PORT" < "$PIA_FIFO" > /tmp/pi2_out.txt 2>&1 &
A_PID=$!

# Wait until Pi-A's port is in LISTEN state (up to 10 s).
# Using ss avoids actually connecting to Pi-A's single-accept slot.
log "Waiting for Pi-A to be ready..."
for i in $(seq 1 20); do
    ss -tlnp 2>/dev/null | grep -q ":${PI2_PORT}" && break
    sleep 0.5
done

log "Starting Pi-B on port $PI1_PORT forwarding to 127.0.0.1:$PI2_PORT..."
"$B_BIN" "$PI1_PORT" 127.0.0.1 "$PI2_PORT" < "$PIB_FIFO" > /tmp/pi1_out.txt 2>&1 &
B_PID=$!

# Wait until Pi-B's WMos port is in LISTEN state.
# Pi-B only reaches listen() after successfully connecting to Pi-A,
# so this also confirms the Pi-B ↔ Pi-A connection is up.
log "Waiting for Pi-B to be ready..."
for i in $(seq 1 20); do
    ss -tlnp 2>/dev/null | grep -q ":${PI1_PORT}" && break
    sleep 0.5
done

# ── Test helpers ───────────────────────────────────────────────────────────

# send_post <json_body>
# Sends an HTTP POST to Pi-B and returns the response body.
send_post() {
    local body="$1"
        curl -s --max-time 5 \
            -X POST "http://127.0.0.1:${PI1_PORT}/" \
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
    pass "Pi-B acknowledged forward"
else
    fail "Pi-B did not acknowledge forward. Response: $RESP"
fi

if wait_for_output /tmp/pi2_out.txt "Wmos" 5; then
    pass "Pi-A printed Device=Wmos"
else
    fail "Pi-A did not print Device=Wmos"
    cat /tmp/pi2_out.txt
fi

if wait_for_output /tmp/pi2_out.txt "ButtonD2" 5; then
    pass "Pi-A printed Sensor name"
else
    fail "Pi-A did not print Sensor name"
fi

if wait_for_output /tmp/pi2_out.txt "1" 5; then
    pass "Pi-A printed Data value"
else
    fail "Pi-A did not print Data value"
fi

# ── Test 2: Multiple button presses (Data increments) ─────────────────────
log "Test 2: Second button press with Data=2"

send_post '{"Device":"Wmos","Sensor":"ButtonD2","Data":2}' > /dev/null
sleep 1

if wait_for_output /tmp/pi2_out.txt "2" 5; then
    pass "Pi-A printed incremented Data value"
else
    fail "Pi-A did not print incremented Data value"
fi

# ── Test 3: Invalid JSON is handled gracefully ─────────────────────────────
log "Test 3: Invalid JSON body"

RESP=$(send_post 'this is not json')
if echo "$RESP" | grep -qi "error\|invalid"; then
    pass "Pi-B returned error for invalid JSON"
else
    fail "Pi-B did not return error for invalid JSON. Response: $RESP"
fi

# ── Test 4: Unknown device falls back to generic handler ──────────────────
log "Test 4: Unknown device dispatches to fallback handler"

PREV_LINES=$(wc -l < /tmp/pi2_out.txt)
send_post '{"Device":"UnknownSensor","Sensor":"TempSensor","Data":"hello"}' > /dev/null
sleep 1

NEW_LINES=$(wc -l < /tmp/pi2_out.txt)
if [ "$NEW_LINES" -gt "$PREV_LINES" ]; then
    pass "Pi-A handled unknown device (produced output)"
else
    fail "Pi-A produced no output for unknown device"
fi

# ── Test 5: String Data field ──────────────────────────────────────────────
log "Test 5: WMos with string Data"

send_post '{"Device":"Wmos","Sensor":"ButtonD2","Data":"pressed"}' > /dev/null
sleep 1

if wait_for_output /tmp/pi2_out.txt "pressed" 5; then
    pass "Pi-A printed string Data value"
else
    fail "Pi-A did not print string Data value"
fi

# ── Test 6: Full-duplex — Pi-A ACK reaches Pi-B ───────────────────────────
log "Test 6: Full-duplex — Pi-A acknowledgement received by Pi-B"

# Pi-A sends an ACK for every valid message; at least one should have
# appeared in Pi-B's output by now (tests 1/2/5 each triggered one).
if wait_for_output /tmp/pi1_out.txt "Received from A" 5; then
    pass "Pi-B received message from Pi-A (full-duplex confirmed)"
else
    fail "Pi-B did not receive any message from Pi-A"
    echo "--- Pi-B output ---"
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
    pass "Pi-A processed concurrent WMos connections"
else
    fail "Pi-A produced no output for concurrent connections"
fi

# ── Test 8: Pi-B typed stdin command forwarded to Pi-A ─────────────────────
log "Test 8: Pi-B types 'Wmos,FromB,99' -> forwarded to Pi-A"
printf '%s\n' 'Wmos,FromB,99' > "$PIB_FIFO"
if wait_for_output /tmp/pi2_out.txt "FromB" 5 && wait_for_output /tmp/pi2_out.txt "99" 5; then
    pass "Pi-A received and printed command from Pi-B via stdin"
else
    fail "Pi-A did not show forwarded stdin command from Pi-B"
    echo "--- Pi-A output ---"
    cat /tmp/pi2_out.txt
fi

# ── Test 9: Pi-A typed stdin command forwarded to Pi-B ─────────────────────
log "Test 9: Pi-A types 'Wmos,FromA,123' -> forwarded to Pi-B"
printf '%s\n' 'Wmos,FromA,123' > "$PIA_FIFO"
if wait_for_output /tmp/pi1_out.txt "FromA" 5 && wait_for_output /tmp/pi1_out.txt "123" 5; then
    pass "Pi-B received and printed command from Pi-A via stdin"
else
    fail "Pi-B did not show forwarded stdin command from Pi-A"
    echo "--- Pi-B output ---"
    cat /tmp/pi1_out.txt
fi

# ── Summary ────────────────────────────────────────────────────────────────
echo ""
echo "==============================="
echo "Results: $PASS passed, $FAIL failed"
echo "==============================="

[ "$FAIL" -eq 0 ] && exit 0 || exit 1
