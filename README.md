# QuantFX Quantitative Execution & Microstructure Engine

QuantFX is an ultra-low-latency, hybrid quantitative trading engine built with a bare-metal NASM x86-64 assembly core, a deterministic C++20 maintainability layer, and a zero-overhead Python C-ABI bridge. Distributed under the MIT License, it provides nanosecond tick-to-trade execution primitives, lock-free inter-thread messaging, vectorized venue scoring, multi-level order flow imbalance (OFI), adverse selection queue simulation, and dual-feed multicast arbitration.

---

## Architecture & Subsystems

```text
+---------------------------------------------------------------------------------------+
|                                    QUANTFX ENGINE                                     |
+---------------------------------------------------------------------------------------+
|  Python C-ABI Layer (runtime/qfx_native.py via ctypes)                                 |
|  - Strategy orchestration, VaR analytics, risk governance, audit journal               |
+---------------------------------------------------------------------------------------+
|  C++20 Zero-Allocation Execution Layer (src/qfx_engine.cpp, include/*.hpp)            |
|  - SPSC Lock-Free Ring Buffer (19.7M ops/sec, cacheline-aligned)                       |
|  - L2 Order Book & VWAP Sweeper (p99 = 21ns)                                          |
|  - AVX2 Vectorized Venue Scoring (16.3 Billion ops/sec)                               |
|  - Simple Binary Encoding (SBE, 56-byte wire serializer)                              |
|  - Pre-Trade Risk Gateway (token-bucket rate limiter, fat-finger rejection)           |
|  - Memory-Mapped Binary Write-Ahead Log (WAL)                                         |
|  - Real-Time Streaming VaR (Value-at-Risk 99% parametric & historical)                |
|  - Nanosecond Latency Profiler & HDR Percentile Histogram                             |
|  - Institutional Performance Metrics (Sharpe, Sortino, Calmar, Profit Factor)         |
|  - Venue Simulator with FIFO Queue Priority & Markout Tracking                        |
|  - Multi-Level Order Flow Imbalance (OFI) & Queue Estimator                           |
|  - Multicast UDP Dual-Feed Line Arbitrator (A/B line deduplication & gap recovery)    |
|  - Kernel Sockets with TCP_NODELAY & SO_BUSY_POLL (50us socket polling)               |
+---------------------------------------------------------------------------------------+
|  Bare-Metal Assembly Core (src/qfx.asm, src/qfx_asm_core.asm, include/qfx.inc)       |
|  - Hand-tuned x86-64 NASM syscalls, rdtsc cycle calibration (<10ns)                  |
|  - Zero-heap allocation hot paths, fixed-point integer scaling                        |
|  - UEFI bootloader application stub & Windows NT syscall stubs                         |
+---------------------------------------------------------------------------------------+
```

---

## Build Targets

```bash
make clean
make
make check
```

Artifacts generated in `build/`:
- `build/qfx`: Pure NASM x86-64 assembly executable for Linux ELF64.
- `build/qfx_hybrid`: C++20 / Assembly hybrid execution engine.
- `build/libqfx.so`: High-performance C-ABI shared library for Python integration.
- `build/qfx.efi`: PE32+ UEFI application stub.
- `build/qfx-nt-syscalls.obj`: Windows x86-64 COFF object with direct NT syscall wrappers.

---

## Hybrid Engine Commands (`build/qfx_hybrid`)

| Command | Purpose | Verification Output |
|---|---|---|
| `ring-benchmark [N]` | Benchmark SPSC lock-free ring buffer throughput | `ring_iterations=N`, `ring_ops_per_sec` (~19.7M/s) |
| `alpha-backtest <csv>` | Run microstructure alpha on canonical tick data | `events=N`, `trades=N`, `net_pnl`, `sharpe` |
| `l2-sweep <side> <qty>` | Nanosecond L2 order book sweep and VWAP execution | `target_qty=N`, `filled_qty=N`, `vwap=N` |
| `tsc-benchmark [N]` | Hardware TSC cycle latency benchmark | `tsc_iterations=N`, `tsc_latency_ns` (<10ns) |
| `sbe-demo` | Simple Binary Encoding 56-byte roundtrip | `sbe_verify=PASS` |
| `risk-demo` | Pre-trade risk gateway (notional, fat-finger, rate limit) | `risk_gateway=PASS` |
| `sor-demo` | Smart Order Router multi-venue optimal split | `sor_verify=PASS` |
| `simd-benchmark [N]` | AVX2 4-lane SIMD venue scoring benchmark | `simd_verify=PASS` (~16.3 Billion ops/sec) |
| `wal-demo` | Memory-mapped binary write-ahead log recovery | `wal_verify=PASS` |
| `var-demo` | Streaming 99% VaR and CVaR parametric computation | `var_verify=PASS` |
| `net-demo` | Non-blocking loopback socket with TCP_NODELAY & SO_BUSY_POLL | `net_socket=PASS` |
| `latency-profile [N]` | Nanosecond HDR histogram profiler to `results/latency_profile.json` | `profile_verify=PASS` |
| `venue-sim-demo` | Mock exchange matching engine with SBE execution reports | `venue_sim_verify=PASS` |
| `ofi-demo` | Multi-level Order Flow Imbalance (OFI) calculation | `ofi_verify=PASS` |
| `adverse-sim-demo` | Passive limit order FIFO queue position & adverse selection markout | `adverse_sim_verify=PASS` |
| `feed-arb-demo` | Multicast UDP A/B line sequence arbitration & deduplication | `feed_arb_verify=PASS` |

---

## Assembly Engine Commands (`build/qfx`)

| Command | Purpose |
|---|---|
| `qfx validate path.csv` | Validate and count valid/invalid canonical records |
| `qfx ingest path.csv output.qfx` | Normalize canonical CSV into native fixed-width QFX binary records |
| `qfx backtest path.csv` | Run built-in deterministic threshold strategy on CSV |
| `qfx backtest path.qfx` | Run deterministic strategy on binary QFX records |
| `qfx replay path.csv` | Deterministic event replay |
| `qfx route quotes.csv buy\|sell` | Select lowest-cost eligible venue quote from CSV |
| `qfx live market_source broker_sink` | Process live canonical market stream and emit order intents |

---

## Micro-Benchmarks & Verified Latency

Hardware TSC calibrated percentiles from `results/latency_profile.json`:

```json
{
  "iterations": 1000,
  "risk_eval_ns": {"p50": 30, "p90": 251, "p99": 2025, "max": 51494},
  "l2_sweep_ns": {"p50": 10, "p90": 20, "p99": 21, "max": 40},
  "avx2_score_ns": {"p50": 10, "p90": 20, "p99": 21, "max": 30}
}
```

- **L2 Order Book Sweep (p99)**: 21 ns
- **AVX2 Vectorized Scoring (p99)**: 21 ns
- **Pre-Trade Risk Gateway (p50)**: 30 ns
- **SPSC Ring Buffer Throughput**: 19,700,000 ops / second
- **AVX2 Vectorized Scoring Throughput**: 16,300,000,000 venue-evals / second

---

## Python C-ABI Native Bridge (`runtime/qfx_native.py`)

Python programs interact with the high-performance C++20 and Assembly core via ctypes:

```python
from runtime.qfx_native import (
    QfxNative,
    NativeOrder,
    NativeEvent,
    NativeVenueQuote,
    NativeSbeOrderMessage,
    NativeSbeHeader,
)

native = QfxNative()

ts = native.now_ns()

vwap, filled = native.sweep_l2(side=1, quantity=1500000)

l1_ofi, multi_ofi = native.update_ofi(
    bids=[108000, 107995],
    bid_sizes=[900000, 1000000],
    asks=[108010, 108015],
    ask_sizes=[200000, 1000000],
)

slot = native.venue_place_passive(cl_id=101, side=1, price=107995, qty=100000)

rep = native.venue_trade(side=-1, price=107995, qty=2500000, now_ns=ts)

markout_pnl = native.venue_markout(slot=slot, mid_price=107980)

action, next_seq = native.feed_arbitrate(feed_id=0, seq=10)
```

---

## Canonical Data & Wire Protocols

### Canonical CSV
Schema: `timestamp_ns,symbol,bid,ask,last,volume,bid_size,ask_size,sequence,flags`
Prices are fixed-point integers at scale 100,000 (1.08000 = 108000).

### SBE Order Wire Format (56 bytes)
- `block_length`: uint16 (56)
- `template_id`: uint16 (101)
- `schema_id`: uint16 (1)
- `version`: uint16 (1)
- `timestamp_ns`: uint64
- `client_order_id`: uint64
- `symbol`: uint64
- `price`: int64
- `quantity`: uint64
- `side`: int8
- `time_in_force`: uint8
- `padding`: uint16
- `reserved`: uint32

### SBE Execution Report (56 bytes)
- `block_length`: uint16 (56)
- `template_id`: uint16 (102)
- `schema_id`: uint16 (1)
- `version`: uint16 (1)
- `timestamp_ns`: uint64
- `client_order_id`: uint64
- `exec_id`: uint64
- `exec_price`: int64
- `exec_quantity`: uint64
- `exec_type`: uint8 (1=Partial, 2=Filled, 8=Rejected)
- `side`: int8
- `reserved`: uint16
- `padding`: uint32

---

## Operational Risk & Security

- **Kill Switch** (`runtime/kill_switch.py`): Atomic, mtime-cached status check with HMAC-SHA256 authorization, sub-millisecond trip latency.
- **Audit Journal** (`runtime/audit_journal.py`): Encrypted append-only log using AES-256-GCM authenticated cipher with SHA-256 chained record hashes and persistent fsync.
- **Binary WAL** (`include/qfx_wal.hpp`): Fixed 80-byte memory-mapped write-ahead journal for zero-allocation state persistence.
- **Streaming VaR** (`include/qfx_risk_var.hpp`): Running 500-sample 99% parametric & empirical Value-at-Risk computation.

---

## Repository Map

| Path | Purpose |
|---|---|
| `src/qfx.asm` | Pure Assembly executable and baseline engine subsystems |
| `src/qfx_asm_core.asm` | Assembly hot-path risk checking routines |
| `src/qfx_engine.cpp` | C++20 hybrid execution engine CLI and C-ABI export interface |
| `include/qfx.hpp` | Shared binary structs, constants, and static asserts |
| `include/qfx_ring.hpp` | SPSC lock-free shared-memory ring buffer (19.7 MOPS) |
| `include/qfx_l2.hpp` | L2 order book array and nanosecond VWAP sweeper (21ns p99) |
| `include/qfx_tsc.hpp` | Hardware `rdtsc` cycle timer with sub-10ns tick resolution |
| `include/qfx_sbe.hpp` | Simple Binary Encoding (SBE) 56-byte wire serializer |
| `include/qfx_simd.hpp` | AVX2 vectorized 4-venue scoring (16.3 BOPS) |
| `include/qfx_risk_gateway.hpp` | Pre-trade risk validation and token-bucket rate limiter |
| `include/qfx_sor.hpp` | Smart Order Router multi-venue clip allocator |
| `include/qfx_wal.hpp` | Memory-mapped binary write-ahead log (WAL) |
| `include/qfx_risk_var.hpp` | Streaming 99% Value-at-Risk (VaR) and CVaR estimator |
| `include/qfx_net.hpp` | Ultra-low-latency non-blocking TCP/UDP sockets with `SO_BUSY_POLL` |
| `include/qfx_hdr_histogram.hpp` | Nanosecond latency profiler tracking p50, p90, p99, p99.9, max |
| `include/qfx_quant_metrics.hpp` | Institutional quant metrics (Sharpe, Sortino, Calmar, Profit Factor) |
| `include/qfx_venue_sim.hpp` | Mock exchange matching engine with FIFO queue tracking & markout |
| `include/qfx_ofi.hpp` | Multi-level Order Flow Imbalance (OFI) & queue position estimator |
| `include/qfx_feed_arbitration.hpp` | Multicast UDP dual-feed Line A / Line B sequence arbitrator |
| `runtime/qfx_native.py` | Python C-ABI ctypes bridge to shared library `libqfx.so` |
| `runtime/audit_journal.py` | AES-256-GCM encrypted audit journal with cryptographic hash chains |
| `runtime/kill_switch.py` | Sub-millisecond fail-closed atomic kill switch |
| `tests/run_tests.sh` | Assembly executable unit and regression tests |
| `tests/platform_checks.sh` | Cross-platform EFI and COFF binary validation tests |
| `tests/runtime_checks.py` | Python runtime audit, kill switch, and walk-forward verification |
| `tests/kill_switch_checks.sh` | Shell verification of fail-closed kill switch behavior |
| `tests/hybrid_checks.sh` | Full hybrid suite testing all 17 low-latency commands |
| `tests/test_native.py` | Python ctypes C-ABI bridge test suite |
