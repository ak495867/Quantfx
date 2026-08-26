# QuantFX Engine

QuantFX is a Linux x86-64 foreign-exchange quantitative engine written in NASM assembly and distributed under the MIT License. It provides deterministic historical validation, CSV-to-QFX normalization, native QFX replay, bid/ask-aware backtesting, a live adapter boundary, fixed-point arithmetic, position and spread limits, drawdown halting, stdin/stdout streaming, and broker-order serialization.

## Build

```text
make
```

The executable is `build/qfx` on the validated Linux target. The current native entry point uses the Linux System V AMD64 ABI and links as an ELF64 binary. Source code contains no comments. Cross-platform target status and bare-metal constraints are defined in `PLATFORM_PORTABILITY.md`.

## Commands

| Command | Purpose |
|---|---|
| `qfx validate path.csv` | Parse and count valid and invalid canonical records |
| `qfx ingest path.csv output.qfx` | Normalize canonical CSV into native fixed-width QFX records |
| `qfx backtest path.csv` | Run the built-in deterministic threshold strategy and print metrics |
| `qfx backtest path.qfx` | Run the same strategy on native records |
| `qfx replay path.csv` | Execute deterministic replay with the same event semantics |
| `qfx route quotes.csv buy|sell` | Select the lowest-cost eligible broker quote |
| `qfx live market_source broker_sink` | Read a canonical market stream and emit accepted intents; use `-` for stdin or stdout |
| `make qemu-firmware` | Run the BIOS firmware smoke test when QEMU is available |
| `python3 tests/runtime_checks.py` | Verify order state, encrypted audit, kill switch, walk-forward, and shortfall modules |

## Routing quote CSV

The route command accepts `timestamp_ns,symbol,bid,ask,latency_us,slippage,fee,reject_ppm,age_ns,capacity`. It excludes stale or undersized venues and scores the remaining quotes using spread, latency, slippage, fees, reject rate, and age. The result reports the selected broker, score, and side-aware execution price.

## Canonical CSV

The input schema is `timestamp_ns,symbol,bid,ask,last,volume,bid_size,ask_size,sequence,flags`. Price fields are fixed-point integers at a scale of 100000. Timestamps, sizes, volume, sequences, and flags are raw integer values.

## Parquet

Parquet is supported through `adapters/parquet_bridge.py`, which maps common timestamp, bid, ask, last, volume, size, sequence, and flag column names into canonical CSV. Install the optional `pyarrow` dependency, run the bridge, and pipe its output into a file or adapter process. The assembly engine remains independent of any single Parquet implementation.

```text
python3 adapters/parquet_bridge.py data/input.parquet --symbol EURUSD > data/input.csv
./build/qfx validate data/input.csv
```

## Live adapter contract

The market adapter emits canonical records. The broker sink receives `SIDE,client_order_id,quantity,price,timestamp_ns,sequence`. Market input and broker output accept `-` for stdin and stdout. Adapter processes are responsible for vendor credentials, authentication, retries, heartbeats, order acknowledgements, broker reconciliation, and vendor-specific error mapping. `adapters/broker_simulator.sh` provides deterministic ACK and FILL responses for offline testing. See `spec/adapter_protocol.md`.

## Risk defaults

The built-in execution path uses a 1,000-unit order, a 1,000,000-unit absolute position limit, a 500-unit spread limit, a 20,000,000-unit drawdown halt threshold, and a 1,000-unit minimum cash reserve. These values are intentionally visible in the executable source and should be replaced by configuration-backed initialization before a production deployment.

## Deployment

`deploy/qfx.service` provides a systemd template for a persistent Linux host. The service should run under a dedicated unprivileged account, use a private environment file for credentials, write journals to a durable filesystem, and start in paper mode until the broker adapter and reconciliation process have been verified.

## Portability status

The validated build currently targets Linux x86-64. The platform-service ABI is defined in `platform/qfx_platform_abi.inc`, but macOS, Windows, and bare-metal support require their target-specific assembler format, linker, entry point, platform service implementation, SDK or board support package, and runtime verification. `PLATFORM_PORTABILITY.md` records the exact boundary and does not mark unvalidated targets as complete.

## Completeness

This repository is a working engine and integration framework, not a claim of funded-account production readiness. Broker-specific certification, native target validation, account reconciliation, and hardware or firmware testing remain deployment requirements.

## Repository map

| Path | Purpose |
|---|---|
| `src/qfx.asm` | Assembly executable and engine subsystems |
| `include/qfx.inc` | Shared constants and record layouts |
| `platform/` | Platform-service ABI for hosted and freestanding backends |
| `adapters/` | Optional data, vendor bridges, and broker simulator |
| `runtime/` | Durable order state, encrypted audit journal, and kill switch |
| `research/` | Purged walk-forward and implementation-shortfall models |
| `ci/` | QEMU firmware runner and target validation |
| `PLATFORM_PORTABILITY.md` | Target matrix and portability boundaries |
| `PRODUCTION_CONTRACT.md` | Generic production safety contract |
| `spec/` | Binary and wire-format specifications |
| `fixtures/` | Deterministic test data |
| `tests/` | Regression and integration checks |
| `config/` | Example risk and runtime configuration |
| `deploy/` | Service and operational units |
| `docs/` | Architecture and usage documentation |
