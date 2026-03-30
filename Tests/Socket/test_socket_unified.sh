#!/usr/bin/env bash
set -euo pipefail
DIR=$(cd "$(dirname "$0")/../.." && pwd)
cd "$DIR"

echo "[TEST] Compiling unified socket..."
cd Socket
gcc -O2 -Wall -pthread -o socket socket.c cJSON.c -lm
cd -

# start server (peer listen 19001, server WMos port 19002)
SERVER_OUT=/tmp/socket_server_out.txt
CLIENT_OUT=/tmp/socket_client_out.txt
rm -f "$SERVER_OUT" "$CLIENT_OUT"

echo "[TEST] Starting server..."
Socket/socket 19001 19002 >"$SERVER_OUT" 2>&1 &
SERVER_PID=$!
sleep 0.5

echo "[TEST] Starting client..."
Socket/socket 19000 127.0.0.1 19001 >"$CLIENT_OUT" 2>&1 &
CLIENT_PID=$!

sleep 0.5

# send a WMos POST to client (which should forward to server)
echo "[TEST] Sending WMos POST to client..."
curl -s -X POST -H "Content-Type: application/json" -d '{"Device":"wmos","Sensor":"T1","Data":42}' http://127.0.0.1:19000/ || true
sleep 0.5

# check server output
if grep -q "Device: wmos" "$SERVER_OUT"; then
  echo "[PASS] Server received Device: wmos"
else
  echo "[FAIL] Server did not receive Device: wmos"
  tail -n +1 "$SERVER_OUT"
  kill $SERVER_PID $CLIENT_PID || true
  exit 1
fi

# cleanup
kill $SERVER_PID $CLIENT_PID || true
sleep 0.2
rm -f "$SERVER_OUT" "$CLIENT_OUT"

echo "[TEST] Unified socket basic test passed"
