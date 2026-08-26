from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

import pyarrow.parquet as pq


BROKERS = {
    "BROKER_A": {"latency_us": 55.0, "slippage": 0.000010, "fee": 0.000005, "reject_ppm": 700},
    "BROKER_B": {"latency_us": 95.0, "slippage": 0.000008, "fee": 0.000004, "reject_ppm": 900},
    "BROKER_C": {"latency_us": 180.0, "slippage": 0.000004, "fee": 0.000006, "reject_ppm": 250},
}


def rejected(sequence: int, reject_ppm: int) -> bool:
    value = (sequence * 1103515245 + 12345) & 0x7FFFFFFF
    return value % 1_000_000 < reject_ppm


def run_file(path: Path, quantity: int, sample_step: int) -> dict:
    pair = path.stem.split("_")[3]
    stats = {name: {"orders": 0, "accepted": 0, "shortfall": 0.0, "candidate_count": 0, "candidate_shortfall": 0.0, "shortfalls": [], "latency_us": spec["latency_us"]} for name, spec in BROKERS.items()}
    routed = {name: 0 for name in BROKERS}
    total = 0
    started = time.perf_counter()
    for batch in pq.ParquetFile(path).iter_batches(batch_size=200_000, columns=["bid", "ask", "sequence"]):
        bids = batch.column("bid").to_pylist()
        asks = batch.column("ask").to_pylist()
        sequences = batch.column("sequence").to_pylist()
        for bid, ask, sequence in zip(bids, asks, sequences):
            mid = (bid + ask) / 2.0
            side = 1 if sequence % 2 == 0 else -1
            candidates = []
            for name, spec in BROKERS.items():
                item = stats[name]
                item["orders"] += 1
                if rejected(int(sequence), spec["reject_ppm"]):
                    continue
                if side > 0:
                    execution = ask + spec["slippage"]
                    shortfall = (execution - mid) * quantity + spec["fee"] * quantity
                else:
                    execution = bid - spec["slippage"]
                    shortfall = (mid - execution) * quantity + spec["fee"] * quantity
                item["candidate_count"] += 1
                item["candidate_shortfall"] += shortfall
                latency_penalty = spec["latency_us"] * 1e-8 * quantity
                score = shortfall + latency_penalty
                candidates.append((score, name, shortfall))
            if candidates:
                _, selected, shortfall = min(candidates)
                routed[selected] += 1
                stats[selected]["accepted"] += 1
                stats[selected]["shortfall"] += shortfall
                if total % sample_step == 0:
                    stats[selected]["shortfalls"].append(shortfall / quantity)
            total += 1
    elapsed_seconds = time.perf_counter() - started
    brokers = {}
    for name, item in stats.items():
        values = item.pop("shortfalls")
        mean_order = item["shortfall"] / item["accepted"] if item["accepted"] else None
        ordered = sorted(values)
        p95 = ordered[min(len(ordered) - 1, int(len(ordered) * 0.95))] if ordered else None
        candidate_order = item["candidate_shortfall"] / item["candidate_count"] if item["candidate_count"] else None
        mean_unit = mean_order / quantity if mean_order is not None else None
        candidate_unit = candidate_order / quantity if candidate_order is not None else None
        brokers[name] = {**item, "mean_shortfall_per_order": mean_order, "mean_shortfall_per_unit": mean_unit, "p95_shortfall_per_unit": p95, "candidate_mean_shortfall_per_order": candidate_order, "candidate_mean_shortfall_per_unit": candidate_unit}
    return {"pair": pair, "ticks": total, "quantity": quantity, "elapsed_seconds": elapsed_seconds, "ticks_per_second": total / elapsed_seconds if elapsed_seconds else None, "mean_processing_us": elapsed_seconds * 1_000_000 / total if total else None, "routed_orders": routed, "brokers": brokers}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", nargs="+", type=Path)
    parser.add_argument("--quantity", type=int, default=100000)
    parser.add_argument("--sample-step", type=int, default=1000)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = {"model": "deterministic_multi_broker_shortfall", "brokers": BROKERS, "files": [run_file(path, args.quantity, args.sample_step) for path in args.input]}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False) + "\n")
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
