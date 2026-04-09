#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
DATA_DIR="$ROOT_DIR/data"
SERVER_BIN="$ROOT_DIR/bin/kvstore_server"
CLIENT_BIN="$ROOT_DIR/bin/kvstore_client"
HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-6380}"
TEST_KEY="${TEST_KEY:-wal-recovery-key}"
TEST_VALUE="${TEST_VALUE:-wal-recovery-value}"

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    cat <<'EOF'
Usage: ./scripts/verify_wal_recovery.sh

Environment overrides:
  HOST       server host (default 127.0.0.1)
  PORT       server port (default 6380)
  TEST_KEY   key written before restart (default wal-recovery-key)
  TEST_VALUE value written before restart (default wal-recovery-value)
EOF
    exit 0
fi

cleanup() {
    if [[ -n "${SERVER_PID:-}" ]]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
        unset SERVER_PID
    fi
}

start_server() {
    cleanup
    "$SERVER_BIN" >"/tmp/kvstore_wal_recovery_server.log" 2>&1 &
    SERVER_PID=$!
    sleep 1
}

send_commands() {
    local commands="$1"
    printf "%b" "$commands" | "$CLIENT_BIN" "$HOST" "$PORT"
}

trap cleanup EXIT

rm -rf "$DATA_DIR"
mkdir -p "$DATA_DIR"

cmake --build "$BUILD_DIR" -j

start_server
send_commands "PUT ${TEST_KEY} ${TEST_VALUE}\nQUIT\n" >/tmp/kvstore_wal_recovery_put.out
cleanup

start_server
GET_OUTPUT="$(send_commands "GET ${TEST_KEY}\nQUIT\n" 2>/tmp/kvstore_wal_recovery_get.err)"

if [[ "$GET_OUTPUT" != *"VALUE ${TEST_VALUE}"* ]]; then
    echo "WAL recovery verification failed"
    echo "$GET_OUTPUT"
    exit 1
fi

echo "WAL recovery verification passed"
