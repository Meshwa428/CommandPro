#!/usr/bin/env python3
"""
RAT Dataset Preprocessor — AdSERP Mouse Tracking Data
======================================================
Converts AdSERP evtrack CSV files → movements.jsonl

Input  (one CSV per session):
  timestamp,xpos,ypos,event,xpath
  1671196817505,0,0,load,/
  1671196819583,1013,185,mousemove,...
  1671196821000,950,210,click,...

Output (movements.jsonl — one JSON object per movement segment):
  {
    "participant": "p004",  "block": "b1",  "task": "t10",
    "start": [x, y],       "end": [x, y],
    "path": [[x, y, t_ms_relative], ...],
    "duration_ms": 420,    "distance_px": 312.4,
    "angle_rad": 1.23,     "waypoints": 27,
    "endpoint_type": "click"
  }

Segmentation strategy:
  A movement segment = all mousemove rows between two intentional actions.
  Segments end at: click, mousedown, or scroll events.
  The endpoint coordinates become the segment's "end" position.
"""

import csv, json, math, sys
from pathlib import Path
from typing import Optional

# ── Paths ────────────────────────────────────────────────────────────────────
ROOT     = Path(__file__).parent.parent.parent
DATA_DIR = ROOT / "Dataset" / "mouse-movement-data" / "mouse-movement-data"
OUT_FILE = ROOT / "Dataset" / "movements.jsonl"
STATS    = ROOT / "Dataset" / "movements_stats.txt"

# ── Quality filters ──────────────────────────────────────────────────────────
MIN_DURATION_MS  = 50      # ignore sub-50ms twitches
MAX_DURATION_MS  = 8000    # ignore idle cursor sitting >8s
MIN_DISTANCE_PX  = 10      # ignore micro-movements <10px
MIN_WAYPOINTS    = 5       # need ≥5 points for a useful trajectory
MAX_SPEED_PX_MS  = 5.0     # >5px/ms is impossible for a human → hardware noise
# ─────────────────────────────────────────────────────────────────────────────


def parse_name(filename: str) -> tuple[str, str, str]:
    parts = Path(filename).stem.split("-")   # p004-b1-t10
    return parts[0], parts[1], parts[2]


def load_csv(path: Path) -> list[dict]:
    rows = []
    with open(path, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            try:
                rows.append({
                    "ts": int(row["timestamp"]),
                    "x":  float(row["xpos"]),
                    "y":  float(row["ypos"]),
                    "ev": row["event"].strip(),
                })
            except (ValueError, KeyError):
                continue
    return rows


def segment(rows: list[dict]) -> list[list[dict]]:
    """Split session events into movement segments at click/scroll boundaries."""
    segs, buf = [], []

    for r in rows:
        ev = r["ev"]

        if ev == "mousemove":
            if r["x"] == 0 and r["y"] == 0:   # skip (0,0) sentinel
                continue
            buf.append(r)

        elif ev in ("click", "mousedown") and r["x"] != 0:
            if buf:
                buf.append(r)       # click = final waypoint of this segment
                segs.append(buf)
            buf = []

        elif ev == "scroll":
            if len(buf) >= MIN_WAYPOINTS:
                segs.append(buf)
            buf = []

    if len(buf) >= MIN_WAYPOINTS:
        segs.append(buf)

    return segs


def quality_check(seg: list[dict]) -> Optional[dict]:
    """Return stats dict if segment passes quality filters, else None."""
    if len(seg) < MIN_WAYPOINTS:
        return None

    s, e = seg[0], seg[-1]
    duration_ms = e["ts"] - s["ts"]
    if not (MIN_DURATION_MS <= duration_ms <= MAX_DURATION_MS):
        return None

    dx, dy = e["x"] - s["x"], e["y"] - s["y"]
    dist = math.sqrt(dx*dx + dy*dy)
    if dist < MIN_DISTANCE_PX:
        return None
    if dist / duration_ms > MAX_SPEED_PX_MS:
        return None

    t0 = s["ts"]
    return {
        "start":         [s["x"], s["y"]],
        "end":           [e["x"], e["y"]],
        "path":          [[r["x"], r["y"], r["ts"] - t0] for r in seg],
        "duration_ms":   duration_ms,
        "distance_px":   round(dist, 2),
        "angle_rad":     round(math.atan2(dy, dx), 4),
        "waypoints":     len(seg),
        "endpoint_type": e["ev"],
    }


def process(path: Path) -> tuple[list[dict], dict]:
    p, b, t = parse_name(path.name)
    rows = load_csv(path)
    segs = segment(rows)

    kept, filtered, records = 0, 0, []
    for seg in segs:
        stats = quality_check(seg)
        if stats is None:
            filtered += 1
        else:
            records.append({"participant": p, "block": b, "task": t, **stats})
            kept += 1

    return records, {"file": path.name, "kept": kept, "filtered": filtered}


def main():
    files = sorted(DATA_DIR.glob("*.csv"))
    if not files:
        print(f"ERROR: no CSV files in {DATA_DIR}", file=sys.stderr)
        sys.exit(1)

    print(f"Files   : {len(files)}")
    print(f"Output  : {OUT_FILE}")
    print()

    total_kept = total_filtered = 0
    file_stats = []

    with open(OUT_FILE, "w") as f:
        for i, path in enumerate(files):
            records, fs = process(path)
            file_stats.append(fs)
            for r in records:
                f.write(json.dumps(r) + "\n")
            total_kept     += fs["kept"]
            total_filtered += fs["filtered"]
            if (i + 1) % 500 == 0 or (i + 1) == len(files):
                pct = (i+1) / len(files) * 100
                print(f"  [{i+1:4d}/{len(files)}] {pct:.0f}%  {total_kept:,} movements kept so far")

    print()
    print(f"✓ Done")
    print(f"  Movements kept    : {total_kept:,}")
    print(f"  Segments filtered : {total_filtered:,}")
    print(f"  Filter rate       : {total_filtered/(total_kept+total_filtered)*100:.1f}%")

    with open(STATS, "w") as sf:
        sf.write("RAT Preprocessing Stats\n=======================\n\n")
        sf.write(f"Movements kept    : {total_kept:,}\n")
        sf.write(f"Segments filtered : {total_filtered:,}\n\n")
        sf.write(f"Filters:\n")
        sf.write(f"  min_duration  : {MIN_DURATION_MS}ms\n")
        sf.write(f"  max_duration  : {MAX_DURATION_MS}ms\n")
        sf.write(f"  min_distance  : {MIN_DISTANCE_PX}px\n")
        sf.write(f"  min_waypoints : {MIN_WAYPOINTS}\n")
        sf.write(f"  max_speed     : {MAX_SPEED_PX_MS}px/ms\n\n")
        sf.write("Per-file:\n")
        for fs in file_stats:
            sf.write(f"  {fs['file']}: {fs['kept']} kept / {fs['kept']+fs['filtered']} raw\n")


if __name__ == "__main__":
    main()
