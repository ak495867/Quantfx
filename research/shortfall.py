from __future__ import annotations

import argparse
import json
from dataclasses import asdict, dataclass
from pathlib import Path


@dataclass(frozen=True)
class Shortfall:
    target_quantity: int
    filled_quantity: int
    unfilled_quantity: int
    decision_price: float
    arrival_price: float
    execution_vwap: float
    benchmark_price: float
    arrival_cost: float
    delay_cost: float
    spread_cost: float
    fee_cost: float
    impact_cost: float
    opportunity_cost: float
    total_cost: float
    cost_per_unit: float


def model(
    decision_price: float,
    side: int,
    target_quantity: int,
    fills: list[dict],
    benchmark_price: float | None = None,
) -> Shortfall:
    if side not in (-1, 1) or target_quantity <= 0:
        raise ValueError("side and target quantity are invalid")
    filled = sum(int(fill["quantity"]) for fill in fills)
    if filled < 0 or filled > target_quantity:
        raise ValueError("fill quantity is invalid")
    if filled:
        execution_vwap = (
            sum(int(fill["quantity"]) * float(fill["price"]) for fill in fills) / filled
        )
        arrival_price = (
            sum(
                int(fill["quantity"]) * float(fill.get("arrival_price", decision_price))
                for fill in fills
            )
            / filled
        )
        fees = sum(float(fill.get("fee", 0.0)) for fill in fills)
        impacts = sum(float(fill.get("impact", 0.0)) for fill in fills)
    else:
        execution_vwap = decision_price
        arrival_price = decision_price
        fees = 0.0
        impacts = 0.0
    benchmark = execution_vwap if benchmark_price is None else benchmark_price
    arrival_cost = side * (arrival_price - decision_price) * filled
    delay_cost = side * (execution_vwap - arrival_price) * filled
    spread_cost = side * (execution_vwap - arrival_price) * filled
    opportunity_cost = side * (benchmark - decision_price) * (target_quantity - filled)
    total = arrival_cost + delay_cost + fees + impacts + opportunity_cost
    return Shortfall(
        target_quantity,
        filled,
        target_quantity - filled,
        decision_price,
        arrival_price,
        execution_vwap,
        benchmark,
        arrival_cost,
        delay_cost,
        spread_cost,
        fees,
        impacts,
        opportunity_cost,
        total,
        total / target_quantity,
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("--decision-price", type=float, required=True)
    parser.add_argument("--side", type=int, choices=(-1, 1), required=True)
    parser.add_argument("--quantity", type=int, required=True)
    parser.add_argument("--benchmark-price", type=float)
    args = parser.parse_args()
    fills = json.loads(Path(args.input).read_text())
    result = model(
        args.decision_price, args.side, args.quantity, fills, args.benchmark_price
    )
    print(json.dumps(asdict(result), sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
