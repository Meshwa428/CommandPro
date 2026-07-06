"""RAT dataset pipeline — deviation-from-line + per-point dt (DMTG-inspired).

The old absolute (x,y,dt) target never converged: nothing pinned the endpoint, so
paths wandered off (curvature 7x real). The fix is structural, like DMTG's pinned
start/end nodes (Eq.3): model the trajectory as DEVIATION from the straight
start->target line, so the endpoints are 0 by construction and the path always
lands on target.

Each movement is resampled to a FIXED N points by SAMPLE INDEX (not uniform time),
so the real timing texture survives: the ~60 Hz clock, the start hesitation and
end settle, and the pauses. Per point we store 3 channels:

    dev_x = x - s_i        # deviation from the line along its own axis (s_i = i/(N-1))
    dev_y = y              # perpendicular curve
    dt    = ms since the previous point   (the human timing signature)

dev_x, dev_y are 0 at both ends (pinned). dt is a free channel; because points are
spaced by INDEX not time, dt varies point to point (small in fast bursts, large in
pauses / at the settle), which is what makes replay feel human instead of a smooth
glide. Total duration = sum(dt); at inference dt is rescaled to the requested time.

Conditioning:
    dist   px chord length            (log, standardized)
    alpha  canonical path length      (== DMTG complexity knob; 1=straight, higher=wigglier)
    persona participant id            (learned embedding = movement style)
"""
from __future__ import annotations
import json
import math
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[3]
JSONL = ROOT / "Dataset" / "movements.jsonl"

N = 64  # points per movement (fixed; smooth uniform-time shape, proven convergence)


def canonicalize(rec: dict):
    """Return (dev[N,2], dt_profile[N], D_ms, alpha, dist_px) or None.

    SHAPE and TIMING are represented separately (they are near-independent, and
    coupling them in one diffusion target made the shared net's dt noise pollute the
    geometry). Shape = uniform-time resample -> smooth deviation-from-line. Timing =
    a sum-to-1 profile of the real per-sample dt (the pauses/bursts), rescaled to any
    duration at generation.
    """
    path = np.asarray(rec["path"], dtype=np.float64)
    if path.shape[0] < 5:
        return None
    start = np.asarray(rec["start"], dtype=np.float64)
    end = np.asarray(rec["end"], dtype=np.float64)
    d = float(np.hypot(*(end - start)))
    if d < 20.0:  # sub-20px hops are click jitter, not aimed movement
        return None

    # canonical frame: start at origin, target rotated onto +x, scaled by distance
    xy = path[:, :2] - start
    ang = math.atan2(end[1] - start[1], end[0] - start[0])
    c, s = math.cos(-ang), math.sin(-ang)
    xy = (xy @ np.array([[c, -s], [s, c]]).T) / d      # target ~ (1,0)

    t = np.maximum.accumulate(path[:, 2].astype(np.float64))  # monotone ms
    t -= t[0]
    D = float(t[-1])
    if D <= 0:
        return None

    # SHAPE: resample to N points equally spaced in time (smooth, low-frequency)
    tq = np.linspace(0.0, D, N)
    rx = np.interp(tq, t, xy[:, 0])
    ry = np.interp(tq, t, xy[:, 1])
    s_line = np.linspace(0.0, 1.0, N)
    dev = np.stack([rx - s_line, ry], axis=1).astype(np.float32)
    dev[0] = 0.0; dev[-1] = 0.0
    if np.abs(dev).max() > 2.0:                        # pathological fling
        return None

    # TIMING: resample the real per-sample dt by INDEX (keeps pauses/bursts), sum-to-1
    M = xy.shape[0]
    rt = np.interp(np.linspace(0.0, M - 1, N), np.arange(M), t)
    dtp = np.diff(rt, prepend=rt[0])
    dt_profile = (dtp / dtp.sum()).astype(np.float32) if dtp.sum() > 0 else np.full(N, 1.0 / N, np.float32)

    seg = np.hypot(np.diff(xy[:, 0]), np.diff(xy[:, 1]))
    alpha = float(np.clip(seg.sum(), 1.0, 4.0))        # canonical path length = complexity
    return dev, dt_profile, D, alpha, d


def smooth(dev: np.ndarray, w: int = 5) -> np.ndarray:
    """Light low-pass on the deviation curves (hand motion is band-limited; diffusion
    leaves per-point jitter). dev: (...,N,2). Endpoints re-pinned to 0."""
    if w < 3:
        return dev
    k = np.hanning(w + 2)[1:-1]; k /= k.sum()
    flat = dev.reshape(-1, dev.shape[-2], 2)
    out = np.empty_like(flat)
    for i in range(len(flat)):
        for c in range(2):
            out[i, :, c] = np.convolve(flat[i, :, c], k, mode="same")
    out[:, 0] = 0.0; out[:, -1] = 0.0
    return out.reshape(dev.shape)


def load_dataset():
    devs, profs, Ds, alphas, dists, parts = [], [], [], [], [], []
    for line in open(JSONL):
        out = canonicalize(json.loads(line))
        if out is None:
            continue
        dev, prof, D, alpha, d = out
        devs.append(dev); profs.append(prof); Ds.append(D); alphas.append(alpha); dists.append(d)
        parts.append(json.loads(line)["participant"])

    participants = sorted(set(parts))
    pidx = {p: i for i, p in enumerate(participants)}
    return {
        "dev": np.asarray(devs, np.float32),               # (K, N, 2) dev_x,dev_y
        "dt_profile": np.asarray(profs, np.float32),       # (K, N) sum-to-1 timing shape
        "D": np.asarray(Ds, np.float32),                   # (K,) ms
        "alpha": np.asarray(alphas, np.float32),           # (K,)
        "dist": np.asarray(dists, np.float32),             # (K,)
        "persona": np.asarray([pidx[p] for p in parts], np.int64),
        "participants": participants,
    }


if __name__ == "__main__":
    ds = load_dataset()
    dev = ds["dev"]
    assert np.allclose(dev[:, 0], 0) and np.allclose(dev[:, -1], 0), "endpoints not pinned"
    prof = ds["dt_profile"]
    print(f"movements: {len(dev)}  personas: {len(ds['participants'])}  N={N}")
    print(f"dev_xy abs: mean={np.abs(dev).mean():.4f} max={np.abs(dev).max():.3f}")
    print(f"dt profile sums to 1: {np.allclose(prof.sum(1), 1.0)}")
    print(f"profile burstiness CV: med={np.median(prof.std(1)/prof.mean(1)):.2f}")
    print(f"alpha: med={np.median(ds['alpha']):.3f} p90={np.percentile(ds['alpha'],90):.3f}")
