#pragma once
#include "synapse/platform/platform.h"
#include <vector>

namespace syn {

// RAT v1 — physics-only human mouse trajectory generator (design doc 005).
// No trained model: multi-peaked velocity profile (2-4 overlapping
// minimum-jerk sub-movements), signal-dependent noise, ~39% overshoot rate,
// pre-movement hesitation, and terminal slowdown, calibrated against
// Dataset/movements.jsonl (see include/synapse/rat/rat_constants.h). Real
// model training (WGAN-GP+LSTM / Auto-GRU / etc.) is deferred — this is the
// Phase 6.5 starting point, not the end state.
class RatModel {
public:
    // mode: "rat" (default) uses the physics generator below; "linear"
    // bypasses it entirely (straight line, constant speed) — for testing,
    // per design 005 §4 (`mouse.mode "linear"`).
    static std::vector<Waypoint> generate(int x0, int y0, int x1, int y1,
                                           double speed_mult = 1.0,
                                           bool linear_mode = false);

    struct Preview { double duration_ms; int waypoints; };
    static Preview preview(int x0, int y0, int x1, int y1, double speed_mult = 1.0);
};

} // namespace syn
