from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from runtime.qfx_native import (
    NativeEvent,
    NativeOrder,
    NativeVenueQuote,
    NativeSbeHeader,
    NativeSbeOrderMessage,
    NativeSbeExecutionReport,
    QfxNative,
)


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

    v1 = native.eval_risk(order, 108018, 0, 1000000)
    assert v1 == 0

    fat_finger = NativeOrder(1, 100000, 108500, 0, 0, 1, 43)
    v2 = native.eval_risk(fat_finger, 108018, 0, 1000100)
    assert v2 == 2

    quotes = [
        NativeVenueQuote(1, 108000, 108015, 500000, 2, 1, 0),
        NativeVenueQuote(2, 108005, 108012, 1000000, 1, 1, 0),
        NativeVenueQuote(3, 107995, 108018, 2000000, 3, 2, 0),
    ]
    splits = native.split_venues(1, 1200000, quotes)
    assert len(splits) >= 2
    assert sum(s.quantity for s in splits) == 1200000

    simd_scores = native.score_simd4(
        [108015, 108020, 108012, 108018],
        [108000, 108005, 108002, 107995],
        [800, 1200, 300, 500],
        [3, 1, 8, 2],
        [2, 1, 0, 1],
    )
    assert len(simd_scores) == 4
    assert simd_scores[2] > 0

    returns = [i * 2 if i % 2 == 0 else -i * 3 for i in range(200)]
    var_99, cvar_99 = native.compute_var(returns)
    assert var_99 > 0
    assert cvar_99 >= var_99

    sbe_req = NativeSbeOrderMessage(
        header=NativeSbeHeader(56, 101, 1, 1),
        timestamp_ns=1000,
        client_order_id=99,
        symbol=1,
        price=108005,
        quantity=500000,
        side=1,
        time_in_force=0,
        padding=0,
        reserved=0,
    )
    sbe_rep = native.venue_match(sbe_req, 2000)
    assert sbe_rep.client_order_id == 99
    assert sbe_rep.exec_price == 108005
    assert sbe_rep.exec_quantity == 500000

    sharpe, sortino = native.quant_stats(
        100000000, 110000000, 2000000, 15000000, 5000000, 150
    )
    assert sharpe > 0.0
    assert sortino > 0.0

    print("PYTHON_NATIVE_ALL_PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
