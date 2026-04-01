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
CLIENTS="${CLIENTS:-8 16}"

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
    local total_requests=$((clients * MULTI_OPS * 2))
    local start_ns
    local end_ns
    local elapsed_ns

    start_ns=$(date +%s%N)
    seq "$clients" | xargs -I{} -P "$clients" \
        "$BENCH_BIN" "$HOST" "$PORT" "$MULTI_OPS" \
        >"/tmp/kvbench_${clients}.out"
    end_ns=$(date +%s%N)
    elapsed_ns=$((end_ns - start_ns))

    awk -v clients="$clients" -v ops="$MULTI_OPS" -v total="$total_requests" -v elapsed_ns="$elapsed_ns" '
        BEGIN {
            seconds = elapsed_ns / 1000000000.0;
            qps = total / seconds;
            printf("clients=%d ops_per_client=%d total_requests=%d wall_seconds=%.4f aggregate_qps=%.2f\n",
                   clients, ops, total, seconds, qps);
        }'
}

run_case() {
    local name="$1"
    shift

    echo "== $name =="
    start_server "$name" "$@"

    echo "-- single client --"
    "$BENCH_BIN" "$HOST" "$PORT" "$SINGLE_OPS"

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
