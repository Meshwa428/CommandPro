#!/usr/bin/env python3
"""
One-off descriptive-statistics pass over Dataset/movements.jsonl, producing
the handful of constants RatModel (src/rat/rat_model.cpp) uses to shape its
physics-only trajectory generator. This is NOT model training — no weights,
no gradient descent — just stats baked into include/synapse/rat/rat_constants.h
so the C++ inference path has zero runtime dependency on this data.

Overshoot rate and curvature scale come directly from the dataset. Duration
(Fitts' law a/b) intentionally does NOT come from this dataset's raw
averages: AdSERP movements mix in browsing/reading pauses, so their absolute
timing (median ~1.7s for ~195px) is far slower than a deliberate reach and
would make default automation speed impractical. Duration uses standard
Fitts' law literature constants instead; only the *shape* parameters below
are dataset-derived.
"""
import json
import math
import statistics
from pathlib import Path

ROOT = Path(__file__).parent.parent.parent
DATA = ROOT / "Dataset" / "movements.jsonl"


def overshoot_rate(movements) -> float:
    n_overshoot = 0
    for d in movements:
        ex, ey = d["end"]
        dists = [math.hypot(p[0] - ex, p[1] - ey) for p in d["path"]]
        min_idx = dists.index(min(dists))
        if min_idx < len(dists) - 1 and dists[min_idx] < 5:
            n_overshoot += 1
    return n_overshoot / len(movements)


def curvature_scale(movements, sample_cap=1500) -> float:
    # Median absolute perpendicular deviation from the straight start->end
    # line, as a fraction of movement distance — a rough "how bowed is the
    # path" figure, used to scale RAT's sub-movement arc offset.
    devs_over_dist = []
    for d in movements[:sample_cap]:
        sx, sy = d["start"]; ex, ey = d["end"]
        dx, dy = ex - sx, ey - sy
        L = math.hypot(dx, dy)
        if L < 30:
            continue
        ux, uy = dx / L, dy / L
        for p in d["path"]:
            px, py = p[0] - sx, p[1] - sy
            perp = px * (-uy) + py * ux
            devs_over_dist.append(abs(perp) / L)
    return statistics.median(devs_over_dist)


def main():
    movements = [json.loads(line) for line in DATA.open()]
    ov = overshoot_rate(movements)
    curv = curvature_scale(movements)
    print(f"n movements           = {len(movements)}")
    print(f"overshoot rate        = {ov:.4f}")
    print(f"curvature scale (frac)= {curv:.4f}")
    print()
    print("Paste into include/synapse/rat/rat_constants.h:")
    print(f"constexpr double RAT_OVERSHOOT_RATE   = {ov:.3f};")
    print(f"constexpr double RAT_CURVATURE_SCALE  = {curv:.3f};")


if __name__ == "__main__":
    main()
