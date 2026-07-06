"""Visualize the real dataset — 'record its real movement' before modelling.

Uses the per-point (dx,dy,dt) representation: paths reconstructed by cumulative
sum, velocity computed from the REAL per-step dt. Saves
tools/rat/ml/out/real_movements.png:
  * reconstructed canonical trajectories
  * true velocity profiles (speed = step / real dt) vs real elapsed time
  * dt distribution (the irregular timing / pauses the model must learn)
  * per-persona mean path (distinct styles -> session persona)
"""
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from data import load_dataset

OUT = Path(__file__).resolve().parent / "out"
OUT.mkdir(exist_ok=True)


def main():
    ds = load_dataset()
    steps, mask, persona, parts = ds["steps"], ds["mask"], ds["persona"], ds["participants"]
    rng = np.random.default_rng(0)

    fig, ax = plt.subplots(2, 2, figsize=(14, 10))

    # 1) Reconstructed canonical trajectories
    for i in rng.choice(len(steps), 120, replace=False):
        L = int(mask[i].sum())
        xy = steps[i, :L, :2]
        ax[0, 0].plot(xy[:, 0], xy[:, 1], color="steelblue", alpha=0.12, lw=1)
    ax[0, 0].scatter([0, 1], [0, 0], c="red", zorder=5, s=30)
    ax[0, 0].set_title("Real trajectories (canonical: start 0,0 -> target 1,0)")
    ax[0, 0].axhline(0, color="gray", lw=0.5); ax[0, 0].set_ylim(-0.5, 0.5)

    # 2) True velocity profiles from real dt
    for i in rng.choice(len(steps), 50, replace=False):
        L = int(mask[i].sum())
        dt = steps[i, :L, 2]
        xy = steps[i, :L, :2]
        disp = np.hypot(np.diff(xy[:, 0], prepend=xy[0, 0]), np.diff(xy[:, 1], prepend=xy[0, 1]))
        speed = disp / np.clip(dt, 1e-3, None)
        elapsed = np.cumsum(dt)
        ax[0, 1].plot(elapsed, speed, color="darkgreen", alpha=0.2, lw=1)
    ax[0, 1].set_title("True velocity profiles (speed vs real elapsed ms)")
    ax[0, 1].set_xlabel("elapsed ms"); ax[0, 1].set_ylabel("canonical speed / ms")
    ax[0, 1].set_xlim(0, 4000)

    # 3) dt distribution — the irregular timing
    dt_all = steps[..., 2][mask > 0]
    ax[1, 0].hist(np.clip(dt_all, 0, 200), bins=60, color="slateblue")
    ax[1, 0].set_title(f"Per-step dt (ms) — median {np.median(dt_all):.0f}, "
                       f"{(dt_all > 100).mean():.1%} are >100ms pauses")
    ax[1, 0].set_xlabel("dt ms (clipped at 200)")

    # 4) Per-persona mean path
    for p in rng.choice(len(parts), 8, replace=False):
        sel = np.where(persona == p)[0]
        paths = []
        for i in sel:
            L = int(mask[i].sum())
            xy = steps[i, :L, :2]
            # resample to 32 pts for averaging
            g = np.linspace(0, len(xy) - 1, 32).astype(int)
            paths.append(xy[g])
        m = np.mean(paths, axis=0)
        ax[1, 1].plot(m[:, 0], m[:, 1], lw=1.8, alpha=0.85, label=parts[p])
    ax[1, 1].set_title("Per-persona mean path (8 participants)")
    ax[1, 1].axhline(0, color="gray", lw=0.5); ax[1, 1].legend(fontsize=8)

    fig.tight_layout()
    fig.savefig(OUT / "real_movements.png", dpi=110)
    print(f"saved {OUT / 'real_movements.png'}")
    print(f"dt: median {np.median(dt_all):.0f}ms, pauses>100ms: {(dt_all>100).mean():.1%}, "
          f"zero-dt (fast bursts): {(dt_all==0).mean():.1%}")


if __name__ == "__main__":
    main()
