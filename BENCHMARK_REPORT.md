# QuantFX Real-Data Benchmark Report


**Benchmark scope:** real historical FX ticks, deterministic modeled multi-venue execution cost, encrypted audit recovery, safe kill-switch verification, and purged walk-forward optimization.

## Executive summary

This benchmark exercised QuantFX on **9,559,151 real historical tick records** covering AUDUSD, EURUSD, GBPUSD, and USDJPY for January 2024. The records came from HistData generic ASCII tick archives and were converted to Zstandard-compressed Parquet. HistData describes the generic rows as time-ordered `DateTime,Bid,Ask,Volume` data and documents the timestamps as Eastern Standard Time without daylight-saving adjustment [1]. QuantFX converted those timestamps using a fixed UTC−05:00 interpretation.

The shortfall experiment is deliberately reported as a **deterministic modeled venue-cost simulation**, not as historical multi-broker market data. One real provider feed was used as the reference quote stream; broker-specific latency, slippage, fee, and rejection assumptions were applied synthetically. Under those assumptions, BROKER_C had the lowest candidate cost and received 99.9747% of routed ticks. This result demonstrates the routing calculation and its sensitivity to configured venue attributes, but it is not evidence that the named or simulated venue is superior in live trading.

The purged walk-forward run produced 20 folds, consisting of five folds for each pair. The simple threshold strategy’s mean test PnL was normalized to pips and averaged 1.67–3.34 pips per test bar depending on pair. The test-to-train normalized PnL decay averaged approximately 70.9%–72.5%, indicating substantial deterioration from in-sample scores in this experiment. This is a diagnostic of the specified toy strategy and sample, not evidence of tradable alpha or a forecast of future performance.

The encrypted audit journal recovered all three appended events and detected ciphertext tampering. The runtime kill switch passed missing, clear, trip, invalid-signature, and stale-state checks. The bare-metal assembly object was assembled and symbol-inspected, and a user-space NASM harness verified only the non-privileged simulation latch. No privileged interrupt handler, PIC port I/O, `cli`, `iretq`, physical hardware switch, firmware boot, QEMU execution, or native non-Linux target was exercised.

## Data provenance and integrity

The source archives were downloaded from HistData’s January 2024 month pages. The provider’s generic tick-data selection page and FAQ are retained in the repository provenance note [2]. Source hashes are listed below; the retained Parquet manifest at `results/parquet_manifest.json` records the corresponding record counts and output paths.

| Pair | Archive | Records | Source SHA-256 |
|---|---|---:|---|
| AUDUSD | `HISTDATA_COM_ASCII_AUDUSD_T_202401.zip` | 1,477,644 | `ca6aefdea014475db9407fe88d8421e0c91d5500685d71b96973aef86ccd0812` |
| EURUSD | `HISTDATA_COM_ASCII_EURUSD_T_202401.zip` | 2,176,113 | `9a4ae15d87abff93311cc60adf7fe83de9078664c6baaf2984fcc70f160b3da9` |
| GBPUSD | `HISTDATA_COM_ASCII_GBPUSD_T_202401.zip` | 2,146,970 | `9d6ad5449985d0bcc7e21b1a1c1ed136208b91329d6cf5b975f3ec87bace566d` |
| USDJPY | `HISTDATA_COM_ASCII_USDJPY_T_202401.zip` | 3,758,424 | `2266c00451e5c8089dfa46bb35b31bea98578ebfca9697bae4817bcc8219b3ef` |
| **Total** | 4 archives | **9,559,151** | See manifest and provenance note |

HistData states that its free data has no warranty or certification and reflects provider or broker-specific quote characteristics [1]. Accordingly, these feeds are treated as one historical reference stream rather than a consolidated institutional FX tape. No synthetic price path was used for the tick inputs.

## Methodology

The conversion pipeline in `research/histdata_to_parquet.py` streams the downloaded archives into Parquet, preserves bid, ask, volume, sequence, and nanosecond timestamp fields, and applies the fixed provider timezone convention. The benchmark scripts consume the resulting Parquet files directly. The retained provenance manifest is `results/parquet_manifest.json`.

The execution test in `research/real_shortfall_benchmark.py` evaluates every tick using a 100,000-unit order. The side alternates deterministically from the tick sequence parity. Each candidate venue applies the following modeled assumptions:

| Venue | Latency assumption | Slippage assumption | Fee assumption | Rejection assumption |
|---|---:|---:|---:|---:|
| BROKER_A | 55 µs | 0.000010 price units | 0.000005 per unit | 700 ppm |
| BROKER_B | 95 µs | 0.000008 price units | 0.000004 per unit | 900 ppm |
| BROKER_C | 180 µs | 0.000004 price units | 0.000006 per unit | 250 ppm |

The router minimizes modeled shortfall plus a latency penalty. The reported candidate cost is the average modeled whole-order cost divided by 100,000 units; it is therefore a price-unit-per-unit measure converted to pips per unit. It is not an observed implementation-shortfall measurement from broker execution reports.

The walk-forward module resamples each feed to five-minute bars. Each fold uses 500 training bars, a five-bar purge gap, 150 test bars, and a five-bar embargo. For each training window, it selects the best threshold from the 10th, 25th, 50th, and 75th percentiles of absolute bar changes. The score is the sum of threshold-triggered signed changes. Results include raw price-unit PnL, pip-normalized PnL, pip PnL per bar, and decay defined as `1 - test_pnl_pips / train_pnl_pips`. This simple score does not model spread, financing, position carry, margin, market impact, or portfolio risk.

## Execution-cost results

The final candidate costs are below. They are deterministic modeled values generated from the real reference ticks and the synthetic venue assumptions above.

| Pair | BROKER_A (pips/unit) | BROKER_B (pips/unit) | BROKER_C (pips/unit) |
|---|---:|---:|---:|
| AUDUSD | 0.7028 | 0.6728 | 0.6528 |
| EURUSD | 0.2980 | 0.2680 | 0.2480 |
| GBPUSD | 0.6169 | 0.5869 | 0.5669 |
| USDJPY | 0.3460 | 0.3457 | 0.3455 |
| **Mean** | **0.4909** | **0.4683** | **0.4533** |

BROKER_C was selected for 99.9747% of all routed ticks; BROKER_A and BROKER_B each received 0% in the aggregate output. This concentration is expected from the configured cost and rejection curves. It also exposes a model-design issue for production routing: a live implementation would need observed quote quality, venue availability, fill probability, queue position, exposure limits, and diversification constraints rather than relying on static coefficients.

The reference graph is available at [`figures/shortfall_by_broker.png`](figures/shortfall_by_broker.png), and the allocation graph is available at [`figures/routing_share.png`](figures/routing_share.png). Both titles explicitly identify the outputs as modeled rather than observed broker measurements.

## Walk-forward results

All PnL values in this table are normalized to pips. PnL-per-bar makes the four pairs more comparable than raw price-unit PnL, while still not making the strategy economically complete.

| Pair | Folds | Mean test PnL (pips) | Mean test PnL (pips/bar) | Mean decay | Median decay |
|---|---:|---:|---:|---:|---:|
| AUDUSD | 5 | 250.59 | 1.6706 | 71.10% | 73.79% |
| EURUSD | 5 | 252.72 | 1.6848 | 71.07% | 69.92% |
| GBPUSD | 5 | 317.93 | 2.1195 | 72.50% | 74.20% |
| USDJPY | 5 | 501.19 | 3.3413 | 70.93% | 69.62% |
| **Total** | **20** | — | — | — | — |

The alpha-decay graph is available at [`figures/oos_alpha_decay.png`](figures/oos_alpha_decay.png). The normalized outputs correct the prior cross-pair comparability issue caused by USDJPY’s different raw price scale. They do not correct for the strategy’s other simplifications or establish profitability after realistic trading costs.

## Security and recovery inspection

The benchmark-specific security run is stored in `results/audit_kill_switch.json`. It appended three encrypted journal events, reopened the journal, verified a three-record chain, and detected a one-bit ciphertext modification as an integrity failure. The journal uses AES-GCM authenticated encryption and a SHA-256 chain linkage between records.

| Check | Result |
|---|---|
| Encrypted journal append count | 3 |
| Recovered record count | 3 |
| Chain verification | PASS |
| Tampered ciphertext detected | PASS |
| Missing kill-switch state fails closed | PASS |
| Signed clear state permits trading | PASS |
| Signed trip state blocks trading | PASS |
| Invalid signature blocks trading | PASS |
| Stale state blocks trading | PASS |

The platform object `build/qfx-kill-switch.o` was assembled as an x86-64 ELF relocatable and contained the expected symbols `qfx_kill_irq_handler`, `qfx_kill_switch_trip`, `qfx_kill_switch_active`, and `qfx_kill_switch_simulate`. The pure-NASM harness called only `qfx_kill_switch_active` and `qfx_kill_switch_simulate`, producing `SIMULATED_LATCH_PASS`. The privileged trip routine and IRQ handler were not called. In particular, this run is **not hardware interrupt validation**: Linux user space cannot safely exercise the handler’s `cli`, PIC port output, or `iretq` path without a bare-metal or virtual-machine supervisor context.

## Validation and reproducibility

The following checks completed successfully after the benchmark changes:

| Validation | Outcome |
|---|---|
| Python syntax compilation for benchmark and ingestion scripts | PASS |
| Native Linux build and regression tests via `make check` | PASS |
| Platform artifact checks | PASS |
| Runtime durability, journal, and file kill-switch checks | PASS |
| Assembly kill-switch static symbol checks | PASS |
| Safe user-space latch simulation | PASS |
| QEMU BIOS firmware smoke test | `QEMU_SKIPPED` because QEMU is unavailable on this host |

The principal commands were:

```text
python3 research/real_shortfall_benchmark.py <PARQUET_DIR>/*.parquet --quantity 100000 --sample-step 1000 --output results/shortfall.json
python3 research/parquet_walk_forward.py <PARQUET_DIR>/*.parquet --train-bars 500 --test-bars 150 --purge-bars 5 --embargo-bars 5 --folds 5 --bar-interval 5min --output results/walk_forward.json
python3 research/make_benchmark_visuals.py --shortfall results/shortfall.json --walk-forward results/walk_forward.json --figures figures --results results
python3 research/benchmark_security_checks.py
make check
make qemu-firmware
```

The shortfall pass processed the four files in approximately 34.86 seconds in total, with measured per-file throughput between approximately 268,537 and 276,732 ticks per second and mean loop processing time between approximately 3.61 and 3.72 microseconds per tick in the Python benchmark process. These are host-process measurements, not a claim about end-to-end ultra-low-latency order execution. Network transport, kernel scheduling, exchange acknowledgement, serialization, broker gateways, and hardware timestamping were not measured.

Machine-readable outputs are in `results/`, provenance is in `benchmark_sources.md`, and visual QA notes are in `results/visual_inspection.md`. This clean package includes scripts, manifests, logs, results, figures, and this report but does not include the large raw archives or Parquet feeds. The hashes and retained conversion manifest allow the data inputs to be independently reacquired and checked against the recorded source artifacts.

## Limitations and interpretation

The most important limitation is that the benchmark does not contain historical multi-broker quote or execution data. The ticks are real, but the broker latency, slippage, fee, and rejection curves are deterministic assumptions. The shortfall and routing findings must therefore be interpreted as a controlled model test, not as a market-venue ranking.

The walk-forward strategy is intentionally small and deterministic. It optimizes a handful of thresholds on one month of data, evaluates only 20 folds, and does not include transaction costs or a complete position-accounting model. A positive test PnL in this score is not evidence of a tradable strategy. The reported decay is useful for software-path and research-pipeline validation, not for investment decisions.

The bare-metal kill-switch assembly is statically checked and its safe simulation entry point is exercised, but no physical switch, interrupt controller, firmware environment, QEMU guest, or production privilege boundary was available. Native Windows, macOS, and Apple Silicon execution also remains outside this Linux-host benchmark. No claim of production readiness, actual broker connectivity, or future returns is made.

## References

[1] [HistData FAQ](https://www.histdata.com/f-a-q/), including generic tick-file layout, timestamp convention, and data-quality disclaimer.  
[2] [HistData generic tick-data selection](https://www.histdata.com/download-free-forex-data/?/ascii/tick-data-quotes), source selection page used for the benchmark inputs.  
[3] [HistData EURUSD January 2024 tick page](https://www.histdata.com/download-free-forex-historical-data/?/ascii/tick-data-quotes/eurusd/2024/1), representative month-level source page and archive naming convention.
