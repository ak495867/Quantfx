from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path

import requests
from requests.adapters import HTTPAdapter
from urllib3.util import Retry

PAIRS = ("EURUSD", "GBPUSD", "USDJPY", "AUDUSD")


def get_resilient_session() -> requests.Session:
    session = requests.Session()
    retries = Retry(
        total=5,
        backoff_factor=1.5,
        status_forcelist=[429, 500, 502, 503, 504],
        raise_on_status=False,
    )
    adapter = HTTPAdapter(max_retries=retries, pool_connections=10, pool_maxsize=10)
    session.mount("https://", adapter)
    session.mount("http://", adapter)
    return session


def fetch_pair(
    pair: str,
    year: int,
    month: int,
    output: Path,
    session: requests.Session | None = None,
) -> dict:
    referer = f"https://www.histdata.com/download-free-forex-historical-data/?/ascii/tick-data-quotes/{pair.lower()}/{year}/{month}"
    sess = session or get_resilient_session()
    headers = {
        "Origin": "https://www.histdata.com",
        "Referer": referer,
        "Content-Type": "application/x-www-form-urlencoded",
        "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
    }
    page = sess.get(referer, headers={"User-Agent": headers["User-Agent"]}, timeout=60)
    page.raise_for_status()
    fields = dict(
        re.findall(
            r'<input type="hidden" name="([^"]+)" id="[^"]+" value="([^"]*)"', page.text
        )
    )
    required = {"tk", "date", "datemonth", "platform", "timeframe", "fxpair"}
    if not required.issubset(fields):
        raise RuntimeError(f"missing download fields for {pair}")

    output.mkdir(parents=True, exist_ok=True)
    archive = output / f"HISTDATA_COM_ASCII_{pair}_T_{year}{month:02d}.zip"
    part_file = archive.with_suffix(".zip.part")

    with sess.post(
        "https://www.histdata.com/get.php",
        data=fields,
        headers=headers,
        timeout=180,
        stream=True,
    ) as response:
        response.raise_for_status()
        hasher = hashlib.sha256()
        total_bytes = 0
        first_chunk = True

        with part_file.open("wb") as stream:
            for chunk in response.iter_content(chunk_size=1048576):
                if not chunk:
                    continue
                if first_chunk:
                    if not chunk.startswith(b"PK"):
                        raise RuntimeError(
                            f"download was not a ZIP for {pair}: {chunk[:120]!r}"
                        )
                    first_chunk = False
                stream.write(chunk)
                hasher.update(chunk)
                total_bytes += len(chunk)

    part_file.replace(archive)
    return {
        "pair": pair,
        "year": year,
        "month": month,
        "url": referer,
        "file": str(archive),
        "bytes": total_bytes,
        "sha256": hasher.hexdigest(),
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Download free historical forex tick data from HistData with automatic retries and streaming."
    )
    parser.add_argument("--year", type=int, default=2024)
    parser.add_argument("--month", type=int, default=1)
    parser.add_argument("--output", default="data/real")
    parser.add_argument("--pairs", nargs="*", default=list(PAIRS))
    args = parser.parse_args()
    session = get_resilient_session()
    rows = [
        fetch_pair(pair, args.year, args.month, Path(args.output), session=session)
        for pair in args.pairs
    ]
    for row in rows:
        print(row)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
