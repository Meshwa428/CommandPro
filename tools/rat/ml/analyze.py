"""Timing/physics analysis of the raw movement dataset (pre-resampling).

Shows the real sensor behaviour the model must reproduce: the ~60 Hz sample clock,
the pause/hesitation tail, and speed carried as variable pixel-jump-per-tick.
Saves out/dataset_timing.png.
"""
import json
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

JSONL = Path(__file__).resolve().parents[3] / "Dataset" / "movements.jsonl"


def main():
    nsamp, durations, firstdt = [], [], []
    all_dt, jump_px = [], []
    dt_by_pos = [[] for _ in range(10)]   # dt vs normalized position in move
    example = None
    for line in open(JSONL):
        r = json.loads(line)
        p = np.asarray(r["path"], float)
        if len(p) < 8:
            continue
        t = p[:, 2]; xy = p[:, :2]
        dt = np.diff(t)
        if (dt < 0).any():
            continue
        nsamp.append(len(p)); durations.append(t[-1] - t[0]); firstdt.append(dt[0])
        all_dt.append(dt)
        jump_px.append(np.hypot(*np.diff(xy, axis=0).T))
        pos = np.linspace(0, 1, len(dt))
        for j, dv in zip((pos * 10).astype(int).clip(0, 9), dt):
            dt_by_pos[j].append(dv)
        if example is None and 40 < len(p) < 90 and (t[-1] - t[0]) > 1000:
            example = (t - t[0], xy, dt)

    all_dt = np.concatenate(all_dt); jump_px = np.concatenate(jump_px)
    fig, ax = plt.subplots(2, 3, figsize=(18, 9))

    ax[0, 0].hist(np.clip(all_dt, 0, 100), bins=100, color="steelblue")
    ax[0, 0].set_title("inter-sample dt (ms) — spike at ~16-17ms = 60Hz clock")
    ax[0, 0].set_xlabel("dt ms"); ax[0, 0].axvline(16.7, color="crimson", lw=1)

    ax[0, 1].hist(np.log10(np.clip(all_dt, 1, None)), bins=80, color="steelblue")
    ax[0, 1].set_title("log10(dt) — right tail = pauses (>50ms)")
    ax[0, 1].set_xlabel("log10 dt")

    ax[0, 2].hist(nsamp, bins=60, range=(0, 250), color="seagreen")
    ax[0, 2].set_title(f"samples per move (med {int(np.median(nsamp))})")
    ax[0, 2].set_xlabel("n samples")

    meds = [np.median(x) for x in dt_by_pos]; p90 = [np.percentile(x, 90) for x in dt_by_pos]
    ax[1, 0].plot(np.linspace(0, 1, 10), meds, "o-", label="median")
    ax[1, 0].plot(np.linspace(0, 1, 10), p90, "s--", label="p90")
    ax[1, 0].set_title("dt vs position in move (start hesitation, end settle)")
    ax[1, 0].set_xlabel("normalized position"); ax[1, 0].set_ylabel("dt ms"); ax[1, 0].legend()

    ax[1, 1].hist(np.clip(jump_px, 0, 60), bins=80, color="indianred")
    ax[1, 1].set_title(f"pixel jump per tick (med {np.median(jump_px):.1f}, max carries the 'snap')")
    ax[1, 1].set_xlabel("px between consecutive samples")

    if example is not None:
        et, exy, edt = example
        seg = np.hypot(*np.diff(exy, axis=0).T)
        ax[1, 2].plot(et[1:], seg, color="darkorange")
        ax[1, 2].set_title("one real move: px/tick vs time (bursty, not constant)")
        ax[1, 2].set_xlabel("ms"); ax[1, 2].set_ylabel("px this tick")

    (Path(__file__).resolve().parent / "out").mkdir(exist_ok=True)
    fig.tight_layout()
    out = Path(__file__).resolve().parent / "out" / "dataset_timing.png"
    fig.savefig(out, dpi=100)
    print("saved", out)
    print(f"samples/move med={int(np.median(nsamp))}  dt med={np.median(all_dt):.0f} mean={all_dt.mean():.1f}")
    print(f"pauses dt>50ms: {100*np.mean(all_dt>50):.1f}%   px/tick med={np.median(jump_px):.1f} p99={np.percentile(jump_px,99):.0f}")


if __name__ == "__main__":
    main()
