#!/usr/bin/env bash
set -euo pipefail
mode=${1:-market}
if [[ "$mode" == "market" ]]; then
    exec your_vendor_market_client --format canonical_csv
fi
if [[ "$mode" == "broker" ]]; then
    exec your_vendor_broker_client --format qfx-broker
fi
printf 'unsupported adapter mode: %s\n' "$mode" >&2
exit 2
