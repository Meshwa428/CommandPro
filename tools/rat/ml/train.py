"""Train the deviation-from-line trajectory diffusion model (CPU, fast).

Targets: dev (dx,dy) per point, standardized per channel.
Conditioning: log-distance, alpha (complexity), persona.
Duration D is NOT diffused — it is a near-deterministic function of distance
(Fitts' law), so we fit log D ~ a*log(dist)+b once and store (a,b,sigma) in the
checkpoint; generation samples D from it. This keeps the diffusion model focused
on trajectory SHAPE and stays tiny.

Usage: python train.py [--epochs 400] [--batch 512]
"""
from __future__ import annotations
import argparse
import os
import time
from pathlib import Path
import numpy as np
import torch

from data import load_dataset, N
from model import TrajDenoiser, Diffusion, DtDenoiser, DtDiffusion

torch.set_num_threads(os.cpu_count() or 4)
CKPT = Path(__file__).resolve().parent / "rat_diffusion.pt"


def build_tensors(ds):
    dev = ds["dev"][..., :2].copy()                           # (K,N,2) spatial only
    # scale-only (no mean subtraction): endpoints are exactly 0 and must stay 0 through
    # denorm, else force-zeroing them creates a discontinuity/jitter.
    ch_mean = np.zeros(2, np.float32)
    ch_std = dev.reshape(-1, 2).std(0) + 1e-6
    dev = dev / ch_std

    dist_log = np.log(ds["dist"])
    d_mean, d_std = dist_log.mean(), dist_log.std() + 1e-6
    dist_n = ((dist_log - d_mean) / d_std).astype(np.float32)[:, None]

    a_mean, a_std = ds["alpha"].mean(), ds["alpha"].std() + 1e-6
    alpha_n = ((ds["alpha"] - a_mean) / a_std).astype(np.float32)[:, None]

    # timing-model targets: per-point dt (log1p, standardized) + duration conditioning
    dt_ms = ds["dt_profile"] * ds["D"][:, None]           # (K,N) absolute ms
    dt_log = np.log1p(dt_ms)
    dt_mean, dt_std = dt_log.mean(), dt_log.std() + 1e-6
    dt_t = ((dt_log - dt_mean) / dt_std).astype(np.float32)
    dur_log = np.log(ds["D"])
    dur_mean, dur_std = dur_log.mean(), dur_log.std() + 1e-6
    dur_n = ((dur_log - dur_mean) / dur_std).astype(np.float32)[:, None]

    # Fitts on MOTION time only (exclude pause bins), so generation can treat total
    # time = motion + idle. Fitting on raw D would fold pauses into "motion" and let
    # the natural move balloon to 10s+. Motion = sum of dt below the pause threshold.
    dt_ms = ds["dt_profile"] * ds["D"][:, None]            # (K,N) per-point ms
    motion_ms = np.where(dt_ms < 45.0, dt_ms, 0.0).sum(1).clip(50.0, None)
    logM = np.log(motion_ms)
    A = np.stack([dist_log, np.ones_like(dist_log)], 1)
    (fa, fb), *_ = np.linalg.lstsq(A, logM, rcond=None)
    sigma = float((logM - A @ [fa, fb]).std())

    norm = {
        "ch_mean": ch_mean.tolist(), "ch_std": ch_std.tolist(),
        "d_mean": float(d_mean), "d_std": float(d_std),
        "a_mean": float(a_mean), "a_std": float(a_std),
        "alpha_lo": float(ds["alpha"].min()), "alpha_hi": float(ds["alpha"].max()),
        "fitts_a": float(fa), "fitts_b": float(fb), "fitts_sigma": sigma,
        "dt_mean": float(dt_mean), "dt_std": float(dt_std),
        "dur_mean": float(dur_mean), "dur_std": float(dur_std),
        "dur_lo": float(dur_log.min()), "dur_hi": float(dur_log.max()),
    }
    return (torch.tensor(dev, dtype=torch.float32), torch.tensor(dist_n), torch.tensor(alpha_n),
            torch.tensor(ds["persona"]), torch.tensor(dt_t), torch.tensor(dur_n), norm)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--epochs", type=int, default=400)
    ap.add_argument("--batch", type=int, default=512)
    ap.add_argument("--lr", type=float, default=1e-3)
    ap.add_argument("--d_model", type=int, default=256)
    ap.add_argument("--layers", type=int, default=4)
    args = ap.parse_args()

    torch.manual_seed(0)
    ds = load_dataset()
    x, dist_n, alpha_n, persona, dt_t, dur_n, norm = build_tensors(ds)
    n_personas = len(ds["participants"])
    K = x.shape[0]
    print(f"train: {K} movements, {n_personas} personas, N={N}", flush=True)

    model = TrajDenoiser(n_personas, N, d_model=args.d_model, n_layers=args.layers)
    diff = Diffusion(model, T=1000, device="cpu")
    dt_model = DtDenoiser(n_personas, N)
    dt_diff = DtDiffusion(dt_model, T=1000, device="cpu")
    opt = torch.optim.AdamW(model.parameters(), lr=args.lr)
    dt_opt = torch.optim.AdamW(dt_model.parameters(), lr=args.lr)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, args.epochs)
    dt_sched = torch.optim.lr_scheduler.CosineAnnealingLR(dt_opt, args.epochs)
    n_params = sum(p.numel() for p in model.parameters()) + sum(p.numel() for p in dt_model.parameters())
    print(f"params: {n_params/1e6:.2f}M  threads: {torch.get_num_threads()}", flush=True)

    # dt profiles: real per-point timing, normalized to sum=1 (a "time shape").
    # Generation samples one of these and rescales it to the requested duration ->
    # genuine human pauses/bursts instead of a modeled (unpredictable) dt.
    dt_prof = ds["dt_profile"].astype(np.float32)
    meta = {"n_personas": n_personas, "seq_len": N, "d_model": args.d_model,
            "layers": args.layers, "norm": norm, "participants": ds["participants"],
            "dt_profiles": dt_prof, "dt_prof_dist": ds["dist"].astype(np.float32)}

    def save():
        torch.save({"model": model.state_dict(), "dt_model": dt_model.state_dict(), **meta}, CKPT)

    model.train(); dt_model.train()
    t0 = time.time()
    for ep in range(args.epochs):
        perm = torch.randperm(K)
        total = 0.0; dtot = 0.0; nb = 0
        for i in range(0, K, args.batch):
            idx = perm[i:i + args.batch]
            loss = diff.loss(x[idx], dist_n[idx], alpha_n[idx], persona[idx])
            opt.zero_grad(); loss.backward(); opt.step()
            dl = dt_diff.loss(dt_t[idx], dist_n[idx], alpha_n[idx], persona[idx], dur_n[idx])
            dt_opt.zero_grad(); dl.backward(); dt_opt.step()
            total += loss.item(); dtot += dl.item(); nb += 1
        sched.step(); dt_sched.step()
        if ep % 20 == 0 or ep == args.epochs - 1:
            print(f"epoch {ep:3d}  shape {total/nb:.4f}  dt {dtot/nb:.4f}  {time.time()-t0:5.1f}s", flush=True)
            save()

    save()
    print(f"saved {CKPT}  ({time.time()-t0:.1f}s total)", flush=True)


if __name__ == "__main__":
    main()
