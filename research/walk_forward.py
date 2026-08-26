from __future__ import annotations

import argparse
import csv
import json
from dataclasses import asdict, dataclass
from pathlib import Path


@dataclass(frozen=True)
class Event:
    timestamp_ns: int
    bid: int
    ask: int
    last: int


@dataclass(frozen=True)
class Fold:
    fold: int
    train_start: int
    train_end: int
    purge_start: int
    purge_end: int
    test_start: int
    test_end: int
    embargo_end: int
    threshold: float
    train_pnl: int
    test_pnl: int


def load_events(path: str) -> list[Event]:
    events: list[Event] = []
    with Path(path).open(newline="") as stream:
        for row in csv.DictReader(stream):
            events.append(Event(int(row["timestamp_ns"]), int(row["bid"]), int(row["ask"]), int(row["last"])))
    events.sort(key=lambda event: event.timestamp_ns)
    return events


def returns(events: list[Event]) -> list[int]:
    return [events[index + 1].last - events[index].last for index in range(len(events) - 1)]


def evaluate(events: list[Event], threshold: float) -> int:
    pnl = 0
    for index in range(len(events) - 1):
        delta = events[index + 1].last - events[index].last
        if delta > threshold:
            pnl += delta
        elif delta < -threshold:
            pnl -= delta
    return pnl


def walk_forward(events: list[Event], train_size: int, test_size: int, purge: int, embargo: int, folds: int) -> list[Fold]:
    if train_size <= 1 or test_size <= 0 or purge < 0 or embargo < 0 or folds <= 0:
        raise ValueError("invalid walk-forward parameters")
    output: list[Fold] = []
    train_start = 0
    test_start = train_start + train_size + purge
    for fold in range(folds):
        train_end = train_start + train_size
        purge_start = train_end
        purge_end = test_start
        test_end = test_start + test_size
        if test_end > len(events):
            break
        train_returns = returns(events[train_start:train_end])
        threshold = abs(sum(train_returns) / len(train_returns)) if train_returns else 0.0
        train_pnl = evaluate(events[train_start:train_end], threshold)
        test_pnl = evaluate(events[test_start:test_end], threshold)
        output.append(Fold(fold, train_start, train_end, purge_start, purge_end, test_start, test_end, test_end + embargo, threshold, train_pnl, test_pnl))
        train_start = test_end + embargo
        test_start = train_start + train_size + purge
    return output


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("--train-size", type=int, default=100)
    parser.add_argument("--test-size", type=int, default=25)
    parser.add_argument("--purge", type=int, default=1)
    parser.add_argument("--embargo", type=int, default=1)
    parser.add_argument("--folds", type=int, default=5)
    args = parser.parse_args()
    events = load_events(args.input)
    folds = walk_forward(events, args.train_size, args.test_size, args.purge, args.embargo, args.folds)
    print(json.dumps({"events": len(events), "folds": [asdict(fold) for fold in folds]}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
