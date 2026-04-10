#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-8765}"
URL="http://${HOST}:${PORT}/defense_dashboard.html"

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    cat <<'EOF'
Usage: ./scripts/verify_demo_http.sh

Environment overrides:
  HOST  demo HTTP host (default 127.0.0.1)
  PORT  demo HTTP port (default 8765)
EOF
    exit 0
fi

cleanup() {
    if [[ -n "${DEMO_PID:-}" ]]; then
        kill "$DEMO_PID" 2>/dev/null || true
        wait "$DEMO_PID" 2>/dev/null || true
        unset DEMO_PID
    fi
}

trap cleanup EXIT

python3 "$ROOT_DIR/demo/defense_demo_server.py" >"/tmp/kvstore_demo_http.log" 2>&1 &
DEMO_PID=$!
sleep 1

HTML="$(NO_PROXY=127.0.0.1,localhost curl --noproxy '*' -fsS "$URL")"
if [[ "$HTML" != *"KV-Store 毕业答辩展示面板"* ]]; then
    echo "Demo HTTP verification failed"
    exit 1
fi

echo "Demo HTTP verification passed"
