"""Train the trajectory diffusion model on the real dataset (CPU).

Normalization (stored in the checkpoint so sampling can invert it):
  dx, dy : standardized
  dt     : log1p then standardized (heavy tail: 0..7500ms)
  dist   : log-px, standardized -> conditioning input

Usage: python train.py [--epochs 300] [--batch 128]
"""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import numpy as np
import torch

from data import load_dataset, MAX_LEN
from model import TrajDenoiser, Diffusion

torch.set_num_threads(os.cpu_count() or 4)

OUT = Path(__file__).resolve().parent
CKPT = OUT / "rat_diffusion.pt"


def build_tensors(ds):
    steps = ds["steps"].copy()                       # (K,L,3) dx,dy,dt
    mask = ds["mask"]
    steps[..., 2] = np.log1p(steps[..., 2])          # dt -> log ms

    valid = mask > 0
    ch_mean = np.zeros(3, np.float32); ch_std = np.ones(3, np.float32)
    for c in range(3):
        v = steps[..., c][valid]
        ch_mean[c], ch_std[c] = v.mean(), v.std() + 1e-6
        steps[..., c] = (steps[..., c] - ch_mean[c]) / ch_std[c]
    steps *= mask[..., None]                          # keep padding at 0

    dist_log = np.log(ds["dist"])
    d_mean, d_std = dist_log.mean(), dist_log.std() + 1e-6
    dist_log = ((dist_log - d_mean) / d_std).astype(np.float32)[:, None]

    norm = {"ch_mean": ch_mean, "ch_std": ch_std, "d_mean": float(d_mean), "d_std": float(d_std)}
    return (torch.tensor(steps), torch.tensor(mask),
            torch.tensor(dist_log), torch.tensor(ds["persona"]), norm)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--epochs", type=int, default=150)
    ap.add_argument("--batch", type=int, default=256)
    ap.add_argument("--lr", type=float, default=3e-4)
    ap.add_argument("--d_model", type=int, default=80)
    ap.add_argument("--layers", type=int, default=4)
    args = ap.parse_args()

    torch.manual_seed(0)
    ds = load_dataset()
    x, mask, dist_log, persona, norm = build_tensors(ds)
    n_personas = len(ds["participants"])
    K = x.shape[0]
    print(f"train: {K} movements, {n_personas} personas, seq_len {MAX_LEN}", flush=True)

    model = TrajDenoiser(n_personas, MAX_LEN, d_model=args.d_model, n_layers=args.layers)
    diff = Diffusion(model, T=1000, device="cpu")
    opt = torch.optim.AdamW(model.parameters(), lr=args.lr)
    n_params = sum(p.numel() for p in model.parameters())
    print(f"model params: {n_params/1e6:.2f}M  threads: {torch.get_num_threads()}", flush=True)

    meta = {
        "n_personas": n_personas, "seq_len": MAX_LEN,
        "d_model": args.d_model, "layers": args.layers,
        "norm": {k: (v.tolist() if isinstance(v, np.ndarray) else v) for k, v in norm.items()},
        "participants": ds["participants"],
    }

    model.train()
    for ep in range(args.epochs):
        perm = torch.randperm(K)
        total = 0.0; nb = 0
        for i in range(0, K, args.batch):
            idx = perm[i:i + args.batch]
            loss = diff.loss(x[idx], dist_log[idx], persona[idx], mask[idx])
            opt.zero_grad(); loss.backward(); opt.step()
            total += loss.item(); nb += 1
        if ep % 5 == 0 or ep == args.epochs - 1:
            print(f"epoch {ep:3d}  loss {total/nb:.4f}", flush=True)
        if ep % 20 == 0 or ep == args.epochs - 1:  # checkpoint periodically
            torch.save({"model": model.state_dict(), **meta}, CKPT)

    torch.save({"model": model.state_dict(), **meta}, CKPT)
    print(f"saved {CKPT}", flush=True)


if __name__ == "__main__":
    main()
