#!/usr/bin/env bash
# test_socket.sh — End-to-end tests for the Pi-B / Pi-A full-duplex pipeline.
#
# Root cause history:
#   The CI run showed:
#     "ioctl SIOCGIFINDEX: No such device"
#   …emitted by Pi-A.c at start-up.  Pi-A.c opens a SocketCAN socket and
#   binds it to the interface "can0".  The hosted GitHub runner has no
#   CAN hardware, so the ioctl fails and Pi-A *exits before opening its
#   TCP listener*.  Pi-B then can't reach Pi-A, every Wmos POST fails,
#   and Test 9 hung forever writing into Pi-A's FIFO (no reader).
#
# This version of the script:
#   1. Creates a virtual CAN device (vcan) named "can0" if it does not
#      already exist, so Pi-A can boot successfully on CI.
#   2. Wraps every FIFO write in `timeout` so a missing reader cannot
#      hang the script (Test 9 hang fix).
#   3. Uses a single, unified cleanup trap that actually kills the
#      background binaries and tail keep-alives.
#
# Pre-built binaries:
#   Set PI_B_BINARY and PI_A_BINARY to use pre-compiled binaries
#   (e.g. downloaded from a CI artifact).  When these variables are not
#   set, the script compiles its own test copies from source.
#
# Exit codes:
#   0  All tests passed.
#   1  One or more tests failed.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOCKET_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)/Socket"

# ── Binary selection (env vars override) ──────────────────────────────────
if [[ -n "${PI_B_BINARY:-}" && -x "${PI_B_BINARY}" ]]; then
    B_BIN="${PI_B_BINARY}"; COMPILED_B=false
elif [[ -n "${B_BINARY:-}" && -x "${B_BINARY}" ]]; then
    B_BIN="${B_BINARY}"; COMPILED_B=false
elif [[ -n "${PI1_BINARY:-}" && -x "${PI1_BINARY}" ]]; then
    B_BIN="${PI1_BINARY}"; COMPILED_B=false
else
    B_BIN="/tmp/Pi-B-test"; COMPILED_B=true
fi

if [[ -n "${PI_A_BINARY:-}" && -x "${PI_A_BINARY}" ]]; then
    A_BIN="${PI_A_BINARY}"; COMPILED_A=false
elif [[ -n "${A_BINARY:-}" && -x "${A_BINARY}" ]]; then
    A_BIN="${A_BINARY}"; COMPILED_A=false
elif [[ -n "${PI2_BINARY:-}" && -x "${PI2_BINARY}" ]]; then
    A_BIN="${PI2_BINARY}"; COMPILED_A=false
else
    A_BIN="/tmp/Pi-A-test"; COMPILED_A=true
fi

PI1_PORT=19000
PI2_PORT=19001

PASS=0
FAIL=0

# ── Helpers ────────────────────────────────────────────────────────────────
log()  { echo "[TEST] $*"; }
pass() { echo "[PASS] $*"; PASS=$((PASS + 1)); }
fail() { echo "[FAIL] $*"; FAIL=$((FAIL + 1)); }

PIA_FIFO=/tmp/piA_in
PIB_FIFO=/tmp/piB_in

cleanup() {
    [[ -n "${B_PID:-}" ]] && kill "$B_PID" 2>/dev/null || true
    [[ -n "${A_PID:-}" ]] && kill "$A_PID" 2>/dev/null || true
    [[ -n "${TAILA:-}" ]] && kill "$TAILA" 2>/dev/null || true
    [[ -n "${TAILB:-}" ]] && kill "$TAILB" 2>/dev/null || true
    wait 2>/dev/null || true
    [[ "${COMPILED_B:-false}" == true ]] && rm -f "$B_BIN" || true
    [[ "${COMPILED_A:-false}" == true ]] && rm -f "$A_BIN" || true
    rm -f "$PIA_FIFO" "$PIB_FIFO" /tmp/pi1_out.txt /tmp/pi2_out.txt
}
trap cleanup EXIT

# Non-blocking write to a FIFO — can never hang the script.
fifo_write() {
    local fifo="$1"
    local line="$2"
    ( timeout 2 bash -c "printf '%s\n' \"\$0\" > \"\$1\"" "$line" "$fifo" \
        || echo "[WARN] FIFO write to $fifo timed out (reader stuck or dead)" >&2 ) &
}

# ── Ensure vcan "can0" exists (fixes Pi-A SIOCGIFINDEX failure on CI) ─────
ensure_vcan() {
    if ip link show can0 >/dev/null 2>&1; then
        log "Interface can0 already exists."
        return 0
    fi
    log "Interface can0 not present — attempting to create vcan can0..."
    local SUDO=""
    if [[ "$(id -u)" -ne 0 ]]; then
        if command -v sudo >/dev/null 2>&1; then SUDO="sudo"; fi
    fi
    $SUDO modprobe vcan 2>/dev/null || true
    if $SUDO ip link add dev can0 type vcan 2>/dev/null; then
        $SUDO ip link set up can0 || true
        log "vcan can0 created and brought up."
    else
        echo "[WARN] Could not create vcan can0. Pi-A may fail SIOCGIFINDEX." >&2
    fi
}
ensure_vcan

# ── Build (only when pre-built binaries were not provided) ────────────────
if [[ "$COMPILED_B" == true ]]; then
    log "Compiling Pi-B..."
    gcc -O2 -o "$B_BIN" "$SOCKET_DIR/Pi-B.c" "$SOCKET_DIR/cJSON.c" \
        -lm -lpthread || { echo "[FAIL] Compilation of Pi-B failed"; exit 1; }
else
    log "Using pre-built Pi-B: $B_BIN"
fi

if [[ "$COMPILED_A" == true ]]; then
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

# Keep the write end of each FIFO open so the binaries never see EOF on stdin.
tail -f /dev/null > "$PIA_FIFO" &
TAILA=$!
tail -f /dev/null > "$PIB_FIFO" &
TAILB=$!

log "Starting Pi-A on port $PI2_PORT..."
"$A_BIN" "$PI2_PORT" < "$PIA_FIFO" > /tmp/pi2_out.txt 2>&1 &
A_PID=$!

log "Waiting for Pi-A to be ready..."
PIA_READY=false
for i in $(seq 1 20); do
    if ss -tlnp 2>/dev/null | grep -q ":${PI2_PORT}"; then
        PIA_READY=true
        break
    fi
    # Detect early exit (e.g. SIOCGIFINDEX failure)
    if ! kill -0 "$A_PID" 2>/dev/null; then
        echo "[FAIL] Pi-A exited before opening port ${PI2_PORT}. Output:"
        cat /tmp/pi2_out.txt || true
        break
    fi
    sleep 0.5
done
$PIA_READY || echo "[WARN] Pi-A not listening on ${PI2_PORT}; tests will likely fail."

log "Starting Pi-B on port $PI1_PORT forwarding to 127.0.0.1:$PI2_PORT..."
"$B_BIN" "$PI1_PORT" 127.0.0.1 "$PI2_PORT" < "$PIB_FIFO" > /tmp/pi1_out.txt 2>&1 &
B_PID=$!

log "Waiting for Pi-B to be ready..."
for i in $(seq 1 20); do
    ss -tlnp 2>/dev/null | grep -q ":${PI1_PORT}" && break
    sleep 0.5
done

# Give the back-channel (Pi-A → Pi-B) a moment to establish.
sleep 1

# ── Test helpers ───────────────────────────────────────────────────────────
send_post() {
    local body="$1"
    curl -s --max-time 5 \
        -X POST "http://127.0.0.1:${PI1_PORT}/" \
        -H "Content-Type: application/json" \
        -d "$body" || echo ""
}

# Wait up to <timeout> seconds for <needle> to appear in <file>.
wait_for_output() {
    local file="$1"
    local needle="$2"
    local timeout="${3:-5}"
    local iterations=$(( timeout * 5 ))
    local i
    for ((i = 0; i < iterations; i++)); do
        grep -qF "$needle" "$file" && return 0
        sleep 0.2
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
if wait_for_output /tmp/pi1_out.txt "Received from A" 5; then
    pass "Pi-B received message from Pi-A (full-duplex confirmed)"
else
    fail "Pi-B did not receive any message from Pi-A"
    echo "--- Pi-B output ---"
    cat /tmp/pi1_out.txt
fi

# ── Test 7: Concurrent WMos connections ───────────────────────────────────
log "Test 7: Concurrent WMos connections handled in parallel"
PREV_LINES=$(wc -l < /tmp/pi2_out.txt)
CURL_PIDS=()
send_post '{"Device":"Wmos","Sensor":"ButtonD2","Data":10}' > /dev/null & CURL_PIDS+=($!)
send_post '{"Device":"Wmos","Sensor":"ButtonD2","Data":11}' > /dev/null & CURL_PIDS+=($!)
send_post '{"Device":"Wmos","Sensor":"ButtonD2","Data":12}' > /dev/null & CURL_PIDS+=($!)
wait "${CURL_PIDS[@]}"
sleep 1
NEW_LINES=$(wc -l < /tmp/pi2_out.txt)
if [ "$NEW_LINES" -gt "$PREV_LINES" ]; then
    pass "Pi-A processed concurrent WMos connections"
else
    fail "Pi-A produced no output for concurrent connections"
fi

# ── Test 8: Pi-B typed stdin command forwarded to Pi-A ─────────────────────
log "Test 8: Pi-B types 'Wmos,FromB,99' -> forwarded to Pi-A"
fifo_write "$PIB_FIFO" 'Wmos,FromB,99'
if wait_for_output /tmp/pi2_out.txt "FromB" 5 && wait_for_output /tmp/pi2_out.txt "99" 5; then
    pass "Pi-A received and printed command from Pi-B via stdin"
else
    fail "Pi-A did not show forwarded stdin command from Pi-B"
    echo "--- Pi-A output ---"
    cat /tmp/pi2_out.txt
fi

# ── Test 9: Pi-A typed stdin command forwarded to Pi-B ─────────────────────
log "Test 9: Pi-A types 'Wmos,FromA,123' -> forwarded to Pi-B"
fifo_write "$PIA_FIFO" 'Wmos,FromA,123'
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

[[ "$FAIL" -eq 0 ]] && exit 0 || exit 1
