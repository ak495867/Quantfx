#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
make clean >/dev/null
make >/dev/null
csv_output=$(./build/qfx validate fixtures/eurusd_sample.csv)
grep -q 'valid_events=7' <<<"$csv_output"
grep -q 'invalid_lines=2' <<<"$csv_output"
rm -f fixtures/eurusd_sample.qfx
./build/qfx ingest fixtures/eurusd_sample.csv fixtures/eurusd_sample.qfx >/dev/null
size=$(stat -c '%s' fixtures/eurusd_sample.qfx)
test "$size" -eq 624
qfx_output=$(./build/qfx validate fixtures/eurusd_sample.qfx)
grep -q 'valid_events=7' <<<"$qfx_output"
grep -q 'invalid_lines=0' <<<"$qfx_output"
backtest_output=$(./build/qfx backtest fixtures/eurusd_sample.csv)
grep -q 'events=7' <<<"$backtest_output"
grep -q 'fills=6' <<<"$backtest_output"
route_buy=$(./build/qfx route fixtures/broker_quotes.csv buy)
grep -q 'broker=BROKER_SMALL' <<<"$route_buy"
grep -q 'score=300' <<<"$route_buy"
grep -q 'price=108020' <<<"$route_buy"
route_sell=$(./build/qfx route fixtures/broker_quotes.csv sell)
grep -q 'broker=BROKER_SMALL' <<<"$route_sell"
grep -q 'score=300' <<<"$route_sell"
grep -q 'price=107995' <<<"$route_sell"
rm -f data/live.journal data/live.checkpoint fixtures/orders.pipe /tmp/qfx_orders.out /tmp/qfx_live.out
mkfifo fixtures/orders.pipe
cat fixtures/orders.pipe >/tmp/qfx_orders.out &
catpid=$!
sleep 1
./build/qfx live fixtures/eurusd_sample.csv fixtures/orders.pipe >/tmp/qfx_live.out
wait "$catpid"
grep -q 'BUY ,1,1000,108035,1000001000,2' /tmp/qfx_orders.out
grep -q 'SELL,5,1000,107970,1000005000,6' /tmp/qfx_orders.out
grep -q 'adapters opened' /tmp/qfx_live.out
test "$(stat -c '%s' data/live.journal)" -eq 480
test "$(stat -c '%s' data/live.checkpoint)" -eq 48
adapters/broker_simulator.sh </tmp/qfx_orders.out >/tmp/qfx_broker_events.out
test "$(grep -c '^ACK,' /tmp/qfx_broker_events.out)" -eq 6
test "$(grep -c '^FILL,' /tmp/qfx_broker_events.out)" -eq 6
rm -f data/live.journal data/live.checkpoint
stream_output=$(cat fixtures/eurusd_sample.csv | ./build/qfx live - - 2>/dev/null)
grep -q 'BUY ,1,1000,108035,1000001000,2' <<<"$stream_output"
grep -q 'SELL,5,1000,107970,1000005000,6' <<<"$stream_output"
rm -f fixtures/orders.pipe
echo PASS
