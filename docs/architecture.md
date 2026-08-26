# QuantFX Engine Architecture

## Scope

QuantFX is a comment-free NASM x86-64 quantitative engine with a validated Linux ELF64 runtime, deterministic historical processing, native QFX storage, a baseline smart router, live order serialization, and portable platform artifacts. The repository also contains Python runtime and research modules for durable order state, encrypted audit records, kill-switch policy, purged walk-forward evaluation, and implementation-shortfall analysis.

The validated native runtime is Linux x86-64. Windows PE/COFF, UEFI, BIOS, Darwin x86-64, Darwin arm64 source, and timer-driver artifacts are generated or statically checked where possible; native execution on those targets requires their target environments.

## Runtime modes

| Mode | Input | Output | Current guarantee |
|---|---|---|---|
| validate | canonical CSV or QFX | validation report | parser and monotonic-sequence checks |
| ingest | canonical CSV | QFX binary | 64-byte header and 80-byte fixed-width records |
| backtest | canonical CSV or QFX | fills and metrics | deterministic event traversal and bid/ask baseline fills |
| replay | canonical CSV or QFX | deterministic event traversal | repeatable processing for identical input |
| route | normalized venue quote CSV | selected broker, score, and price | stale and undersized venue exclusion plus cost baseline |
| live | file, FIFO, or stdin market stream | broker-sink order records, journal, checkpoint | bounded baseline risk gates and deterministic client IDs |
| research | canonical CSV | JSON fold or shortfall report | purged walk-forward and cost decomposition |

## Canonical market event

The canonical CSV schema is `timestamp_ns,symbol,bid,ask,last,volume,bid_size,ask_size,sequence,flags`. Timestamp, quantity, sequence, and flag fields are raw integers. Price fields use fixed-point integers with a default scale of 100000. The current assembly parser intentionally supports the restricted canonical form and does not provide a general quoted-CSV implementation.

## Native QFX storage

QFX currently uses a 64-byte header followed by 80-byte fixed-width event records. Ingestion finalizes the record count in the header after writing the records. The format is suitable for deterministic local replay and is not yet a versioned, checksummed, compressed, indexed production data lake format.

## Strategy and portfolio

The assembly executable contains one deterministic threshold strategy and a baseline fixed-point paper-fill path. The current portfolio tracks position, cash, equity, peak equity, drawdown, fills, trades, and basic PnL counters. A generalized external strategy ABI, full multi-currency accounting, margin, leverage tiers, financing, swaps, average-cost lots, and complete order-type semantics remain separate production work.

## Broker and routing boundary

The line-oriented broker boundary emits `SIDE,client_order_id,quantity,price,timestamp_ns,sequence`. The repository includes a deterministic broker simulator that emits ACK and FILL records for offline tests. The `route` command accepts `timestamp_ns,symbol,bid,ask,latency_us,slippage,fee,reject_ppm,age_ns,capacity` and scores eligible venue rows using spread, latency, slippage, fee, reject rate, age, and capacity.

The router is currently a deterministic quote-scoring baseline. It is not yet a live multi-broker quote fan-in service, parent-order planner, partial-fill allocator, cancel/replace state machine, or broker reconciliation manager. A concrete funded broker still requires a vendor-specific adapter for credentials, TLS, subscriptions, heartbeats, retries, acknowledgements, rejects, cancellations, fills, snapshots, and recovery.

## Risk and live state

The assembly live path enforces baseline spread, order-size, position-size, cash, sequence, and drawdown checks. It supports file, FIFO, stdin, and stdout or file broker sinks. It writes an append-only fixed-width event journal, fsyncs journal writes, writes a checkpoint containing position, cash, equity, peak, drawdown, and sequence state, and loads a valid checkpoint at live startup.

The Python runtime adds an explicit order transition graph, idempotent event identifiers, journal replay, reconciliation checks, and fail-closed behavior. The kill switch uses a signed atomic state file and treats missing, stale, or invalid state as blocked trading. The encrypted audit module uses AES-GCM records, per-record nonces, chained authenticated data, SHA-256 linkage, fsync, and verification. These Python controls are tested modules and are not yet fully linked into the assembly executable’s live order path.

## Research integrity

The purged walk-forward module creates explicit training, purge, test, and embargo boundaries and emits deterministic JSON fold output. The implementation-shortfall module decomposes execution cost into arrival, delay, spread, fees, modeled impact, opportunity cost, and total cost. Research results still require real vendor data, point-in-time feature construction, out-of-sample governance, and independent review before deployment decisions.

## Platform boundary

The current assembly source directly uses Linux syscall numbers and the System V AMD64 process entry convention. `platform/qfx_platform_abi.inc` defines the intended service-table boundary for exit, I/O, file operations, synchronization, timing, and networking. Windows NT dispatch veneers, Darwin service-table veneers, a Darwin arm64 syscall source file, UEFI and BIOS entry artifacts, and PIT/TSC/IDT timer primitives are supplied as platform work. They require native SDK, firmware, board, linker, and hardware validation before they can be called supported runtimes.

## Build and validation

`make` builds the validated Linux executable. `make platform` produces the available PE/COFF, EFI, BIOS, Darwin object, Windows NT syscall object, and timer artifacts. `make check` runs the assembly regression suite, platform-format checks, and Python runtime tests. `make qemu-firmware` runs the BIOS smoke test when QEMU is installed and otherwise reports a non-failing skip. The CI workflow defines Linux, macOS, Windows, and firmware validation jobs.

## Security and operational boundary

Credentials are not stored in the repository. The secure audit key, kill-switch key, broker credentials, and configuration secrets must be supplied through protected deployment mechanisms. The current repository provides baseline persistence, signed kill-switch policy, encrypted audit functionality, and test automation; it does not claim a complete secrets manager, HSM integration, secure boot chain, remote operator control plane, alerting system, or funded-account certification.

## Design principle

Autonomy is bounded autonomy. The system may rank eligible venues and emit deterministic orders within explicit constraints. It must not bypass risk gates, assume an unknown broker response is a fill, trade through an unresolved reconciliation mismatch, or silently continue after an integrity failure. The design is influenced by general systematic research discipline and does not reproduce proprietary strategies, source code, or methods associated with any individual or firm.
