#pragma once

// Tuning constants for RatModel (src/rat/rat_model.cpp). Plain literals baked
// into the binary — no model weights, no dataset, no runtime file I/O. The
// spline generator reads these to shape its control-point spread and overshoot
// behaviour; adjust them by hand to taste. Duration uses standard Fitts'-law
// literature constants.

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
