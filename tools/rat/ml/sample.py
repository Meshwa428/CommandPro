"""Sample trajectories from the trained diffusion model and compare to real.

Loads the checkpoint, generates (dx,dy,dt) sequences for a spread of
distances/personas, inverts normalization, reconstructs path + timing, and
saves tools/rat/ml/out/generated_vs_real.png plus a metrics printout comparing
generated vs real on curvature, overshoot, velocity-peak timing and dt.
"""
from __future__ import annotations
import os
from pathlib import Path
import numpy as np
import torch
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from data import load_dataset, MAX_LEN
from model import TrajDenoiser, Diffusion

torch.set_num_threads(os.cpu_count() or 4)
OUT = Path(__file__).resolve().parent
CKPT = OUT / "rat_diffusion.pt"


def load():
    ck = torch.load(CKPT, map_location="cpu", weights_only=False)
    model = TrajDenoiser(ck["n_personas"], ck["seq_len"],
                         d_model=ck.get("d_model", 80), n_layers=ck.get("layers", 4))
    model.load_state_dict(ck["model"]); model.eval()
    return model, ck


def denorm(x, norm):
    """x: (B,L,3) normalized -> (dx,dy,dt_ms)."""
    ch_mean = np.asarray(norm["ch_mean"]); ch_std = np.asarray(norm["ch_std"])
    x = x * ch_std + ch_mean
    x[..., 2] = np.expm1(np.clip(x[..., 2], None, 9.0))  # dt back to ms, cap
    x[..., 2] = np.clip(x[..., 2], 0.0, None)
    return x


def metrics(steps, mask):
    """steps: (K,L,3), mask: (K,L). Returns dict of realism stats."""
    curv, over, peakt, dtmed = [], [], [], []
    for i in range(len(steps)):
        L = int(mask[i].sum()) if mask is not None else len(steps[i])
        if L < 3:
            continue
        s = steps[i, :L]
        xy = s[:, :2]
        end = xy[-1]
        d = np.hypot(*end)
        if d < 1e-3:
            continue
        ax = end / d
        proj = xy @ ax
        perp = xy[:, 0] * (-ax[1]) + xy[:, 1] * ax[0]
        curv.append(np.abs(perp).max() / d)
        over.append(proj.max() > d * 1.02)
        dt = np.clip(s[:, 2], 1e-3, None)
        disp = np.hypot(np.diff(xy[:, 0], prepend=xy[0, 0]),
                        np.diff(xy[:, 1], prepend=xy[0, 1]))  # per-step displacement
        speed = disp / dt
        peakt.append(np.cumsum(dt)[np.argmax(speed)] / dt.sum())
        dtmed.append(np.median(s[:, 2]))
    return {
        "curvature": float(np.median(curv)),
        "overshoot": float(np.mean(over)),
        "peak_time": float(np.median(peakt)),
        "dt_median_ms": float(np.median(dtmed)),
    }


def main():
    model, ck = load()
    diff = Diffusion(model, T=1000, device="cpu")
    ds = load_dataset()
    norm = ck["norm"]

    # Condition on a spread of real distances, a few fixed personas.
    rng = np.random.default_rng(1)
    B = 200
    real_idx = rng.choice(len(ds["dist"]), B, replace=False)
    dist_px = ds["dist"][real_idx]
    dist_log = ((np.log(dist_px) - norm["d_mean"]) / norm["d_std"]).astype(np.float32)[:, None]
    persona = torch.tensor(rng.integers(0, ck["n_personas"], B))

    gen = diff.sample(torch.tensor(dist_log), persona, ck["seq_len"], steps=50).numpy()
    gen = denorm(gen, norm)
    gmask = np.ones((B, ck["seq_len"]), np.float32)

    gm = metrics(gen, gmask)
    rm = metrics(ds["steps"], ds["mask"])
    print("            generated | real")
    for k in ["curvature", "overshoot", "peak_time", "dt_median_ms"]:
        print(f"  {k:13s} {gm[k]:8.3f} | {rm[k]:.3f}")

    # Plot generated vs real
    fig, ax = plt.subplots(1, 3, figsize=(18, 5))
    for i in range(min(60, B)):
        xy = gen[i, :, :2]
        ax[0].plot(xy[:, 0], xy[:, 1], color="crimson", alpha=0.15, lw=1)
    ax[0].scatter([0, 1], [0, 0], c="black", s=30, zorder=5)
    ax[0].set_title("GENERATED trajectories"); ax[0].set_ylim(-0.5, 0.5); ax[0].axhline(0, color="gray", lw=0.5)

    ridx = rng.choice(len(ds["steps"]), 60, replace=False)
    for i in ridx:
        L = int(ds["mask"][i].sum())
        xy = ds["steps"][i, :L, :2]
        ax[1].plot(xy[:, 0], xy[:, 1], color="steelblue", alpha=0.15, lw=1)
    ax[1].scatter([0, 1], [0, 0], c="black", s=30, zorder=5)
    ax[1].set_title("REAL trajectories"); ax[1].set_ylim(-0.5, 0.5); ax[1].axhline(0, color="gray", lw=0.5)

    # velocity profiles overlay (speed = per-step displacement / dt)
    for i in range(min(40, B)):
        xy = gen[i, :, :2]; dt = np.clip(gen[i, :, 2], 1e-3, None)
        disp = np.hypot(np.diff(xy[:, 0], prepend=xy[0, 0]), np.diff(xy[:, 1], prepend=xy[0, 1]))
        ax[2].plot(np.cumsum(dt), disp / dt, color="crimson", alpha=0.15, lw=1)
    ax[2].set_title("GENERATED velocity (speed vs elapsed ms)"); ax[2].set_xlim(0, 4000)

    fig.tight_layout(); fig.savefig(OUT / "out" / "generated_vs_real.png", dpi=110)
    (OUT / "out").mkdir(exist_ok=True)
    print(f"saved {OUT / 'out' / 'generated_vs_real.png'}")


if __name__ == "__main__":
    main()
