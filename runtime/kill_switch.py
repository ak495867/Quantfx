from __future__ import annotations

import argparse
import hashlib
import hmac
import json
import os
import time
from pathlib import Path


class KillSwitchError(RuntimeError):
    pass


class KillSwitch:
    def __init__(
        self,
        path: str | os.PathLike[str],
        key: bytes,
        max_age_ns: int = 86_400_000_000_000,
    ):
        if len(key) < 32:
            raise ValueError("kill-switch key must be at least 32 bytes")
        self.path = Path(path)
        self.key = key
        self.max_age_ns = max_age_ns

    def _sign(self, payload: dict) -> str:
        encoded = json.dumps(payload, sort_keys=True, separators=(",", ":")).encode()
        return hmac.new(self.key, encoded, hashlib.sha256).hexdigest()

    def _write(self, enabled: bool, reason: str) -> None:
        payload = {"enabled": enabled, "reason": reason, "updated_ns": time.time_ns()}
        record = dict(payload)
        record["signature"] = self._sign(payload)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        temporary = self.path.with_suffix(self.path.suffix + ".new")
        temporary.write_text(json.dumps(record, sort_keys=True) + "\n")
        os.chmod(temporary, 0o600)
        os.replace(temporary, self.path)

    def trip(self, reason: str) -> None:
        self._write(True, reason)

    def clear(self, reason: str) -> None:
        self._write(False, reason)

    def status(self) -> dict:
        if not self.path.exists():
            return {"enabled": True, "reason": "missing-kill-switch"}
        record = json.loads(self.path.read_text())
        payload = {key: record[key] for key in ("enabled", "reason", "updated_ns")}
        if not hmac.compare_digest(record.get("signature", ""), self._sign(payload)):
            return {"enabled": True, "reason": "invalid-kill-switch-signature"}
        if time.time_ns() - int(record["updated_ns"]) > self.max_age_ns:
            return {"enabled": True, "reason": "stale-kill-switch"}
        return payload

    def can_trade(self) -> bool:
        status = self.status()
        return status.get("enabled") is False


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("path")
    parser.add_argument("key")
    parser.add_argument("command", choices=("trip", "clear", "status", "can-trade"))
    parser.add_argument("reason", nargs="?", default="operator")
    args = parser.parse_args()
    key = Path(args.key).read_bytes()
    switch = KillSwitch(args.path, key)
    if args.command == "trip":
        switch.trip(args.reason)
        return 0
    if args.command == "clear":
        switch.clear(args.reason)
        return 0
    if args.command == "status":
        print(json.dumps(switch.status(), sort_keys=True))
        return 0
    if switch.can_trade():
        return 0
    raise KillSwitchError(json.dumps(switch.status(), sort_keys=True))


if __name__ == "__main__":
    raise SystemExit(main())
