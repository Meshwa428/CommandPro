#pragma once
#include "synapse/platform/platform.h"
#include <vector>
#include <string>

namespace syn {

// Tunable movement parameters. Defaults are the dataset-calibrated constants
// baked in rat_constants.h; a per-user calibration (`syn rat calibrate`)
// overrides them from ~/.config/synapse/rat_user.bin (design 005 §6). The
// generator reads these at runtime rather than the constexprs directly so a
// user profile can take effect without recompiling.
struct RatProfile {
    double fitts_a;         // Fitts' law intercept (ms)
    double fitts_b;         // Fitts' law slope (ms / bit)
    double curvature_scale; // path bow as fraction of distance
    double tremor_sigma;    // signal-dependent noise coefficient
    double overshoot_rate;  // fraction of movements that overshoot [0,1]

    static RatProfile defaults();          // baked constants
    bool save(const char* path) const;     // serialize to rat_user.bin
    static bool load(const char* path, RatProfile& out);  // false if absent/invalid
};

// Path to the per-user calibration file (~/.config/synapse/rat_user.bin).
// Empty if $HOME is unset. Does not create the directory.
std::string rat_user_profile_path();

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

    // The profile in effect: the user calibration if ~/.config/synapse/
    // rat_user.bin loads, otherwise baked defaults. Loaded once, lazily.
    static const RatProfile& active_profile();
    static bool active_is_user();  // true if a user profile was loaded
};

} // namespace syn
