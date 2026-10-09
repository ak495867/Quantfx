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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("path")
    parser.add_argument("--symbol", default="UNKNOWN")
    args = parser.parse_args()
    table = pq.read_table(args.path)
    columns = set(table.column_names)
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
    arrays = {
        name: table[name].to_pylist() if name else None
        for name in [
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
    }
    size = len(arrays[timestamp])
    for index in range(size):
        ts = int(arrays[timestamp][index])
        b = (
            scale(arrays[bid][index], 100000)
            if isinstance(arrays[bid][index], float)
            else int(arrays[bid][index])
        )
        a = (
            scale(arrays[ask][index], 100000)
            if isinstance(arrays[ask][index], float)
            else int(arrays[ask][index])
        )
        l = (
            scale(arrays[last][index], 100000)
            if isinstance(arrays[last][index], float)
            else int(arrays[last][index])
        )
        v = int(arrays[volume][index]) if volume else 0
        bs = int(arrays[bid_size][index]) if bid_size else 0
        asks = int(arrays[ask_size][index]) if ask_size else 0
        seq = int(arrays[sequence][index]) if sequence else index
        flg = int(arrays[flags][index]) if flags else 0
        sys.stdout.write(
            f"{ts},{args.symbol},{b},{a},{l},{v},{bs},{asks},{seq},{flg}\n"
        )


if __name__ == "__main__":
    main()
