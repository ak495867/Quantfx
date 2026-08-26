#!/usr/bin/env bash
set -euo pipefail
broker_order_id=0
while IFS=, read -r side client_order_id quantity price timestamp sequence; do
    [[ -z "${side:-}" ]] && continue
    broker_order_id=$((broker_order_id + 1))
    printf 'ACK,%s,%s\n' "$client_order_id" "$broker_order_id"
    printf 'FILL,%s,%s,%s,0,%s\n' "$client_order_id" "$quantity" "$price" "$timestamp"
done
