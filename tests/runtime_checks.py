from __future__ import annotations

import json
import tempfile
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from research.shortfall import model
from research.walk_forward import Event, walk_forward
from runtime.audit_journal import AuditIntegrityError, EncryptedAuditJournal
from runtime.kill_switch import KillSwitch
from runtime.order_state import DurableOrderStore, OrderState, OrderStateError


def main() -> int:
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        journal = root / "orders.jsonl"
        store = DurableOrderStore(journal)
        store.create("order-1", 1000, "broker-a")
        store.transition("order-1", OrderState.SUBMITTED)
        store.transition("order-1", OrderState.ACKED)
        store.transition("order-1", OrderState.PARTIALLY_FILLED, 400)
        store.transition("order-1", OrderState.FILLED, 1000)
        restored = DurableOrderStore(journal)
        assert restored.records["order-1"].state == "FILLED"
        try:
            restored.transition("order-1", OrderState.CANCELLED)
        except OrderStateError:
            pass
        else:
            raise AssertionError("terminal transition accepted")
        key = root / "audit.key"
        key.write_bytes(b"k" * 32)
        audit_path = root / "audit.log"
        audit = EncryptedAuditJournal(audit_path, key.read_bytes())
        audit.append({"event": "accepted", "order_id": "order-1"})
        assert EncryptedAuditJournal(audit_path, key.read_bytes()).verify() == 1
        tampered = json.loads(audit_path.read_text())
        tampered["chain"] = "0" * 64
        audit_path.write_text(json.dumps(tampered) + "\n")
        try:
            EncryptedAuditJournal(audit_path, key.read_bytes())
        except AuditIntegrityError:
            pass
        else:
            raise AssertionError("audit tampering accepted")
        switch = KillSwitch(root / "switch.json", key.read_bytes())
        assert not switch.can_trade()
        switch.clear("test")
        assert switch.can_trade()
        switch.trip("test")
        assert not switch.can_trade()
    events = [Event(index, 100000 + index, 100010 + index, 100005 + index) for index in range(40)]
    folds = walk_forward(events, 10, 5, 2, 2, 2)
    assert folds[0].purge_end - folds[0].purge_start == 2
    assert folds[0].test_start - folds[0].train_end == 2
    result = model(100.0, 1, 100, [{"quantity": 60, "price": 100.2, "fee": 0.5}, {"quantity": 40, "price": 100.3, "fee": 0.5}], 100.4)
    assert result.filled_quantity == 100
    assert result.total_cost > 0
    print("RUNTIME_PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
