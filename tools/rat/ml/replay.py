"""Drive the REAL cursor with a model-generated trajectory (test harness).

Creates a virtual ABSOLUTE pointer via /dev/uinput and replays the path with its
real per-point timing. Absolute (tablet-style) positioning is used on purpose: it
bypasses libinput pointer acceleration, so the cursor traces the exact generated
path instead of a re-accelerated version of it.

Usage:
    python replay.py                         # diagonal across a 1920x1080 screen
    python replay.py --from 200 200 --to 1600 900 --persona 3 --alpha 1.4
    python replay.py --screen 2560 1440 --loop 3

Needs write access to /dev/uinput (you have it via the input-group ACL). If the
cursor doesn't move, the compositor may need a moment to bind the new device —
rerun once.
"""
from __future__ import annotations
import argparse
import subprocess
import time

import numpy as np
from evdev import UInput, AbsInfo, ecodes as e

from generate import MouseModel

ABS_MAX = 65535


def detect_screen():
    """Best-effort desktop size in px; falls back to 1920x1080."""
    try:  # Xwayland is up (DISPLAY set), xrandr reports the desktop size
        out = subprocess.check_output(["xrandr"], text=True, stderr=subprocess.DEVNULL)
        for ln in out.splitlines():
            if "*" in ln:  # active mode line, e.g. "   1920x1080     60.00*+"
                w, h = ln.split()[0].split("x")
                return int(w), int(h)
    except Exception:
        pass
    return 1920, 1080


def make_pointer():
    cap = {
        e.EV_KEY: [e.BTN_LEFT],
        e.EV_ABS: [(e.ABS_X, AbsInfo(0, 0, ABS_MAX, 0, 0, 0)),
                   (e.ABS_Y, AbsInfo(0, 0, ABS_MAX, 0, 0, 0))],
    }
    ui = UInput(cap, name="synapse-rat-test", version=0x3)
    time.sleep(0.8)  # let the compositor bind the device
    return ui


def densify(path, hz):
    """Cap the gap between emitted points at ~1/hz so a slow move doesn't teleport,
    WITHOUT globally resampling (that would erase the model's real dt texture — the
    pauses and big per-tick jumps). Only long dt gaps get filled; short human ticks
    pass through untouched."""
    if hz <= 0:
        return path
    max_gap = 1000.0 / hz
    out = [path[0]]
    for (x, y, t) in path[1:]:
        px, py, pt = out[-1]
        gap = t - pt
        if gap > max_gap * 1.5:
            n = int(gap / max_gap)
            for k in range(1, n):
                f = k / n
                out.append((px + (x - px) * f, py + (y - py) * f, pt + gap * f))
        out.append((x, y, t))
    return out


def replay(ui, path, W, H, hz=125):
    """path: [(x_px, y_px, t_ms)]. Traces the cursor along it, clock-scheduled.
    Uses the model's own per-point timing; only over-long gaps are subdivided."""
    frames = densify(path, hz)
    t0 = time.perf_counter()
    for x, y, t in frames:
        target = t0 + t / 1000.0
        now = time.perf_counter()
        if target > now:
            time.sleep(target - now)          # schedule against absolute clock (no drift)
        ui.write(e.EV_ABS, e.ABS_X, int(min(max(x / W, 0), 1) * ABS_MAX))
        ui.write(e.EV_ABS, e.ABS_Y, int(min(max(y / H, 0), 1) * ABS_MAX))
        ui.syn()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--from", dest="src", type=float, nargs=2, default=None)
    ap.add_argument("--to", dest="dst", type=float, nargs=2, default=None)
    ap.add_argument("--screen", type=int, nargs=2, default=None)
    ap.add_argument("--persona", type=int, default=0)
    ap.add_argument("--alpha", type=float, default=None)
    ap.add_argument("--duration", type=float, default=None,
                    help="total move time in ms; omit to use the Fitts estimate")
    ap.add_argument("--rate", type=int, default=60,
                    help="sample rate Hz: sets point count (duration*rate). ~60=real mouse, "
                         "higher=smoother, lower=snappier bigger jumps")
    ap.add_argument("--loop", type=int, default=1)
    args = ap.parse_args()

    W, H = args.screen or detect_screen()
    src = tuple(args.src) if args.src else (0.15 * W, 0.2 * H)
    dst = tuple(args.dst) if args.dst else (0.85 * W, 0.8 * H)
    print(f"screen {W}x{H}  {src} -> {dst}  persona={args.persona} alpha={args.alpha}")

    m = MouseModel()
    ui = make_pointer()
    try:
        for i in range(args.loop):
            path = m.generate(src[0], src[1], dst[0], dst[1],
                              persona=args.persona, alpha=args.alpha,
                              duration_ms=args.duration, rate_hz=args.rate)
            print(f"  run {i+1}: {len(path)} pts, {path[-1][2]} ms")
            print("  moving in 2s — watch the cursor...")
            time.sleep(2)
            replay(ui, path, W, H, hz=args.rate)
            src, dst = dst, src  # bounce back for the next loop
    finally:
        ui.close()
    print("done")


if __name__ == "__main__":
    main()
