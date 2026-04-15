#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
COMPARE_BIN="$ROOT_DIR/bin/kvstore_compare_bench"

OPS_PER_THREAD="${OPS_PER_THREAD:-500}"
MAX_THREADS="${MAX_THREADS:-8}"
OUTPUT_FILE="${OUTPUT_FILE:-}"
PRELOAD_KEYS="${PRELOAD_KEYS:-0}"
WORKLOAD="${WORKLOAD:-mixed}"

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    cat <<'EOF'
Usage: ./scripts/run_compare_bench.sh

Environment overrides:
  OPS_PER_THREAD benchmark operations per thread (default 500)
  MAX_THREADS    maximum thread count in sweep (default 8)
  PRELOAD_KEYS   keys inserted before benchmark starts (default 0)
  WORKLOAD       mixed | read | read-all | write (default mixed)
  OUTPUT_FILE    optional file path for tee output
EOF
    exit 0
fi

cmake --build "$BUILD_DIR" -j

if [[ -n "$OUTPUT_FILE" ]]; then
    mkdir -p "$(dirname "$OUTPUT_FILE")"
    "$COMPARE_BIN" "$OPS_PER_THREAD" "$MAX_THREADS" "$PRELOAD_KEYS" "$WORKLOAD" | tee "$OUTPUT_FILE"
else
    "$COMPARE_BIN" "$OPS_PER_THREAD" "$MAX_THREADS" "$PRELOAD_KEYS" "$WORKLOAD"
fi
