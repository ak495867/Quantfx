import argparse
import sys
import pyarrow.parquet as pq


def pick(names, columns):
    for name in names:
        if name in columns:
            return name
    return None


def scale(value, factor):
    return int(round(float(value) * factor))


from __future__ import annotations

import argparse
import sys
from typing import Sequence
import pyarrow.parquet as pq


def pick(names: Sequence[str], columns: set[str]) -> str | None:
    for name in names:
        if name in columns:
            return name
    return None


def scale(value: float | int, factor: int) -> int:
    return int(round(float(value) * factor))


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Stream and normalize Parquet market data to canonical QuantFX CSV format."
    )
    parser.add_argument("path", help="Path to Parquet input file")
    parser.add_argument(
        "--symbol", default="UNKNOWN", help="Symbol name (e.g., EURUSD)"
    )
    parser.add_argument(
        "--batch-size", type=int, default=65536, help="Batch streaming buffer size"
    )
    args = parser.parse_args()

    parquet_file = pq.ParquetFile(args.path)
    columns = set(parquet_file.schema.names)
    timestamp = pick(["timestamp_ns", "timestamp", "time", "datetime", "ts"], columns)
    bid = pick(["bid", "bid_price", "bid_px"], columns)
    ask = pick(["ask", "ask_price", "ask_px"], columns)
    last = pick(["last", "last_price", "close", "price", "mid"], columns)
    volume = pick(["volume", "size", "qty"], columns)
    bid_size = pick(["bid_size", "bid_qty"], columns)
    ask_size = pick(["ask_size", "ask_qty"], columns)
    sequence = pick(["sequence", "seq", "id"], columns)
    flags = pick(["flags", "flag"], columns)
    required = [timestamp, bid, ask, last]
    if any(item is None for item in required):
        raise SystemExit("missing required parquet columns")

    needed_cols = [
        col
        for col in [
            timestamp,
            bid,
            ask,
            last,
            volume,
            bid_size,
            ask_size,
            sequence,
            flags,
        ]
        if col
    ]
    symbol = args.symbol
    out = sys.stdout.buffer
    seq_counter = 0

    for batch in parquet_file.iter_batches(
        batch_size=args.batch_size, columns=needed_cols
    ):
        b_len = batch.num_rows
        if b_len == 0:
            continue
        ts_arr = batch.column(timestamp).to_pylist()
        bid_arr = batch.column(bid).to_pylist()
        ask_arr = batch.column(ask).to_pylist()
        last_arr = batch.column(last).to_pylist()
        vol_arr = batch.column(volume).to_pylist() if volume else None
        bs_arr = batch.column(bid_size).to_pylist() if bid_size else None
        as_arr = batch.column(ask_size).to_pylist() if ask_size else None
        seq_arr = batch.column(sequence).to_pylist() if sequence else None
        flg_arr = batch.column(flags).to_pylist() if flags else None

        lines: list[bytes] = []
        for index in range(b_len):
            ts = int(ts_arr[index])
            b_val = bid_arr[index]
            b = scale(b_val, 100000) if isinstance(b_val, float) else int(b_val)
            a_val = ask_arr[index]
            a = scale(a_val, 100000) if isinstance(a_val, float) else int(a_val)
            l_val = last_arr[index]
            l = scale(l_val, 100000) if isinstance(l_val, float) else int(l_val)
            v = int(vol_arr[index]) if vol_arr else 0
            bs = int(bs_arr[index]) if bs_arr else 0
            asks = int(as_arr[index]) if as_arr else 0
            seq = int(seq_arr[index]) if seq_arr else (seq_counter + index)
            flg = int(flg_arr[index]) if flg_arr else 0
            lines.append(
                f"{ts},{symbol},{b},{a},{l},{v},{bs},{asks},{seq},{flg}\n".encode(
                    "ascii"
                )
            )
        seq_counter += b_len
        out.write(b"".join(lines))
    out.flush()


if __name__ == "__main__":
    main()
