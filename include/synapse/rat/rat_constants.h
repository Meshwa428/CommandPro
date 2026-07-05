#pragma once

// Calibration constants for RatModel (src/rat/rat_model.cpp), computed once
// offline by tools/rat/calibrate_physics.py from Dataset/movements.jsonl
// (6,715 real human mouse movements). Not model weights — descriptive
// statistics baked in so the C++ inference path has zero runtime file I/O
// and zero dependency on the dataset. Re-run the script and paste new
// values here if the dataset changes; nothing else needs updating.
//
// Duration (Fitts' law a/b) intentionally does NOT come from this dataset —
// see calibrate_physics.py's docstring for why — and uses standard
// Fitts'-law-literature constants instead.

namespace syn {

// Fraction of movements that overshoot the target and correct back (design
// 005 §2.6 cites ~35% in the literature; dataset measures 39%).
constexpr double RAT_OVERSHOOT_RATE  = 0.391;

// Median |perpendicular deviation from the straight line| as a fraction of
// movement distance — how bowed a real path is. Scales RAT's sub-movement
// arc offset.
constexpr double RAT_CURVATURE_SCALE = 0.056;

// Fitts' law: duration_ms = FITTS_A_MS + FITTS_B_MS * log2(distance/W + 1),
// W = assumed target width in px. Literature-standard constants (not
// dataset-derived — see file header).
constexpr double RAT_FITTS_A_MS = 100.0;
constexpr double RAT_FITTS_B_MS = 120.0;
constexpr double RAT_TARGET_W_PX = 24.0;

} // namespace syn
