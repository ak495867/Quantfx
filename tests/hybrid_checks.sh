#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
./build/qfx_hybrid ring-benchmark 1000000 | grep -q 'ring_iterations=1000000'
./build/qfx_hybrid alpha-backtest fixtures/eurusd_sample.csv | grep -q 'events=7'
./build/qfx_hybrid l2-sweep 1 1000000 | grep -q 'target_qty=1000000'
./build/qfx_hybrid tsc-benchmark 100000 | grep -q 'tsc_iterations=100000'
./build/qfx_hybrid sbe-demo | grep -q 'sbe_verify=PASS'
./build/qfx_hybrid risk-demo | grep -q 'risk_gateway=PASS'
./build/qfx_hybrid sor-demo | grep -q 'sor_verify=PASS'
./build/qfx_hybrid simd-benchmark 100000 | grep -q 'simd_verify=PASS'
./build/qfx_hybrid wal-demo | grep -q 'wal_verify=PASS'
./build/qfx_hybrid var-demo | grep -q 'var_verify=PASS'
./build/qfx_hybrid net-demo | grep -q 'net_socket=PASS'
./build/qfx_hybrid latency-profile 1000 | grep -q 'profile_verify=PASS'
./build/qfx_hybrid venue-sim-demo | grep -q 'venue_sim_verify=PASS'
python3 tests/test_native.py | grep -q 'PYTHON_NATIVE_ALL_PASS'
echo HYBRID_PASS
