#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
DATA_DIR="$ROOT_DIR/data"
SERVER_BIN="$ROOT_DIR/bin/kvstore_server"
BENCH_BIN="$ROOT_DIR/bin/kvstore_bench"

HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-6380}"
SINGLE_OPS="${SINGLE_OPS:-1000}"
MULTI_OPS="${MULTI_OPS:-500}"
PIPELINE_DEPTH="${PIPELINE_DEPTH:-1}"
MULTI_PIPELINE_DEPTH="${MULTI_PIPELINE_DEPTH:-8}"
SCENARIO="${SCENARIO:-put-get}"
CLIENTS="${CLIENTS:-8 16}"

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    cat <<'EOF'
Usage: ./scripts/run_network_bench.sh

Environment overrides:
  HOST                 server host (default 127.0.0.1)
  PORT                 server port (default 6380)
  SINGLE_OPS           single-client operations per phase (default 1000)
  MULTI_OPS            multi-client operations per phase (default 500)
  PIPELINE_DEPTH       single-client pipeline depth (default 1)
  MULTI_PIPELINE_DEPTH multi-client pipeline depth (default 8)
  SCENARIO             benchmark scenario: put-get | full (default put-get)
  CLIENTS              space-separated client counts (default "8 16")
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
    local name="$1"
    shift

    cleanup
    rm -rf "$DATA_DIR"
    mkdir -p "$DATA_DIR"

    "$SERVER_BIN" "$@" >"/tmp/${name}_server.log" 2>&1 &
    SERVER_PID=$!
    sleep 1
}

run_multi_client() {
    local clients="$1"
    "$BENCH_BIN" "$HOST" "$PORT" "$MULTI_OPS" "$MULTI_PIPELINE_DEPTH" "$SCENARIO" "$clients"
}

run_case() {
    local name="$1"
    shift

    echo "== $name =="
    start_server "$name" "$@"

    echo "-- single client --"
    "$BENCH_BIN" "$HOST" "$PORT" "$SINGLE_OPS" "$PIPELINE_DEPTH" "$SCENARIO"

    echo "-- multi client --"
    for client_count in $CLIENTS; do
        run_multi_client "$client_count"
    done
    echo
}

trap cleanup EXIT

cmake --build "$BUILD_DIR" -j

run_case "no_wal" --no-wal
run_case "with_wal_sync0" --wal-sync-ms 0
run_case "with_wal_sync10" --wal-sync-ms 10
