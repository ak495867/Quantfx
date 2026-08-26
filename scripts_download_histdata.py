from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path

import requests


PAIRS = ("EURUSD", "GBPUSD", "USDJPY", "AUDUSD")


def fetch_pair(pair: str, year: int, month: int, output: Path) -> dict:
    referer = f"https://www.histdata.com/download-free-forex-historical-data/?/ascii/tick-data-quotes/{pair.lower()}/{year}/{month}"
    session = requests.Session()
    headers = {
        "Origin": "https://www.histdata.com",
        "Referer": referer,
        "Content-Type": "application/x-www-form-urlencoded",
        "User-Agent": "Mozilla/5.0",
    }
    page = session.get(referer, headers={"User-Agent": headers["User-Agent"]}, timeout=60)
    page.raise_for_status()
    fields = dict(re.findall(r'<input type="hidden" name="([^"]+)" id="[^"]+" value="([^"]*)"', page.text))
    required = {"tk", "date", "datemonth", "platform", "timeframe", "fxpair"}
    if not required.issubset(fields):
        raise RuntimeError(f"missing download fields for {pair}")
    response = session.post("https://www.histdata.com/get.php", data=fields, headers=headers, timeout=120)
    response.raise_for_status()
    if not response.content.startswith(b"PK"):
        raise RuntimeError(f"download was not a ZIP for {pair}: {response.content[:120]!r}")
    output.mkdir(parents=True, exist_ok=True)
    archive = output / f"HISTDATA_COM_ASCII_{pair}_T_{year}{month:02d}.zip"
    archive.write_bytes(response.content)
    return {"pair": pair, "year": year, "month": month, "url": referer, "file": str(archive), "bytes": len(response.content), "sha256": hashlib.sha256(response.content).hexdigest()}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--year", type=int, default=2024)
    parser.add_argument("--month", type=int, default=1)
    parser.add_argument("--output", default="data/real")
    parser.add_argument("--pairs", nargs="*", default=list(PAIRS))
    args = parser.parse_args()
    rows = [fetch_pair(pair, args.year, args.month, Path(args.output)) for pair in args.pairs]
    for row in rows:
        print(row)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
