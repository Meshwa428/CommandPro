"""RAT dataset pipeline — per-point (x, y, dt) representation.

Each real movement is a sequence of samples (x, y, t_ms) at IRREGULAR times —
that irregular timing (pauses, micro-hesitations, hardware jitter) is the human
signature, so we keep it instead of resampling to uniform time. The model emits,
per point, the ABSOLUTE canonical position AND the time since the previous point:

    step_i = (x_i, y_i, dt_i)

Absolute positions (not deltas) avoid cumulative random-walk drift — a small
per-step error no longer accumulates into a wandering path, and the endpoint
stays anchored near the target. Positions are in a canonical frame (start at
origin, target on +x, scaled so the target is at (1,0)); dt stays in real ms.
A path is just the positions; its timing is cumsum(dt). Sequences are padded to
MAX_LEN with a mask; paths longer than MAX_LEN are subsampled (endpoints kept).

Conditioning: distance (px), persona (participant id).
"""
from __future__ import annotations
import json
import math
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[3]
JSONL = ROOT / "Dataset" / "movements.jsonl"

MAX_LEN = 96  # steps per movement (median real length ~47; covers the bulk)


def _subsample(path: np.ndarray, max_pts: int) -> np.ndarray:
    """Keep <= max_pts points, endpoints included, uniform by index."""
    if path.shape[0] <= max_pts:
        return path
    idx = np.linspace(0, path.shape[0] - 1, max_pts).round().astype(int)
    return path[np.unique(idx)]


def canonicalize(rec: dict):
    """Return (steps[L,3]=(x,y,dt_ms), dist_px) or None. L <= MAX_LEN."""
    path = np.asarray(rec["path"], dtype=np.float64)
    if path.shape[0] < 3:
        return None
    start = np.asarray(rec["start"], dtype=np.float64)
    end = np.asarray(rec["end"], dtype=np.float64)
    d = float(np.hypot(*(end - start)))
    if d < 1.0:
        return None

    path = _subsample(path, MAX_LEN)          # keep <= MAX_LEN points

    xy = path[:, :2] - start                  # translate: start at origin
    ang = math.atan2(end[1] - start[1], end[0] - start[0])
    c, s = math.cos(-ang), math.sin(-ang)     # rotate target onto +x
    xy = xy @ np.array([[c, -s], [s, c]]).T
    xy /= d                                    # scale: target near (1,0)

    t = np.maximum.accumulate(path[:, 2])     # monotone time
    dt = np.diff(t, prepend=t[0])[:, None]    # (L,1) time since prev point, ms
    dt = np.clip(dt, 0.0, None)
    steps = np.concatenate([xy.astype(np.float32), dt.astype(np.float32)], axis=1)  # (L,3)
    if steps.shape[0] < 2:
        return None
    return steps, d


def load_dataset():
    steps, dists, parts = [], [], []
    for line in open(JSONL):
        rec = json.loads(line)
        out = canonicalize(rec)
        if out is None:
            continue
        s, d = out
        steps.append(s); dists.append(d); parts.append(rec["participant"])

    participants = sorted(set(parts))
    pidx = {p: i for i, p in enumerate(participants)}

    K = len(steps)
    padded = np.zeros((K, MAX_LEN, 3), np.float32)
    mask = np.zeros((K, MAX_LEN), np.float32)
    for i, s in enumerate(steps):
        L = min(len(s), MAX_LEN)
        padded[i, :L] = s[:L]
        mask[i, :L] = 1.0
    return {
        "steps": padded,                                   # (K, MAX_LEN, 3) dx,dy,dt
        "mask":  mask,                                     # (K, MAX_LEN)
        "dist":  np.asarray(dists, np.float32),            # (K,)
        "persona": np.asarray([pidx[p] for p in parts], np.int64),
        "participants": participants,
    }


if __name__ == "__main__":
    ds = load_dataset()
    st, mask = ds["steps"], ds["mask"]
    lens = mask.sum(1)
    dt = st[..., 2][mask > 0]
    print(f"movements: {len(st)}  personas: {len(ds['participants'])}")
    print(f"steps/move: min={int(lens.min())} med={int(np.median(lens))} max={int(lens.max())}")
    print(f"dt ms/step: min={dt.min():.1f} med={np.median(dt):.1f} max={dt.max():.1f} mean={dt.mean():.1f}")
    # endpoint should sit near (1,0); duration == real
    i = 0
    L = int(lens[i])
    print(f"move0 endpoint: ({st[i,L-1,0]:.3f}, {st[i,L-1,1]:.3f})  (expect ~1,0)")
    print(f"move0 duration: {st[i,:L,2].sum():.0f} ms over {L} points")
