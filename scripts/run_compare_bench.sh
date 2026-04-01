#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
COMPARE_BIN="$ROOT_DIR/bin/kvstore_compare_bench"

OPS_PER_THREAD="${OPS_PER_THREAD:-500}"
MAX_THREADS="${MAX_THREADS:-8}"
OUTPUT_FILE="${OUTPUT_FILE:-}"

cmake --build "$BUILD_DIR" -j

if [[ -n "$OUTPUT_FILE" ]]; then
    mkdir -p "$(dirname "$OUTPUT_FILE")"
    "$COMPARE_BIN" "$OPS_PER_THREAD" "$MAX_THREADS" | tee "$OUTPUT_FILE"
else
    "$COMPARE_BIN" "$OPS_PER_THREAD" "$MAX_THREADS"
fi
