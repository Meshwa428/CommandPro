#include "synapse/rat/rat_model.h"
#include "synapse/rat/rat_constants.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <unistd.h>

namespace syn {

// ── Per-session RNG ────────────────────────────────────────────────────────────
// splitmix64, seeded once per process from std::random_device (Linux: backed
// by getrandom()/RDRAND) + high-res clock + PID, so the same `mouse x, y`
// produces a different curve every run — the pattern itself can't be
// fingerprinted across sessions (design 005 §5).

static uint64_t splitmix64_next(uint64_t& state)
{
    uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static uint64_t make_session_seed()
{
    std::random_device rd;
    uint64_t a = (uint64_t(rd()) << 32) ^ uint64_t(rd());
    uint64_t b = uint64_t(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    uint64_t c = uint64_t(getpid());
    return a ^ (b * 0x9E3779B97F4A7C15ULL) ^ (c * 0xBF58476D1CE4E5B9ULL);
}

static uint64_t& rng_state()
{
    static uint64_t state = make_session_seed();
    return state;
}

static double rand01(uint64_t& s)
{
    return double(splitmix64_next(s) >> 11) * (1.0 / 9007199254740992.0); // 53-bit mantissa
}

static double randn(uint64_t& s) // Box-Muller
{
    double u1 = std::max(rand01(s), 1e-12);
    double u2 = rand01(s);
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
}

// ── Trajectory generation ──────────────────────────────────────────────────────
//
// Real human movement is a sequence of overlapping minimum-jerk sub-movements
// (design 005 §2.1), not one smooth arc. This generates that as several
// minimum-jerk segments chained end-to-end in time: a primary movement
// (optionally overshooting the target, §2.6) followed by 1-3 shrinking
// correction segments converging on the true target. Because minimum-jerk
// velocity is zero at both ends of each segment, chaining them already
// produces the multi-peaked velocity envelope detectors look for — without
// needing literal analytic impulse summation.
std::vector<Waypoint> RatModel::generate(int x0, int y0, int x1, int y1,
                                          double speed_mult, bool linear_mode)
{
    std::vector<Waypoint> out;
    double dx = x1 - x0, dy = y1 - y0;
    double distance = std::hypot(dx, dy);
    if (distance < 1.0) { out.push_back({x1, y1, 0}); return out; }
    if (speed_mult <= 0.0) speed_mult = 1.0; // defensive; callers should validate

    double duration_ms = (RAT_FITTS_A_MS + RAT_FITTS_B_MS *
                           std::log2(distance / RAT_TARGET_W_PX + 1.0)) / speed_mult;
    duration_ms = std::clamp(duration_ms, 60.0, 4000.0);

    uint64_t& rs = rng_state();

    if (linear_mode) {
        int n = std::max(2, int(duration_ms / 16.0));
        for (int i = 0; i <= n; ++i) {
            double u = double(i) / n;
            out.push_back({int(std::lround(x0 + dx * u)), int(std::lround(y0 + dy * u)),
                            uint32_t(std::lround(duration_ms * u))});
        }
        return out;
    }

    double hesitation_ms = 100.0 + rand01(rs) * 200.0; // 100-300ms, §2.5

    bool overshoot = rand01(rs) < RAT_OVERSHOOT_RATE;
    double px = x1, py = y1;
    if (overshoot) {
        double frac = 0.03 + rand01(rs) * 0.05; // overshoot by 3-8% of distance
        px = x1 + dx * frac; py = y1 + dy * frac;
    }

    struct Seg { double sx, sy, ex, ey, dur_ms; };
    std::vector<Seg> segs;

    double primary_time_frac = 0.55 + rand01(rs) * 0.20;
    double primary_dur = duration_ms * primary_time_frac;
    segs.push_back({double(x0), double(y0), px, py, primary_dur});
    double remaining_ms = duration_ms - primary_dur;

    int n_corrections = 1 + int(rand01(rs) * 3); // 1-3 corrections: 2-4 sub-movements total
    double cx = px, cy = py;
    for (int i = 0; i < n_corrections; ++i) {
        bool last = (i == n_corrections - 1);
        double tx = last ? double(x1) : (cx + (x1 - cx) * (0.5 + rand01(rs) * 0.3));
        double ty = last ? double(y1) : (cy + (y1 - cy) * (0.5 + rand01(rs) * 0.3));
        double seg_dur = last ? std::max(60.0, remaining_ms)
                              : std::max(40.0, remaining_ms * (0.5 + rand01(rs) * 0.2));
        segs.push_back({cx, cy, tx, ty, seg_dur});
        remaining_ms = std::max(40.0, remaining_ms - seg_dur);
        cx = tx; cy = ty;
    }
    segs.back().ex = x1; segs.back().ey = y1; // exact landing, no float drift

    double curvature_amp = distance * RAT_CURVATURE_SCALE;
    double curve_sign = (rand01(rs) < 0.5) ? -1.0 : 1.0; // which side the arm sweeps

    double t_cursor = hesitation_ms;
    out.push_back({x0, y0, uint32_t(std::lround(t_cursor))});

    for (size_t si = 0; si < segs.size(); ++si) {
        const Seg& sg = segs[si];
        double sdx = sg.ex - sg.sx, sdy = sg.ey - sg.sy;
        double seg_len = std::hypot(sdx, sdy);
        double ux = seg_len > 1e-6 ? sdx / seg_len : 0.0;
        double uy = seg_len > 1e-6 ? sdy / seg_len : 0.0;
        double perpx = -uy, perpy = ux;
        int n = std::max(2, int(sg.dur_ms / 16.0));
        double speed_px_per_ms = seg_len / std::max(sg.dur_ms, 1.0);

        for (int i = 1; i <= n; ++i) {
            double u = double(i) / n;
            double s = 3 * u * u - 2 * u * u * u; // minimum-jerk position fraction
            double bx = sg.sx + sdx * s;
            double by = sg.sy + sdy * s;

            double arc = (si == 0) ? curve_sign * curvature_amp * std::sin(M_PI * u) : 0.0;

            // Signal-dependent noise (§2.2): sigma scales with local speed,
            // not flat — fast mid-movement is noisier, slow approach is precise.
            double dsdu = 6 * u * (1 - u);
            double local_speed = speed_px_per_ms * dsdu;
            double jitter = local_speed > 0 ? randn(rs) * (0.4 * local_speed) : 0.0;

            double fx = bx + perpx * (arc + jitter);
            double fy = by + perpy * (arc + jitter);
            t_cursor += sg.dur_ms / n;
            out.push_back({int(std::lround(fx)), int(std::lround(fy)),
                            uint32_t(std::lround(t_cursor))});
        }
    }
    out.back().x = x1; out.back().y = y1; // exact final landing
    return out;
}

RatModel::Preview RatModel::preview(int x0, int y0, int x1, int y1, double speed_mult)
{
    auto path = generate(x0, y0, x1, y1, speed_mult, false);
    double dur = path.empty() ? 0.0 : double(path.back().t_ms);
    return {dur, int(path.size())};
}

} // namespace syn
