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
                 duration_ms: float | None = None, rate_hz: float = 60.0,
                 rest_gap_ms: float = 2500.0, ddim_steps: int = 50, seed=None):
        """Return a list of (x, y, t_ms): real screen coords + absolute time.

        duration_ms is the TOTAL time BUDGET (motion + idle), respected exactly. The
        aimed motion itself takes its natural Fitts time (physics — you can't glide
        800px continuously for 10s); any leftover slack is filled with REST pauses
        whose count scales with the slack (~one per rest_gap_ms). So a bigger budget
        yields more/longer rests: the amount of idling is predictable from the budget
        even though where each rest lands is random. None -> natural Fitts time, no slack.

        Point count is not fixed: output points = total_time * rate_hz. rate_hz ~60 =
        a real mouse's polling; raise for smoother, lower for snappier bigger jumps.
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

        # natural aimed-motion time (Fitts), independent of the budget
        t_move = math.exp(nm["fitts_a"] * math.log(d) + nm["fitts_b"]
                          + nm["fitts_sigma"] * float(np.random.randn()))
        budget = t_move if duration_ms is None else float(duration_ms)
        t_move = min(t_move, budget)                        # tight budget -> move faster
        slack = max(0.0, budget - t_move)

        # timing: real dt profile (pauses/bursts) from a similar-distance move, over t_move
        near = np.argsort(np.abs(np.log(self.dt_prof_dist) - math.log(d)))[:64]
        prof = self.dt_profiles[np.random.choice(near)]     # sums to 1
        t = np.cumsum(prof) * t_move                        # timestamp per shape node

        # resample motion to n = t_move*rate points, equally spaced in time. Fast bursts
        # -> big jumps (snap). A big-dt profile bin (a real micro-pause) -> HOLD position
        # across those frames (dead stop), not a slow slide.
        n = max(8, int(round(t_move / 1000.0 * rate_hz)))
        frame = 1000.0 / rate_hz
        pause_thresh = max(3.0 * frame, 40.0)
        tq = np.linspace(0.0, t_move, n)
        idx = np.clip(np.searchsorted(t, tq, side="right") - 1, 0, self.seq_len - 2)
        seg_dt = t[idx + 1] - t[idx]
        frac = np.where(seg_dt > 1e-6, (tq - t[idx]) / seg_dt, 0.0)
        frac = np.where(seg_dt > pause_thresh, 0.0, frac)
        xr = real[idx, 0] + frac * (real[idx + 1, 0] - real[idx, 0])
        yr = real[idx, 1] + frac * (real[idx + 1, 1] - real[idx, 1])
        motion = [(float(px), float(py), tt) for px, py, tt in zip(xr, yr, tq)]
        motion[-1] = (float(x1), float(y1), t_move)

        # fill the slack with rest pauses (count scales with slack) -> total == budget
        out = self._add_rests(motion, slack, rate_hz, rest_gap_ms)
        return [(px, py, int(round(tt))) for px, py, tt in out]

    @staticmethod
    def _add_rests(pts, slack_ms, rate_hz, rest_gap_ms):
        """Insert dead-stop rests summing to slack_ms at random points. Rest COUNT
        scales with slack (~one per rest_gap_ms); placement/durations are random."""
        if slack_ms < 60.0 or len(pts) < 4:
            if slack_ms >= 60.0:                            # too short to place -> hold at end
                x, y, t = pts[-1]
                pts = pts[:-1] + [(x, y, t + slack_ms)]
            return pts
        frame = 1000.0 / rate_hz
        lo, hi = int(len(pts) * 0.1), int(len(pts) * 0.9)
        n = max(1, int(np.random.poisson(slack_ms / max(rest_gap_ms, 200.0))))
        n = min(n, hi - lo)
        durs = np.diff(np.concatenate([[0.0], np.sort(np.random.uniform(0, slack_ms, n - 1)), [slack_ms]]))
        locs = sorted(np.random.choice(range(lo, hi), size=n, replace=False))
        loc_dur = dict(zip(locs, durs))
        out, shift = [], 0.0
        for i, (x, y, t) in enumerate(pts):
            nt = t + shift
            out.append((x, y, nt))
            if i in loc_dur:
                hold = loc_dur[i]
                for j in range(1, max(1, int(hold / frame)) + 1):
                    out.append((x, y, nt + frame * j))
                shift += hold
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
