#!/usr/bin/env bash
# test_socket.sh — End-to-end tests for the Pi-1 / Pi-2 full-duplex pipeline.
#
# The script compiles the binaries, starts Pi-2 and Pi-1 as background
# processes, sends synthetic HTTP POST requests that mimic the WMos D1 Mini,
# and verifies that:
#   - Pi-2 prints the expected dispatched output  (Pi-1 → Pi-2 direction).
#   - Pi-1 receives acknowledgements from Pi-2    (Pi-2 → Pi-1 direction).
#
# Exit codes:
#   0  All tests passed.
#   1  One or more tests failed.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SOCKET_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

PI1_BIN="$SOCKET_DIR/Pi-1-test"
PI2_BIN="$SOCKET_DIR/Pi-2-test"

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
    rm -f "$PI1_BIN" "$PI2_BIN" /tmp/pi2_out.txt /tmp/pi1_out.txt
}
trap cleanup EXIT

# ── Build ──────────────────────────────────────────────────────────────────

log "Compiling Pi-1..."
gcc -O2 -o "$PI1_BIN" "$SOCKET_DIR/Pi-1.c" "$SOCKET_DIR/cJSON.c" -lm -lpthread || {
    echo "[FAIL] Compilation of Pi-1 failed"; exit 1
}

log "Compiling Pi-2..."
gcc -O2 -o "$PI2_BIN" "$SOCKET_DIR/Pi-2.c" "$SOCKET_DIR/cJSON.c" -lm -lpthread || {
    echo "[FAIL] Compilation of Pi-2 failed"; exit 1
}

# ── Start servers ──────────────────────────────────────────────────────────

log "Starting Pi-2 on port $PI2_PORT..."
"$PI2_BIN" "$PI2_PORT" > /tmp/pi2_out.txt 2>&1 &
PI2_PID=$!

# Give Pi-2 a moment to bind before Pi-1 connects.
sleep 0.5

log "Starting Pi-1 on port $PI1_PORT forwarding to localhost:$PI2_PORT..."
"$PI1_BIN" "$PI1_PORT" localhost "$PI2_PORT" > /tmp/pi1_out.txt 2>&1 &
PI1_PID=$!

# Give both processes time to establish the persistent connection.
sleep 1

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

RESP=$(send_post '{"Device":"Wmos","Button":"Buttons, servos, temp","Data":1}')

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

if wait_for_output /tmp/pi2_out.txt "Buttons, servos, temp" 5; then
    pass "Pi-2 printed Button category"
else
    fail "Pi-2 did not print Button category"
fi

if wait_for_output /tmp/pi2_out.txt "1" 5; then
    pass "Pi-2 printed Data value"
else
    fail "Pi-2 did not print Data value"
fi

# ── Test 2: Multiple button presses (Data increments) ─────────────────────
log "Test 2: Second button press with Data=2"

send_post '{"Device":"Wmos","Button":"Buttons, servos, temp","Data":2}' > /dev/null
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
send_post '{"Device":"UnknownSensor","Button":"test","Data":"hello"}' > /dev/null
sleep 1

NEW_LINES=$(wc -l < /tmp/pi2_out.txt)
if [ "$NEW_LINES" -gt "$PREV_LINES" ]; then
    pass "Pi-2 handled unknown device (produced output)"
else
    fail "Pi-2 produced no output for unknown device"
fi

# ── Test 5: String Data field ──────────────────────────────────────────────
log "Test 5: WMos with string Data"

send_post '{"Device":"Wmos","Button":"Buttons, servos, temp","Data":"pressed"}' > /dev/null
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

# ── Summary ────────────────────────────────────────────────────────────────
echo ""
echo "==============================="
echo "Results: $PASS passed, $FAIL failed"
echo "==============================="

[ "$FAIL" -eq 0 ] && exit 0 || exit 1
