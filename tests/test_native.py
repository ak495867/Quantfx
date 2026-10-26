from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from runtime.qfx_native import NativeEvent, NativeOrder, QfxNative


def main() -> int:
    native = QfxNative()
    ts = native.now_ns()
    assert ts > 0

    vwap, filled = native.sweep_l2(1, 1500000)
    assert filled == 1500000
    assert vwap > 0

    events = [
        NativeEvent(
            timestamp_ns=i * 1000,
            symbol=1,
            bid=100000,
            ask=100010,
            last=100005,
            volume=10,
            bid_size=100,
            ask_size=100,
            sequence=i,
            flags=0,
        )
        for i in range(20)
    ]
    report = native.process_events(events)
    assert report.events == 20

    order = NativeOrder(
        side=1,
        quantity=500000,
        price=108020,
        stop_loss=0,
        take_profit=0,
        symbol=1,
        client_order_id=42,
    )
    encoded, decoded, decoded_ts = native.sbe_roundtrip(order, 987654321)
    assert len(encoded) == 56
    assert decoded.client_order_id == 42
    assert decoded.price == 108020
    assert decoded_ts == 987654321

    print("PYTHON_NATIVE_PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
