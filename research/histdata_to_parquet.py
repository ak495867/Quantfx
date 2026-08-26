from __future__ import annotations

import argparse
import csv
import hashlib
import json
import zipfile
from datetime import datetime, timedelta, timezone
from pathlib import Path

import pyarrow as pa
import pyarrow.parquet as pq


UTC_OFFSET = timezone(timedelta(hours=-5))
SCHEMA = pa.schema([
    ("timestamp_ns", pa.int64()),
    ("symbol", pa.string()),
    ("bid", pa.float64()),
    ("ask", pa.float64()),
    ("mid", pa.float64()),
    ("spread", pa.float64()),
    ("volume", pa.float64()),
    ("sequence", pa.int64()),
    ("flags", pa.int32()),
])


def parse_timestamp(value: str) -> int:
    date_part, time_part = value.split()
    base = datetime.strptime(date_part + time_part[:6], "%Y%m%d%H%M%S").replace(tzinfo=UTC_OFFSET)
    return int(base.timestamp() * 1_000_000_000) + int(time_part[6:]) * 1_000_000


def convert_archive(archive: Path, output: Path, batch_size: int) -> dict:
    pair = archive.name.split("_")[3]
    rows = {name: [] for name in SCHEMA.names}
    count = 0
    writer = None
    with zipfile.ZipFile(archive) as bundle:
        csv_name = next(name for name in bundle.namelist() if name.lower().endswith(".csv"))
        with bundle.open(csv_name) as raw:
            stream = (line.decode("ascii", "replace") for line in raw)
            reader = csv.reader(stream, delimiter=",")
            for sequence, row in enumerate(reader):
                if len(row) != 4:
                    continue
                try:
                    timestamp_ns = parse_timestamp(row[0])
                    bid = float(row[1])
                    ask = float(row[2])
                    volume = float(row[3])
                except (ValueError, IndexError):
                    continue
                rows["timestamp_ns"].append(timestamp_ns)
                rows["symbol"].append(pair)
                rows["bid"].append(bid)
                rows["ask"].append(ask)
                rows["mid"].append((bid + ask) / 2.0)
                rows["spread"].append(ask - bid)
                rows["volume"].append(volume)
                rows["sequence"].append(sequence)
                rows["flags"].append(0)
                count += 1
                if count % batch_size == 0:
                    table = pa.Table.from_pydict(rows, schema=SCHEMA)
                    if writer is None:
                        output.parent.mkdir(parents=True, exist_ok=True)
                        writer = pq.ParquetWriter(output, SCHEMA, compression="zstd")
                    writer.write_table(table)
                    rows = {name: [] for name in SCHEMA.names}
    if count % batch_size:
        table = pa.Table.from_pydict(rows, schema=SCHEMA)
        if writer is None:
            output.parent.mkdir(parents=True, exist_ok=True)
            writer = pq.ParquetWriter(output, SCHEMA, compression="zstd")
        writer.write_table(table)
    if writer is not None:
        writer.close()
    return {"pair": pair, "source": str(archive), "output": str(output), "records": count, "sha256": hashlib.sha256(archive.read_bytes()).hexdigest()}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", nargs="+", type=Path)
    parser.add_argument("--output", type=Path, default=Path("data/real/parquet"))
    parser.add_argument("--batch-size", type=int, default=100_000)
    args = parser.parse_args()
    manifest = [convert_archive(path, args.output / f"{path.name.rsplit('.', 1)[0]}.parquet", args.batch_size) for path in args.input]
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "manifest.json").write_text(json.dumps({"timezone": "EST_fixed_minus_5", "sources": manifest}, indent=2) + "\n")
    print(json.dumps(manifest, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
