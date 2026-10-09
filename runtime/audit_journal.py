from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
import secrets
from pathlib import Path
from cryptography.hazmat.primitives.ciphers.aead import AESGCM


class AuditIntegrityError(RuntimeError):
    pass


class EncryptedAuditJournal:
    def __init__(self, path: str | os.PathLike[str], key: bytes):
        if len(key) not in (16, 24, 32):
            raise ValueError("AES key must be 16, 24, or 32 bytes")
        self.path = Path(path)
        self.key = key
        self.chain = "0" * 64
        self.count = 0
        self._verify_existing()

    def _verify_existing(self) -> None:
        if not self.path.exists():
            return
        with self.path.open("rb") as stream:
            for raw in stream:
                if not raw.strip():
                    continue
                record = json.loads(raw)
                nonce = base64.b64decode(record["nonce"])
                ciphertext = base64.b64decode(record["ciphertext"])
                expected_chain = hashlib.sha256(
                    (
                        self.chain + base64.b64encode(nonce + ciphertext).decode()
                    ).encode()
                ).hexdigest()
                if record["chain"] != expected_chain:
                    raise AuditIntegrityError("audit chain verification failed")
                AESGCM(self.key).decrypt(nonce, ciphertext, self.chain.encode())
                self.chain = record["chain"]
                self.count += 1

    def append(self, event: dict) -> str:
        payload = json.dumps(event, sort_keys=True, separators=(",", ":")).encode()
        nonce = secrets.token_bytes(12)
        ciphertext = AESGCM(self.key).encrypt(nonce, payload, self.chain.encode())
        chain_input = base64.b64encode(nonce + ciphertext).decode()
        chain = hashlib.sha256((self.chain + chain_input).encode()).hexdigest()
        record = {
            "nonce": base64.b64encode(nonce).decode(),
            "ciphertext": base64.b64encode(ciphertext).decode(),
            "chain": chain,
        }
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self.path.open("ab") as stream:
            stream.write(
                (
                    json.dumps(record, sort_keys=True, separators=(",", ":")) + "\n"
                ).encode()
            )
            stream.flush()
            os.fsync(stream.fileno())
        self.chain = chain
        self.count += 1
        return chain

    def verify(self) -> int:
        self.chain = "0" * 64
        self.count = 0
        self._verify_existing()
        return self.count


def load_key(path: str) -> bytes:
    return Path(path).read_bytes()


def main() -> int:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    init_key = sub.add_parser("init-key")
    init_key.add_argument("path")
    append = sub.add_parser("append")
    append.add_argument("journal")
    append.add_argument("key")
    append.add_argument("payload")
    verify = sub.add_parser("verify")
    verify.add_argument("journal")
    verify.add_argument("key")
    args = parser.parse_args()
    if args.command == "init-key":
        Path(args.path).parent.mkdir(parents=True, exist_ok=True)
        Path(args.path).write_bytes(secrets.token_bytes(32))
        os.chmod(args.path, 0o600)
        return 0
    journal = EncryptedAuditJournal(args.journal, load_key(args.key))
    if args.command == "append":
        event = json.loads(args.payload)
        print(journal.append(event))
        return 0
    print(journal.verify())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
