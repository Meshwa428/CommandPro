"""Sample trajectories from the trained model and compare to real data.

Reconstructs canonical paths (line + deviation) for both generated and real
movements and reports the realism metrics: curvature, overshoot, velocity
peak-time, duration. Saves out/generated_vs_real.png.
"""
from __future__ import annotations
import os
from pathlib import Path
import numpy as np
import torch
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from data import load_dataset, smooth, N
from model import TrajDenoiser, Diffusion

torch.set_num_threads(os.cpu_count() or 4)
OUT = Path(__file__).resolve().parent
CKPT = OUT / "rat_diffusion.pt"


def load():
    ck = torch.load(CKPT, map_location="cpu", weights_only=False)
    model = TrajDenoiser(ck["n_personas"], ck["seq_len"], d_model=ck["d_model"], n_layers=ck["layers"])
    model.load_state_dict(ck["model"]); model.eval()
    return model, ck


def canon_from_dev(steps):
    """steps (B,N,3)=(dev_x,dev_y,dt) -> canonical xy (B,N,2)."""
    s = np.linspace(0.0, 1.0, steps.shape[1])
    xy = steps[..., :2].copy()
    xy[..., 0] += s
    return xy


def metrics(steps):
    """steps (K,N,3): dev_x,dev_y,dt. Speed uses the real per-point dt."""
    xy = canon_from_dev(steps)
    dt_all = steps[..., 2]
    curv, over, peakt, dt_cv = [], [], [], []
    for i in range(len(xy)):
        p = xy[i]; dt = np.clip(dt_all[i], 1e-3, None)
        end = p[-1]; d = np.hypot(*end)
        if d < 1e-3:
            continue
        ax = end / d
        proj = p @ ax
        perp = p[:, 0] * (-ax[1]) + p[:, 1] * ax[0]
        curv.append(np.abs(perp).max() / d)
        over.append(proj.max() > d * 1.02)
        disp = np.hypot(*np.diff(p, axis=0).T)
        speed = disp / dt[1:]
        peakt.append((np.argmax(speed) + 1) / (len(p) - 1))
        dt_cv.append(dt[1:].std() / dt[1:].mean())   # timing burstiness
    return {"curvature": float(np.median(curv)),
            "overshoot": float(np.mean(over)),
            "peak_time": float(np.median(peakt)),
            "dt_cv": float(np.median(dt_cv))}


def main():
    model, ck = load()
    diff = Diffusion(model, T=1000, device="cpu")
    ds = load_dataset()
    nm = ck["norm"]

    rng = np.random.default_rng(1)
    B = 400
    idx = rng.choice(len(ds["dist"]), B, replace=False)
    dist_px = ds["dist"][idx]
    dist_n = ((np.log(dist_px) - nm["d_mean"]) / nm["d_std"]).astype(np.float32)[:, None]
    alpha_px = ds["alpha"][idx]  # condition on the real movements' complexity (fair)
    alpha_n = ((alpha_px - nm["a_mean"]) / nm["a_std"]).astype(np.float32)[:, None]
    persona = torch.tensor(rng.integers(0, ck["n_personas"], B))

    dev = diff.sample(torch.tensor(dist_n), torch.tensor(alpha_n), persona, N, steps=50).numpy()
    dev = dev * np.asarray(nm["ch_std"]) + np.asarray(nm["ch_mean"])
    dev[:, 0] = 0.0; dev[:, -1] = 0.0
    dev = smooth(dev, w=5)
    # attach real dt profiles (as generate does) so timing metrics are meaningful
    prof = np.asarray(ck["dt_profiles"]); pick = rng.integers(0, len(prof), B)
    dt = prof[pick] * ds["D"][idx][:, None]                   # scale to Fitts-ish duration
    gen = np.concatenate([dev, dt[..., None]], axis=2)
    gen_xy = canon_from_dev(gen)

    real = np.concatenate([ds["dev"], (ds["dt_profile"] * ds["D"][:, None])[..., None]], axis=2)
    gm = metrics(gen)
    rm = metrics(real)
    print("             generated | real")
    for k in ["curvature", "overshoot", "peak_time", "dt_cv"]:
        print(f"  {k:13s} {gm[k]:9.3f} | {rm[k]:.3f}")

    real_xy = canon_from_dev(real)

    fig, ax = plt.subplots(1, 2, figsize=(14, 5))
    for i in range(min(80, B)):
        ax[0].plot(gen_xy[i, :, 0], gen_xy[i, :, 1], color="crimson", alpha=0.12, lw=1)
    ax[0].set_title("GENERATED"); ax[0].scatter([0, 1], [0, 0], c="black", s=30, zorder=5)
    ridx = rng.choice(len(real_xy), 80, replace=False)
    for i in ridx:
        ax[1].plot(real_xy[i, :, 0], real_xy[i, :, 1], color="steelblue", alpha=0.12, lw=1)
    ax[1].set_title("REAL"); ax[1].scatter([0, 1], [0, 0], c="black", s=30, zorder=5)
    for a in ax:
        a.set_xlim(-0.3, 1.3); a.set_ylim(-0.5, 0.5); a.axhline(0, color="gray", lw=0.5)
    (OUT / "out").mkdir(exist_ok=True)
    fig.tight_layout(); fig.savefig(OUT / "out" / "generated_vs_real.png", dpi=110)
    print(f"saved {OUT / 'out' / 'generated_vs_real.png'}")


if __name__ == "__main__":
    main()
