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

from model import TrajDenoiser, Diffusion, DtDenoiser, DtDiffusion
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
        self.dt_model = DtDenoiser(ck["n_personas"], ck["seq_len"])
        self.dt_model.load_state_dict(ck["dt_model"]); self.dt_model.eval()
        self.dt_diff = DtDiffusion(self.dt_model, T=1000, device="cpu")
        self.norm = ck["norm"]
        self.seq_len = ck["seq_len"]
        self.n_personas = ck["n_personas"]
        self.participants = ck["participants"]
        self.dt_profiles = np.asarray(ck["dt_profiles"])      # (K,N) sum-to-1 timing shapes
        self.dt_prof_dist = np.asarray(ck["dt_prof_dist"])    # (K,) their move distances

    def generate(self, x0, y0, x1, y1, persona: int = 0, alpha: float | None = None,
                 duration_ms: float | None = None, rate_hz: float = 60.0,
                 ddim_steps: int = 50, seed=None):
        """Return a list of (x, y, t_ms): real screen coords + absolute time.

        duration_ms is the TOTAL time, respected exactly. The TIMING MODEL predicts the
        per-point dt for this duration — it was trained conditioned on duration, and the
        data shows longer moves spend proportionally more time paused (pause-fraction
        0.22 -> 0.64 as duration grows), so the model distributes pauses and slow/fast
        motion across the whole budget instead of moving fast then freezing. None ->
        natural Fitts duration.

        Point count is not fixed: output points = duration * rate_hz. rate_hz ~60 = a
        real mouse's polling; raise for smoother, lower for snappier bigger jumps.
        """
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

        # total duration: Fitts natural time unless the caller sets it
        if duration_ms is None:
            duration_ms = math.exp(nm["fitts_a"] * math.log(d) + nm["fitts_b"]
                                   + nm["fitts_sigma"] * float(np.random.randn()))
        duration_ms = float(duration_ms)

        # TIMING MODEL: predict the per-point dt profile for THIS duration. Trained
        # conditioned on duration, it distributes pauses/motion across the whole budget.
        dur_log = np.array([[(math.log(duration_ms) - nm["dur_mean"]) / nm["dur_std"]]], np.float32)
        dtp = self.dt_diff.sample(torch.tensor(dist_log), torch.tensor(alpha_n), p,
                                  torch.tensor(dur_log), self.seq_len, steps=ddim_steps)[0].numpy()
        dtp = np.expm1(dtp * nm["dt_std"] + nm["dt_mean"])  # back to ms
        dtp = np.clip(dtp, 0.0, None); dtp[0] = 0.0
        prof = dtp / dtp.sum() if dtp.sum() > 1e-6 else np.full(self.seq_len, 1.0 / self.seq_len)
        t = np.cumsum(prof) * duration_ms                   # timestamp per shape node

        # resample to n = duration*rate points, equally spaced in time. Fast bursts ->
        # big jumps (snap). Big-dt profile bins (pauses) -> HOLD position (dead stop).
        n = max(8, int(round(duration_ms / 1000.0 * rate_hz)))
        frame = 1000.0 / rate_hz
        pause_thresh = max(3.0 * frame, 40.0)
        tq = np.linspace(0.0, duration_ms, n)
        idx = np.clip(np.searchsorted(t, tq, side="right") - 1, 0, self.seq_len - 2)
        seg_dt = t[idx + 1] - t[idx]
        frac = np.where(seg_dt > 1e-6, (tq - t[idx]) / seg_dt, 0.0)
        frac = np.where(seg_dt > pause_thresh, 0.0, frac)
        xr = real[idx, 0] + frac * (real[idx + 1, 0] - real[idx, 0])
        yr = real[idx, 1] + frac * (real[idx + 1, 1] - real[idx, 1])

        out = [(float(px), float(py), int(round(tt))) for px, py, tt in zip(xr, yr, tq)]
        out[-1] = (float(x1), float(y1), int(round(duration_ms)))
        return out


if __name__ == "__main__":
    import numpy as np
    m = MouseModel()
    # total-time budget: motion is ~fixed, extra time becomes more/longer rests
    for dur in (None, 1500, 5000, 10000):
        np.random.seed(0)
        path = m.generate(100, 100, 900, 600, persona=3, alpha=1.3, duration_ms=dur, seed=0)
        xy = np.array([(p[0], p[1]) for p in path]); seg = np.hypot(*np.diff(xy, axis=0).T)
        # count distinct rest runs (consecutive near-still frames)
        still = seg < 0.6; rests = 0; cur = 0; held = 0
        for s in still:
            if s: cur += 1; held += 1
            elif cur: rests += 1 if cur > 3 else 0; cur = 0
        tot = path[-1][2]
        print(f"budget={str(dur):>5}ms -> total={tot:5d}ms, {len(path):4d} pts, "
              f"{rests} rests, {held*1000//60}ms idle, end=({path[-1][0]:.0f},{path[-1][1]:.0f})")
