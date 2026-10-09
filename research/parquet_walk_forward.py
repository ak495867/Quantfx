from __future__ import annotations

import argparse
import json
from dataclasses import asdict, dataclass
from pathlib import Path

import numpy as np
import pandas as pd
import pyarrow.parquet as pq


@dataclass
class FoldResult:
    pair: str
    fold: int
    train_start: str
    train_end: str
    purge_start: str
    purge_end: str
    test_start: str
    test_end: str
    embargo_end: str
    pip_scale: int
    threshold_raw: float
    threshold_pips: float
    train_pnl_raw: float
    test_pnl_raw: float
    train_pnl_pips: float
    test_pnl_pips: float
    train_pnl_pips_per_bar: float
    test_pnl_pips_per_bar: float
    alpha_decay: float | None
    train_observations: int
    test_observations: int


def load_series(path: Path, bar_interval: str) -> pd.Series:
    table = pq.read_table(path, columns=["timestamp_ns", "mid"])
    frame = table.to_pandas()
    index = pd.to_datetime(frame["timestamp_ns"], unit="ns", utc=True)
    series = pd.Series(frame["mid"].to_numpy(dtype=float), index=index).sort_index()
    return series.resample(bar_interval).last().dropna()


def score(series: pd.Series, threshold: float) -> float:
    changes = series.diff().dropna()
    signals = np.where(
        changes > threshold, 1.0, np.where(changes < -threshold, -1.0, 0.0)
    )
    return float(np.sum(signals * changes.to_numpy()))


def run_pair(
    path: Path,
    train_bars: int,
    test_bars: int,
    purge_bars: int,
    embargo_bars: int,
    folds: int,
    bar_interval: str,
) -> list[FoldResult]:
    pair = path.stem.split("_")[3]
    pip_scale = 100 if "JPY" in pair else 10000
    series = load_series(path, bar_interval)
    results: list[FoldResult] = []
    train_start = 0
    test_start = train_start + train_bars + purge_bars
    thresholds_fraction = np.array([0.1, 0.25, 0.5, 0.75])
    for fold in range(folds):
        train_end = train_start + train_bars
        purge_end = test_start
        test_end = test_start + test_bars
        if test_end > len(series):
            break
        train = series.iloc[train_start:train_end]
        test = series.iloc[test_start:test_end]
        changes = train.diff().dropna().abs().to_numpy()
        candidates = (
            np.quantile(changes, thresholds_fraction)
            if len(changes)
            else np.array([0.0])
        )
        scores = [score(train, float(candidate)) for candidate in candidates]
        best_index = int(np.argmax(scores))
        threshold = float(candidates[best_index])
        train_pnl_raw = float(scores[best_index])
        test_pnl_raw = score(test, threshold)
        train_pnl_pips = train_pnl_raw * pip_scale
        test_pnl_pips = test_pnl_raw * pip_scale
        decay = (
            None if train_pnl_pips == 0 else float(1.0 - test_pnl_pips / train_pnl_pips)
        )
        results.append(
            FoldResult(
                pair,
                fold,
                str(train.index[0]),
                str(train.index[-1]),
                str(series.index[train_end]),
                str(series.index[purge_end - 1]),
                str(test.index[0]),
                str(test.index[-1]),
                str(series.index[min(test_end + embargo_bars - 1, len(series) - 1)]),
                pip_scale,
                threshold,
                threshold * pip_scale,
                train_pnl_raw,
                test_pnl_raw,
                train_pnl_pips,
                test_pnl_pips,
                train_pnl_pips / len(train),
                test_pnl_pips / len(test),
                decay,
                len(train),
                len(test),
            )
        )
        train_start = test_end + embargo_bars
        test_start = train_start + train_bars + purge_bars
    return results


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", nargs="+", type=Path)
    parser.add_argument("--train-bars", type=int, default=500)
    parser.add_argument("--test-bars", type=int, default=150)
    parser.add_argument("--purge-bars", type=int, default=5)
    parser.add_argument("--embargo-bars", type=int, default=5)
    parser.add_argument("--folds", type=int, default=5)
    parser.add_argument("--bar-interval", default="5min")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    folds = [
        asdict(fold)
        for path in args.input
        for fold in run_pair(
            path,
            args.train_bars,
            args.test_bars,
            args.purge_bars,
            args.embargo_bars,
            args.folds,
            args.bar_interval,
        )
    ]
    result = {
        "model": "purged_walk_forward_threshold_optimization",
        "parameters": vars(args)
        | {"input": [str(path) for path in args.input], "output": str(args.output)},
        "folds": folds,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False) + "\n")
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
