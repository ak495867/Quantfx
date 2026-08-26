# QuantFX Usage Guide

## Purpose and operating model

QuantFX is a low-level foreign-exchange research and execution framework. The native executable is written in NASM x86-64 assembly and is designed around deterministic market-data validation, fixed-point QFX records, replay, backtesting, quote routing, and a live adapter boundary. Python modules provide optional Parquet conversion, research benchmarks, durable order state, encrypted audit journaling, and a signed file-based kill switch.

The validated hosted target is **Linux x86-64**. The repository also contains Windows PE, Darwin, UEFI, BIOS, timer, and interrupt-oriented platform sources, but those targets require their own toolchains, loaders, privilege boundaries, and target-specific verification. Do not interpret the presence of a platform source file as proof of native production validation.

QuantFX is an engineering and research framework, not a broker, investment adviser, or guarantee of trading performance. Begin with fixtures and paper trading. Do not connect credentials or route real orders until the adapter, reconciliation, risk limits, operational monitoring, and venue-specific behavior have been independently verified.

## Requirements

For the validated Linux build, install NASM, GNU `ld`, GNU Make, and Python 3. Python is not required to build the native executable, but it is required for runtime checks, Parquet conversion, research scripts, encrypted journaling, and the kill switch. The Parquet bridge and real-data benchmark require `pyarrow`; the research scripts also use `numpy`, `pandas`, and `matplotlib`.

| Component | Required for | Typical command or package |
|---|---|---|
| NASM | Native Linux and platform assembly | `nasm` |
| GNU `ld` | Native Linux linking | `ld` |
| GNU Make | Build and test targets | `make` |
| Python 3 | Runtime and research modules | `python3` |
| PyArrow | Parquet bridge and Parquet research feeds | `python3 -m pip install pyarrow` |
| NumPy and pandas | Walk-forward analysis | `python3 -m pip install numpy pandas` |
| Matplotlib | Benchmark graphs | `python3 -m pip install matplotlib` |
| `cryptography` | AES-GCM audit journal | `python3 -m pip install cryptography` |

Use the package manager appropriate to the host. The commands below assume the required executables are available on `PATH`.

## Build the native engine

From the repository root, run:

```text
make
```

The resulting executable is `build/qfx`. The default Makefile uses NASM to assemble `src/qfx.asm` and GNU `ld` to link the ELF64 Linux executable. Remove generated build output with:

```text
make clean
```

Build and inspect the non-Linux and freestanding artifacts with:

```text
make platform
```

This creates target-specific objects and images under `build/` where the host toolchain supports them. The platform sources are portability artifacts and interfaces; native execution on Windows, macOS, Apple Silicon, UEFI, BIOS, or a physical interrupt controller must be validated on the corresponding target.

## Run the automated checks

Run the complete local validation suite with:

```text
make check
```

The suite covers the native assembly engine, CSV and QFX fixtures, deterministic backtesting, route selection, live FIFO and standard-stream behavior, platform artifact checks, durable runtime state, encrypted audit journaling, file kill-switch behavior, and the safe assembly kill-switch simulation. The BIOS smoke test is separate:

```text
make qemu-firmware
```

If QEMU is not installed, the firmware target reports `QEMU_SKIPPED`. A skipped QEMU run is not firmware validation.

You can run the Python runtime checks directly with:

```text
python3 tests/runtime_checks.py
```

The repository’s source convention is comment-free implementation code. Markdown documentation, including this guide, may contain explanatory prose.

## Canonical CSV input

The native engine accepts canonical CSV records with the following ten fields:

```text
timestamp_ns,symbol,bid,ask,last,volume,bid_size,ask_size,sequence,flags
```

Prices are fixed-point integers at a scale of 100000. Timestamps, quantities, volume, sequence numbers, and flags are integer fields. The sample file `fixtures/eurusd_sample.csv` demonstrates the accepted format and includes valid and invalid rows for validation testing.

Validate a CSV file and display the record counts:

```text
./build/qfx validate fixtures/eurusd_sample.csv
./build/qfx validate path/to/market.csv
```

A typical validation result reports valid events and invalid lines. Invalid input should be investigated rather than silently forwarded to a live adapter.

## Convert CSV to native QFX

Normalize canonical CSV into the native fixed-width QFX format with:

```text
./build/qfx ingest fixtures/eurusd_sample.csv /tmp/eurusd_sample.qfx
```

Validate the resulting QFX file:

```text
./build/qfx validate /tmp/eurusd_sample.qfx
```

QFX is useful when a deterministic replay or backtest should consume the same serialized record structure used by the native engine. The binary layout and field definitions are specified in `spec/qfx_format.md` and `include/qfx.inc`.

## Run a deterministic backtest

Run the built-in deterministic threshold strategy against canonical CSV:

```text
./build/qfx backtest fixtures/eurusd_sample.csv
```

The same strategy can consume a native QFX file:

```text
./build/qfx backtest /tmp/eurusd_sample.qfx
```

The output includes event and fill counts and strategy metrics implemented by the native engine. This built-in strategy is a deterministic software-path test, not a validated investment strategy. For serious research, create a separate strategy specification, include realistic costs and financing, and use out-of-sample evaluation.

## Replay historical events

Replay canonical events through the deterministic event path with:

```text
./build/qfx replay fixtures/eurusd_sample.csv
```

Replay is intended to make event handling reproducible. Keep the input file, executable version, configuration, and output logs together when comparing runs.

## Route broker quotes

The route command accepts a quote CSV with these fields:

```text
timestamp_ns,symbol,bid,ask,latency_us,slippage,fee,reject_ppm,age_ns,capacity
```

Route a buy or sell request through the eligible quote set:

```text
./build/qfx route fixtures/broker_quotes.csv buy
./build/qfx route fixtures/broker_quotes.csv sell
```

The router rejects stale or undersized venues and scores remaining venues using spread, latency, slippage, fees, rejection rate, and quote age. It reports the selected broker, score, and side-aware execution price. The fixture is deterministic and is not a substitute for live quote normalization, broker authentication, capacity checks, reconciliation, or exchange-specific execution rules.

## Use the live adapter boundary

The live command reads canonical market records from a market source and writes accepted order intents to a broker sink:

```text
./build/qfx live market_source broker_sink
```

Use `-` for standard input or standard output. For example, stream a fixture through the engine and capture emitted orders:

```text
cat fixtures/eurusd_sample.csv | ./build/qfx live - -
```

For a FIFO-based local integration test:

```text
rm -f fixtures/orders.pipe /tmp/qfx_orders.out
mkfifo fixtures/orders.pipe
cat fixtures/orders.pipe > /tmp/qfx_orders.out &
./build/qfx live fixtures/eurusd_sample.csv fixtures/orders.pipe
cat /tmp/qfx_orders.out
rm -f fixtures/orders.pipe
```

The emitted broker-order format is:

```text
SIDE,client_order_id,quantity,price,timestamp_ns,sequence
```

The market adapter and broker sink are intentionally vendor-neutral. The adapter owner is responsible for credentials, authentication, TLS, reconnects, heartbeats, throttles, idempotency, order acknowledgements, execution reports, cancellations, account reconciliation, and vendor-specific error mapping. The native engine does not make those external guarantees automatically.

Use the deterministic broker simulator for offline testing:

```text
cat /tmp/qfx_orders.out | adapters/broker_simulator.sh
```

The simulator emits `ACK` and `FILL` records. It is a local test utility, not a brokerage connection.

## Parquet input and vendor data

The engine does not require a specific data vendor. Convert vendor Parquet files to canonical CSV with:

```text
python3 adapters/parquet_bridge.py data/input.parquet --symbol EURUSD > data/input.csv
./build/qfx validate data/input.csv
```

The bridge recognizes common column names for timestamps, bid, ask, last, volume, sizes, sequence, and flags. It requires timestamp, bid, ask, and last fields. Floating-point prices are scaled to the native 100000 price scale; already-integer values are passed through. Confirm the source units before using this path because an incorrectly scaled feed can produce invalid prices and risk calculations.

For large files, prefer a streaming or columnar preprocessing workflow and validate representative records before ingesting the complete dataset. Preserve the original source hash, timezone interpretation, schema mapping, and conversion command.

## Real-data research benchmark

The benchmark scripts in `research/` are separate from the native live path. A real-data run requires Parquet files staged locally. Convert downloaded tick archives with:

```text
python3 research/histdata_to_parquet.py data/real/*.zip --output data/real/parquet --manifest data/real/parquet/manifest.json
```

Run the deterministic modeled execution-cost benchmark:

```text
python3 research/real_shortfall_benchmark.py data/real/parquet/*.parquet --quantity 100000 --sample-step 1000 --output results/shortfall.json
```

Run the purged walk-forward threshold study:

```text
python3 research/parquet_walk_forward.py data/real/parquet/*.parquet --train-bars 500 --test-bars 150 --purge-bars 5 --embargo-bars 5 --folds 5 --bar-interval 5min --output results/walk_forward.json
```

Generate CSV summaries and figures:

```text
python3 research/make_benchmark_visuals.py --shortfall results/shortfall.json --walk-forward results/walk_forward.json --figures figures --results results
```

The retained benchmark report and result files explain the exact data provenance, source hashes, pair-specific pip normalization, synthetic venue assumptions, audit recovery results, and limitations. The shortfall benchmark uses real historical reference ticks but deterministic modeled broker attributes; it is not historical multi-broker execution data. The walk-forward output is a research diagnostic and is not evidence of future returns.

## Durable order state

The durable order store is available as a Python module:

```text
python3 -c 'from runtime.order_state import DurableOrderStore; store = DurableOrderStore("data/order_state.jsonl"); print(store.create("demo-1", 1000, "BROKER_TEST")); print(store.transition("demo-1", "ACKED"))'
```

The command-line inspection form prints the restored records from an existing journal:

```text
python3 runtime/order_state.py data/order_state.jsonl
```

The store persists append-only events, verifies a hash chain during restore, enforces legal state transitions, and halts on chain or reconciliation errors. A normal order lifecycle begins with creation and proceeds through states such as `SUBMITTED`, `ACKED`, `PARTIALLY_FILLED`, and `FILLED`, or a terminal rejection, cancellation, expiration, or unknown state. Integrate broker snapshots and reconciliation before treating an order as safely managed.

## Encrypted audit journal

Create a 32-byte AES key with restricted permissions:

```text
mkdir -p data
python3 runtime/audit_journal.py init-key data/audit.key
```

Append an authenticated encrypted event:

```text
python3 runtime/audit_journal.py append data/audit.jsonl data/audit.key '{"event":"order_submitted","order_id":"demo-1","quantity":1000}'
```

Verify and recover the journal chain:

```text
python3 runtime/audit_journal.py verify data/audit.jsonl data/audit.key
```

The journal uses AES-GCM for authenticated encryption and chained record linkage. Protect the key separately from the journal. Back up keys and journals under an operational retention policy; encryption does not replace access control, key rotation, secure backups, or incident response.

## Signed fail-closed kill switch

Create a key with at least 32 bytes and restrict its permissions:

```text
mkdir -p data
head -c 32 /dev/urandom > data/kill-switch.key
chmod 600 data/kill-switch.key
```

Clear the switch for a controlled paper-trading test:

```text
python3 runtime/kill_switch.py data/kill-switch.json data/kill-switch.key clear paper-test
python3 runtime/kill_switch.py data/kill-switch.json data/kill-switch.key status
python3 runtime/kill_switch.py data/kill-switch.json data/kill-switch.key can-trade
```

Trip the switch immediately:

```text
python3 runtime/kill_switch.py data/kill-switch.json data/kill-switch.key trip operator-stop
python3 runtime/kill_switch.py data/kill-switch.json data/kill-switch.key status
```

The runtime blocks trading when the switch file is missing, stale, malformed, or has an invalid signature. The assembly platform layer additionally contains a privileged bare-metal trip path and a safe unprivileged simulation entry point. Never call the privileged `cli`, port-I/O, or interrupt-return path from ordinary Linux user space. Hardware validation requires a controlled bare-metal or virtual-machine test environment with an approved interrupt and recovery procedure.

## Configuration and deployment

Review `config/default.toml` and `config/qfx.env.example` before deployment. Keep credentials in an external protected environment file and never commit them. `deploy/qfx.service` provides a systemd template for a persistent Linux host. Run under a dedicated unprivileged account, use durable storage for state and audit records, start in paper mode, and make the kill switch observable to the operations team.

A production deployment should add venue-specific authentication, connection supervision, reconciliation, margin and financing models, clock monitoring, alerting, secrets management, rate-limit handling, restart drills, backup restoration drills, and independent risk controls. The presence of a systemd unit does not by itself make a trading deployment production-ready.

## Clean-up and reproducible runs

Remove native build output after local validation with:

```text
make clean
```

Remove Python caches if they were generated outside the ignored paths:

```text
find . -type d -name __pycache__ -prune -exec rm -rf {} +
```

For every research run, record the input manifest, source hashes, timezone, command-line arguments, software versions, output hashes, and validation status. Keep raw data outside the clean source package when distributing code. The benchmark package’s `results/reproducibility.json` and `results/reproducibility.log` show the intended record format.

## Repository map

| Path | Use |
|---|---|
| `src/qfx.asm` | Native Linux x86-64 engine |
| `include/qfx.inc` | Shared native constants and layouts |
| `platform/` | Windows, Darwin, firmware, timer, and kill-switch sources |
| `adapters/` | Parquet bridge and deterministic broker simulator |
| `runtime/` | Durable order state, encrypted audit, and file kill switch |
| `research/` | Walk-forward, shortfall, data conversion, and visualization scripts |
| `tests/` | Native, platform, runtime, and safe kill-switch checks |
| `spec/` | QFX binary and adapter protocol specifications |
| `config/` | Example runtime configuration |
| `deploy/` | Linux service template |
| `fixtures/` | Deterministic CSV, QFX, and broker quote inputs |
| `docs/architecture.md` | Architecture reference |
| `docs/usage.md` | This usage guide |
| `results/` | Retained benchmark results and reproducibility records |
| `figures/` | Retained benchmark visualizations |

## Safety and interpretation

Use QuantFX first as a deterministic research and integration system. Validate all prices, timestamps, units, quantities, and venue responses at each boundary. Do not infer execution quality from a backtest that omits spread, market impact, financing, latency variation, rejected orders, partial fills, or account constraints. Do not infer future performance from the included benchmark results. Real-money deployment requires target-specific testing and operational approval beyond the commands documented here.
