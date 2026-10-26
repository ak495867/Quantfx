#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
./build/qfx_hybrid ring-benchmark 1000000 | grep -q 'ring_iterations=1000000'
./build/qfx_hybrid alpha-backtest fixtures/eurusd_sample.csv | grep -q 'events=7'
echo HYBRID_PASS
