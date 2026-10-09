from __future__ import annotations

import argparse
import json
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def pip_scale(pair: str) -> int:
    return 100 if "JPY" in pair else 10000


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--shortfall", type=Path, required=True)
    parser.add_argument("--walk-forward", type=Path, required=True)
    parser.add_argument("--figures", type=Path, required=True)
    parser.add_argument("--results", type=Path, required=True)
    args = parser.parse_args()
    args.figures.mkdir(parents=True, exist_ok=True)
    args.results.mkdir(parents=True, exist_ok=True)
    shortfall = json.loads(args.shortfall.read_text())
    short_rows = []
    route_rows = []
    for item in shortfall["files"]:
        pair = item["pair"]
        for broker, metrics in item["brokers"].items():
            short_rows.append(
                {
                    "pair": pair,
                    "broker": broker,
                    "candidate_mean_price_units_per_unit": metrics[
                        "candidate_mean_shortfall_per_unit"
                    ],
                    "candidate_mean_pips_per_unit": metrics[
                        "candidate_mean_shortfall_per_unit"
                    ]
                    * pip_scale(pair),
                    "candidate_mean_shortfall_per_order": metrics[
                        "candidate_mean_shortfall_per_order"
                    ],
                    "routed_orders": item["routed_orders"][broker],
                    "ticks": item["ticks"],
                    "route_share": item["routed_orders"][broker] / item["ticks"],
                }
            )
            route_rows.append(
                {
                    "pair": pair,
                    "broker": broker,
                    "route_share": item["routed_orders"][broker] / item["ticks"],
                }
            )
    sf = pd.DataFrame(short_rows)
    routes = pd.DataFrame(route_rows)
    wf = pd.DataFrame(json.loads(args.walk_forward.read_text())["folds"])
    wf["test_date"] = pd.to_datetime(wf["test_start"])
    wf["alpha_decay_pct"] = wf["alpha_decay"] * 100.0
    sf.to_csv(args.results / "shortfall_by_broker.csv", index=False)
    routes.to_csv(args.results / "routing_share.csv", index=False)
    wf.to_csv(args.results / "walk_forward_folds.csv", index=False)
    summary = {
        "pairs": sorted(sf["pair"].unique().tolist()),
        "ticks": {item["pair"]: item["ticks"] for item in shortfall["files"]},
        "mean_candidate_shortfall_pips_per_unit": sf.groupby("broker")[
            "candidate_mean_pips_per_unit"
        ]
        .mean()
        .to_dict(),
        "route_share": routes.groupby("broker")["route_share"].mean().to_dict(),
        "mean_test_pnl_pips": wf.groupby("pair")["test_pnl_pips"].mean().to_dict(),
        "mean_test_pnl_pips_per_bar": wf.groupby("pair")["test_pnl_pips_per_bar"]
        .mean()
        .to_dict(),
        "mean_alpha_decay_pct": wf.groupby("pair")["alpha_decay_pct"].mean().to_dict(),
        "median_alpha_decay_pct": wf.groupby("pair")["alpha_decay_pct"]
        .median()
        .to_dict(),
        "folds": int(len(wf)),
    }
    (args.results / "benchmark_summary.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n"
    )
    plt.style.use("seaborn-v0_8-whitegrid")
    fig, ax = plt.subplots(figsize=(11, 6))
    for broker, group in sf.groupby("broker"):
        ax.plot(
            group["pair"],
            group["candidate_mean_pips_per_unit"],
            marker="o",
            linewidth=2,
            label=broker,
        )
    ax.set_title("Real Tick Feed: Deterministic Modeled Execution Cost by Venue")
    ax.set_ylabel("Modeled candidate cost (pips per unit)")
    ax.set_xlabel("Currency pair")
    ax.legend()
    fig.tight_layout()
    fig.savefig(args.figures / "shortfall_by_broker.png", dpi=180)
    plt.close(fig)
    pivot = routes.pivot(index="pair", columns="broker", values="route_share").fillna(
        0.0
    )
    fig, ax = plt.subplots(figsize=(11, 6))
    pivot.plot.bar(stacked=True, ax=ax, color=["#264653", "#2a9d8f", "#e9c46a"])
    ax.set_title("Deterministic Modeled Router Allocation Share")
    ax.set_ylabel("Share of ticks routed")
    ax.set_xlabel("Currency pair")
    ax.set_ylim(0, 1)
    ax.legend(title="Venue")
    fig.tight_layout()
    fig.savefig(args.figures / "routing_share.png", dpi=180)
    plt.close(fig)
    fig, ax = plt.subplots(figsize=(11, 6))
    for pair, group in wf.groupby("pair"):
        ax.plot(
            group["fold"], group["alpha_decay_pct"], marker="o", linewidth=2, label=pair
        )
    ax.axhline(0, color="black", linewidth=1)
    ax.set_title("Purged Walk-Forward: Out-of-Sample Normalized PnL Decay")
    ax.set_ylabel("Decay from train to test (%)")
    ax.set_xlabel("Walk-forward fold")
    ax.legend()
    fig.tight_layout()
    fig.savefig(args.figures / "oos_alpha_decay.png", dpi=180)
    plt.close(fig)
    print(json.dumps(summary, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
