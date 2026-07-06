"""Inference API for the trained mouse model.

    generate(x0, y0, x1, y1, persona=..., ...) -> [(x, y, t_ms), ...]

Given a start and target (and optional conditioning), sample a trajectory from
the diffusion model and map it from the canonical frame back to real screen
coordinates with per-point absolute timestamps. This is the exact interface a
caller (and later the C++ runtime) uses.

Conditioning supported now: distance (from start/target), persona (user profile).
Planned: movement intent, screen size / DPI (add as extra conditioning inputs).
"""
from __future__ import annotations
import math
import os
from pathlib import Path
import numpy as np
import torch

from model import TrajDenoiser, Diffusion

torch.set_num_threads(os.cpu_count() or 4)
CKPT = Path(__file__).resolve().parent / "rat_diffusion.pt"


class MouseModel:
    def __init__(self, ckpt: Path = CKPT):
        ck = torch.load(ckpt, map_location="cpu", weights_only=False)
        self.model = TrajDenoiser(ck["n_personas"], ck["seq_len"],
                                  d_model=ck.get("d_model", 80), n_layers=ck.get("layers", 4))
        self.model.load_state_dict(ck["model"]); self.model.eval()
        self.diff = Diffusion(self.model, T=1000, device="cpu")
        self.norm = ck["norm"]
        self.seq_len = ck["seq_len"]
        self.n_personas = ck["n_personas"]
        self.participants = ck["participants"]

    def generate(self, x0, y0, x1, y1, persona: int = 0, ddim_steps: int = 50):
        """Return a list of (x, y, t_ms): real screen coords + absolute time."""
        d = math.hypot(x1 - x0, y1 - y0)
        if d < 1.0:
            return [(float(x1), float(y1), 0)]
        ang = math.atan2(y1 - y0, x1 - x0)

        dist_log = np.array([[(math.log(d) - self.norm["d_mean"]) / self.norm["d_std"]]], np.float32)
        p = torch.tensor([persona % self.n_personas])
        raw = self.diff.sample(torch.tensor(dist_log), p, self.seq_len, steps=ddim_steps)[0].numpy()

        # invert normalization -> canonical (x, y, dt_ms)
        ch_mean = np.asarray(self.norm["ch_mean"]); ch_std = np.asarray(self.norm["ch_std"])
        s = raw * ch_std + ch_mean
        cxy = s[:, :2]
        # dt: undo log1p; cap per-step to a sane max — expm1 amplifies tail
        # errors, and mid-movement pauses beyond ~0.5s aren't wanted for automation.
        dt = np.clip(np.expm1(np.clip(s[:, 2], None, 6.5)), 0.0, 500.0)

        # canonical -> real: scale by distance, rotate by +angle, translate to start
        c, si = math.cos(ang), math.sin(ang)
        R = np.array([[c, -si], [si, c]])
        real = (cxy * d) @ R.T + np.array([x0, y0])
        # monotone cumulative time; anchor exact landing on the target
        t = np.cumsum(dt)
        out = [(float(px), float(py), int(round(tt))) for (px, py), tt in zip(real, t)]
        out[-1] = (float(x1), float(y1), out[-1][2])
        return out


if __name__ == "__main__":
    m = MouseModel()
    path = m.generate(100, 100, 900, 600, persona=3)
    print(f"generated {len(path)} points, persona 3, {path[-1][2]} ms total")
    for p in path[:4]:
        print("  ", p)
    print("   ...")
    for p in path[-3:]:
        print("  ", p)
