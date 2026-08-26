# QuantFX Production Contract

QuantFX can become production-oriented without hard-coding a broker, but funded execution still requires a concrete adapter implementation. The generic engine contract below is the boundary that every broker and data vendor must satisfy.

| Contract area | Required behavior | Generic implementation target |
|---|---|---|
| Configuration | Parse validated runtime settings and reject unsafe values | key-value configuration parser with checksum |
| Journal | Persist every accepted intent, acknowledgement, fill, rejection, and state transition | append-only length-delimited journal with flush policy |
| Recovery | Reconstruct cash, positions, orders, and sequence state after interruption | journal replay plus checkpoint compaction |
| Broker state | Handle idempotency, acknowledgements, rejects, cancels, fills, snapshots, and heartbeats | deterministic broker simulator and adapter protocol |
| Portfolio | Track currency exposure, average price, realized/unrealized PnL, margin, leverage, financing, and fees | fixed-point accounting subsystem |
| Strategy | Load versioned strategy modules and consume indicators and events | stable callback ABI and built-in indicator library |
| Research | Generate equity curves, trade ledgers, risk metrics, and experiment manifests | machine-readable and human-readable report outputs |
| Safety | Enforce stale data, spread, price deviation, rate, margin, loss, drawdown, and kill-switch controls | pre-trade gate plus durable operator state |
| Operations | Provide health, metrics, graceful shutdown, secrets isolation, and service supervision | Unix control socket and systemd hardening |

## Non-negotiable live gate

The engine must remain paper-only until a concrete adapter passes sandbox tests for authentication, reconnect, heartbeat, broker reconciliation, duplicate intent handling, partial fills, cancellation, and restart recovery. No generic implementation can infer vendor-specific account semantics or safely submit orders to an unspecified funded account.

## Source constraint

All assembly source added to the repository is comment-free. Documentation may contain explanatory prose, tables, and operational warnings.
