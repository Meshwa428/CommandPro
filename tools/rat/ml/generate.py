"""Inference API for the trained mouse model.

    generate(x0,y0, x1,y1, persona=id, alpha=None, ddim_steps=50) -> [(x,y,t_ms), ...]

Sample a deviation-from-line trajectory from the diffusion model, add it to the
straight start->target line, and map back to real screen coordinates with per-point
timestamps. Endpoints are pinned, so the path always starts at the source and lands
exactly on the target.

Conditioning:
  * distance     : from start/target (Fitts also sets the duration)
  * alpha        : complexity / movement intent. None -> sample from the training
                   range (low = decisive/straight, high = hesitant/wandering).
  * persona      : user movement style (embedding; per-user calibration hooks here).

Duration comes from the stored Fitts fit (log D = a*log(dist)+b + noise); override
`duration_ms` to use an external calibration (e.g. the C++ rat's per-user Fitts).
"""
from __future__ import annotations
import math
import os
from pathlib import Path
import numpy as np
import torch

from model import TrajDenoiser, Diffusion
from data import smooth, N

torch.set_num_threads(os.cpu_count() or 4)
CKPT = Path(__file__).resolve().parent / "rat_diffusion.pt"


class MouseModel:
    def __init__(self, ckpt: Path = CKPT):
        ck = torch.load(ckpt, map_location="cpu", weights_only=False)
        self.model = TrajDenoiser(ck["n_personas"], ck["seq_len"],
                                  d_model=ck["d_model"], n_layers=ck["layers"])
        self.model.load_state_dict(ck["model"]); self.model.eval()
        self.diff = Diffusion(self.model, T=1000, device="cpu")
        self.norm = ck["norm"]
        self.seq_len = ck["seq_len"]
        self.n_personas = ck["n_personas"]
        self.participants = ck["participants"]
        self.dt_profiles = np.asarray(ck["dt_profiles"])      # (K,N) sum-to-1 timing shapes
        self.dt_prof_dist = np.asarray(ck["dt_prof_dist"])    # (K,) their move distances

    def generate(self, x0, y0, x1, y1, persona: int = 0, alpha: float | None = None,
                 duration_ms: float | None = None, ddim_steps: int = 50, seed=None):
        """Return a list of (x, y, t_ms): real screen coords + absolute time."""
        d = math.hypot(x1 - x0, y1 - y0)
        if d < 1.0:
            return [(float(x1), float(y1), 0)]
        if seed is not None:
            torch.manual_seed(seed)
        nm = self.norm
        ang = math.atan2(y1 - y0, x1 - x0)

        if alpha is None:  # sample complexity from the training range
            alpha = float(np.random.uniform(nm["alpha_lo"], min(nm["alpha_hi"], 2.5)))
        dist_log = np.array([[(math.log(d) - nm["d_mean"]) / nm["d_std"]]], np.float32)
        alpha_n = np.array([[(alpha - nm["a_mean"]) / nm["a_std"]]], np.float32)
        p = torch.tensor([persona % self.n_personas])

        dev = self.diff.sample(torch.tensor(dist_log), torch.tensor(alpha_n), p,
                               self.seq_len, steps=ddim_steps)[0].numpy()
        dev = dev * np.asarray(nm["ch_std"]) + np.asarray(nm["ch_mean"])
        dev[0] = 0.0; dev[-1] = 0.0
        dev = smooth(dev[None], w=5)[0]

        # canonical point = straight line + deviation
        s = np.linspace(0.0, 1.0, self.seq_len)
        cxy = np.stack([s + dev[:, 0], dev[:, 1]], axis=1)

        # canonical -> real: scale by distance, rotate, translate to start
        c, si = math.cos(ang), math.sin(ang)
        real = (cxy * d) @ np.array([[c, -si], [si, c]]).T + np.array([x0, y0])

        # timing: pick a real dt profile (pauses/bursts) from a similar-distance move,
        # rescale it to the requested/Fitts total. This is genuine human timing texture.
        if duration_ms is None:
            logD = nm["fitts_a"] * math.log(d) + nm["fitts_b"] + nm["fitts_sigma"] * float(np.random.randn())
            duration_ms = math.exp(logD)
        near = np.argsort(np.abs(np.log(self.dt_prof_dist) - math.log(d)))[:64]
        prof = self.dt_profiles[np.random.choice(near)]     # sums to 1
        t = np.cumsum(prof) * duration_ms

        out = [(float(px), float(py), int(round(tt))) for (px, py), tt in zip(real, t)]
        out[-1] = (float(x1), float(y1), int(round(duration_ms)))
        return out


if __name__ == "__main__":
    m = MouseModel()
    for a in (1.1, 1.8):
        path = m.generate(100, 100, 900, 600, persona=3, alpha=a, seed=0)
        xy = np.array([(p[0], p[1]) for p in path])
        seg = np.hypot(*np.diff(xy, axis=0).T).sum()
        chord = math.hypot(800, 500)
        print(f"alpha={a}: {len(path)} pts, {path[-1][2]} ms, pathlen/chord={seg/chord:.2f}, "
              f"end=({path[-1][0]:.0f},{path[-1][1]:.0f}) target=(900,600)")
