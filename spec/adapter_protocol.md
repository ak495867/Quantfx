# Adapter Protocol

## Market data

A market-data adapter emits UTF-8 records with one event per line:

`timestamp_ns,symbol,bid,ask,last,volume,bid_size,ask_size,sequence,flags`

Prices are integer fixed-point values at the configured instrument scale. The adapter owns authentication, vendor throttling, reconnect behavior, symbol mapping, session calendars, and vendor-specific timestamp conversion. A finite file is suitable for historical ingestion and replay. A long-lived named pipe or socket is suitable for live operation.

## Broker orders

The core emits one line per accepted live intent:

`SIDE,client_order_id,quantity,price,timestamp_ns,sequence`

The broker adapter returns one line per state transition:

`ACK,client_order_id,broker_order_id`

`FILL,client_order_id,quantity,price,fee,timestamp_ns`

`REJECT,client_order_id,reason_code`

`CANCEL_ACK,client_order_id`

`POSITION,symbol,quantity,average_price`

`HEARTBEAT,sequence,timestamp_ns`

Adapters must preserve client order IDs, reject duplicate IDs idempotently, and never convert a reject into a fill. The core treats missing heartbeats, sequence gaps, and reconciliation mismatches as a trading halt.

## Vendor neutrality

A vendor adapter may be implemented as a process, a shared object, a FIX gateway, a terminal bridge, an HTTP client, a WebSocket client, or a file decoder. The core only depends on the canonical records above. Multiple feeds can be merged by assigning monotonically increasing source sequences before they cross the adapter boundary.
