from __future__ import annotations

import argparse
import hashlib
import json
import os
import time
from dataclasses import dataclass, asdict
from enum import Enum
from pathlib import Path


class OrderState(str, Enum):
    NEW = "NEW"
    SUBMITTED = "SUBMITTED"
    ACKED = "ACKED"
    PARTIALLY_FILLED = "PARTIALLY_FILLED"
    FILLED = "FILLED"
    CANCEL_PENDING = "CANCEL_PENDING"
    CANCELLED = "CANCELLED"
    REJECTED = "REJECTED"
    EXPIRED = "EXPIRED"
    UNKNOWN = "UNKNOWN"


TERMINAL = {
    OrderState.FILLED,
    OrderState.CANCELLED,
    OrderState.REJECTED,
    OrderState.EXPIRED,
    OrderState.UNKNOWN,
}
TRANSITIONS = {
    OrderState.NEW: {
        OrderState.SUBMITTED,
        OrderState.REJECTED,
        OrderState.EXPIRED,
        OrderState.UNKNOWN,
    },
    OrderState.SUBMITTED: {
        OrderState.ACKED,
        OrderState.REJECTED,
        OrderState.CANCEL_PENDING,
        OrderState.EXPIRED,
        OrderState.UNKNOWN,
    },
    OrderState.ACKED: {
        OrderState.PARTIALLY_FILLED,
        OrderState.FILLED,
        OrderState.CANCEL_PENDING,
        OrderState.EXPIRED,
        OrderState.UNKNOWN,
    },
    OrderState.PARTIALLY_FILLED: {
        OrderState.PARTIALLY_FILLED,
        OrderState.FILLED,
        OrderState.CANCEL_PENDING,
        OrderState.UNKNOWN,
    },
    OrderState.CANCEL_PENDING: {
        OrderState.CANCELLED,
        OrderState.PARTIALLY_FILLED,
        OrderState.FILLED,
        OrderState.UNKNOWN,
    },
    OrderState.FILLED: set(),
    OrderState.CANCELLED: set(),
    OrderState.REJECTED: set(),
    OrderState.EXPIRED: set(),
    OrderState.UNKNOWN: set(),
}


@dataclass
class OrderRecord:
    order_id: str
    broker_id: str
    state: str
    quantity: int
    filled_quantity: int
    remaining_quantity: int
    updated_ns: int
    version: int
    event_hash: str


class OrderStateError(RuntimeError):
    pass


class DurableOrderStore:
    def __init__(self, path: str | os.PathLike[str]):
        self.path = Path(path)
        self.records: dict[str, OrderRecord] = {}
        self.events: set[str] = set()
        self.chain = "0" * 64
        self.halted = False
        self._restore()

    def _restore(self) -> None:
        if not self.path.exists():
            return
        with self.path.open("rb") as stream:
            for raw in stream:
                if not raw.strip():
                    continue
                try:
                    event = json.loads(raw)
                    digest = event.pop("chain")
                    encoded = json.dumps(
                        event, sort_keys=True, separators=(",", ":")
                    ).encode()
                    expected = hashlib.sha256(
                        (self.chain + encoded.decode()).encode()
                    ).hexdigest()
                    if digest != expected:
                        self.halted = True
                        raise OrderStateError("journal chain verification failed")
                    self.chain = digest
                    self._apply_event(event, False)
                except Exception:
                    self.halted = True
                    raise

    def _append(self, event: dict) -> None:
        encoded = json.dumps(event, sort_keys=True, separators=(",", ":"))
        digest = hashlib.sha256((self.chain + encoded).encode()).hexdigest()
        payload = dict(event)
        payload["chain"] = digest
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self.path.open("ab") as stream:
            stream.write(
                (
                    json.dumps(payload, sort_keys=True, separators=(",", ":")) + "\n"
                ).encode()
            )
            stream.flush()
            os.fsync(stream.fileno())
        self.chain = digest

    def _apply_event(self, event: dict, persist: bool) -> OrderRecord:
        if event["event_id"] in self.events:
            return self.records[event["order_id"]]
        order_id = event["order_id"]
        next_state = OrderState(event["state"])
        current = self.records.get(order_id)
        if current is None:
            if next_state != OrderState.NEW:
                raise OrderStateError("order must start in NEW")
            current_state = None
            quantity = int(event["quantity"])
            filled = 0
            version = 0
            broker_id = str(event.get("broker_id", ""))
        else:
            current_state = OrderState(current.state)
            if next_state not in TRANSITIONS[current_state]:
                raise OrderStateError(
                    f"illegal transition {current_state.value}->{next_state.value}"
                )
            quantity = current.quantity
            filled = int(event.get("filled_quantity", current.filled_quantity))
            version = current.version + 1
            broker_id = str(event.get("broker_id", current.broker_id))
        if quantity <= 0 or filled < 0 or filled > quantity:
            raise OrderStateError("invalid quantity state")
        remaining = quantity - filled
        if next_state == OrderState.FILLED and remaining != 0:
            raise OrderStateError("filled order must have zero remaining quantity")
        if next_state == OrderState.PARTIALLY_FILLED and not 0 < filled < quantity:
            raise OrderStateError("partial fill quantity is invalid")
        record = OrderRecord(
            order_id,
            broker_id,
            next_state.value,
            quantity,
            filled,
            remaining,
            int(event["updated_ns"]),
            version,
            self.chain,
        )
        self.records[order_id] = record
        self.events.add(event["event_id"])
        if persist:
            self._append(event)
        return record

    def transition(
        self,
        order_id: str,
        state: OrderState,
        filled_quantity: int | None = None,
        broker_id: str = "",
        event_id: str | None = None,
    ) -> OrderRecord:
        if self.halted:
            raise OrderStateError("order store halted")
        event_id = (
            event_id
            or hashlib.sha256(
                f"{order_id}:{state.value}:{filled_quantity}:{time.time_ns()}".encode()
            ).hexdigest()
        )
        current = self.records.get(order_id)
        quantity = current.quantity if current else 0
        event = {
            "event_id": event_id,
            "order_id": order_id,
            "broker_id": broker_id,
            "state": state.value,
            "quantity": quantity,
            "filled_quantity": (
                filled_quantity
                if filled_quantity is not None
                else (current.filled_quantity if current else 0)
            ),
            "updated_ns": time.time_ns(),
        }
        if current is None and state == OrderState.NEW:
            raise OrderStateError("NEW requires create")
        return self._apply_event(event, True)

    def create(
        self,
        order_id: str,
        quantity: int,
        broker_id: str = "",
        event_id: str | None = None,
    ) -> OrderRecord:
        if self.halted:
            raise OrderStateError("order store halted")
        if order_id in self.records:
            return self.records[order_id]
        event = {
            "event_id": event_id
            or hashlib.sha256(f"create:{order_id}:{quantity}".encode()).hexdigest(),
            "order_id": order_id,
            "broker_id": broker_id,
            "state": OrderState.NEW.value,
            "quantity": quantity,
            "filled_quantity": 0,
            "updated_ns": time.time_ns(),
        }
        return self._apply_event(event, True)

    def reconcile(self, broker_snapshot: dict[str, dict]) -> None:
        for order_id, record in self.records.items():
            snapshot = broker_snapshot.get(order_id)
            if snapshot is None and OrderState(record.state) not in TERMINAL:
                self.halted = True
                raise OrderStateError("open order missing from broker snapshot")
            if (
                snapshot is not None
                and int(snapshot.get("filled_quantity", -1)) != record.filled_quantity
            ):
                self.halted = True
                raise OrderStateError("broker fill mismatch")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("journal")
    args = parser.parse_args()
    store = DurableOrderStore(args.journal)
    print(
        json.dumps(
            {key: asdict(value) for key, value in store.records.items()}, sort_keys=True
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
