#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
SERVER_BIN="$ROOT_DIR/bin/kvstore_server"
BENCH_BIN="$ROOT_DIR/bin/kvstore_bench"
HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-6380}"
OPERATIONS="${OPERATIONS:-20}"
PIPELINE_DEPTH="${PIPELINE_DEPTH:-1}"

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    cat <<'EOF'
Usage: ./scripts/verify_protocol_regression.sh

Environment overrides:
  HOST            server host (default 127.0.0.1)
  PORT            server port (default 6380)
  OPERATIONS      requests per phase (default 20)
  PIPELINE_DEPTH  in-flight requests per batch (default 1)
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

trap cleanup EXIT

cmake --build "$BUILD_DIR" -j

"$SERVER_BIN" --no-wal >"/tmp/kvstore_protocol_regression_server.log" 2>&1 &
SERVER_PID=$!
sleep 1

"$BENCH_BIN" "$HOST" "$PORT" "$OPERATIONS" "$PIPELINE_DEPTH" full

echo "Protocol regression verification passed"
