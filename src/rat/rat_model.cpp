#include "synapse/rat/rat_model.h"
#include "synapse/rat/rat_constants.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <unistd.h>

namespace syn {

// ── Per-user calibration profile ────────────────────────────────────────────────

RatProfile RatProfile::defaults()
{
    return { RAT_FITTS_A_MS, RAT_FITTS_B_MS, RAT_CURVATURE_SCALE,
             /*tremor_sigma=*/0.4, RAT_OVERSHOOT_RATE };
}

// On-disk layout: 4-byte magic "RAT1" + 5 little-endian float32 (24 bytes).
static const char RAT_MAGIC[4] = {'R', 'A', 'T', '1'};

bool RatProfile::save(const char* path) const
{
    std::FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    float v[5] = { float(fitts_a), float(fitts_b), float(curvature_scale),
                   float(tremor_sigma), float(overshoot_rate) };
    bool ok = std::fwrite(RAT_MAGIC, 1, 4, f) == 4 &&
              std::fwrite(v, sizeof(float), 5, f) == 5;
    std::fclose(f);
    return ok;
}

bool RatProfile::load(const char* path, RatProfile& out)
{
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char magic[4];
    float v[5];
    bool ok = std::fread(magic, 1, 4, f) == 4 &&
              std::memcmp(magic, RAT_MAGIC, 4) == 0 &&
              std::fread(v, sizeof(float), 5, f) == 5;
    std::fclose(f);
    if (!ok) return false;
    // Reject NaN/inf and out-of-range values from a corrupt file.
    for (float x : v) if (!std::isfinite(x)) return false;
    if (v[4] < 0.0f || v[4] > 1.0f) return false;         // overshoot_rate
    if (v[0] < 0.0f || v[1] < 0.0f || v[3] < 0.0f) return false;
    out = { v[0], v[1], v[2], v[3], v[4] };
    return true;
}

std::string rat_user_profile_path()
{
    const char* home = std::getenv("HOME");
    if (!home || !*home) return {};
    return std::string(home) + "/.config/synapse/rat_user.bin";
}

RatProfile estimate_profile(const std::vector<CalibrationSample>& s)
{
    RatProfile p = RatProfile::defaults();
    if (s.empty()) return p;

    // Fitts' law least-squares: time = a + b * ID, ID = log2(dist/W + 1).
    double n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
    double curv = 0, trem = 0, over = 0;
    for (const auto& m : s) {
        if (m.distance <= 0 || m.target_w <= 0 || m.time_ms <= 0) continue;
        double id = std::log2(m.distance / m.target_w + 1.0);
        n += 1; sx += id; sy += m.time_ms; sxx += id * id; sxy += id * m.time_ms;
        curv += m.curvature_frac;
        trem += m.tremor;
        over += m.overshot ? 1.0 : 0.0;
    }
    if (n < 2) return p;  // need at least two points to fit a line

    double denom = n * sxx - sx * sx;
    if (std::getenv("SYN_RAT_DEBUG")) {
        double b0 = std::fabs(denom) > 1e-9 ? (n * sxy - sx * sy) / denom : 0.0;
        double a0 = (sy - b0 * sx) / n;
        std::fprintf(stderr, "[rat] n=%.0f raw fit: a=%.1f b=%.1f (denom=%.3g)\n",
                     n, a0, b0, denom);
    }
    if (std::fabs(denom) > 1e-9) {
        double b = (n * sxy - sx * sy) / denom;
        double a = (sy - b * sx) / n;
        // Accept any real fit with a positive slope; clamp to sane ranges rather
        // than reject. A slightly negative intercept is a normal least-squares
        // artifact (short movement times extrapolated to zero difficulty), so
        // floor it at 0 instead of discarding the whole fit and losing the
        // user's slope. Intercept stays well under ~300ms because pre-movement
        // hesitation is modelled separately at generation time.
        if (std::isfinite(a) && std::isfinite(b) && b > 0) {
            p.fitts_a = std::clamp(a, 0.0, 300.0);
            p.fitts_b = std::clamp(b, 20.0, 300.0);
        }
    }
    p.curvature_scale = curv / n;
    p.tremor_sigma    = trem / n;
    p.overshoot_rate  = std::clamp(over / n, 0.0, 1.0);
    return p;
}

const RatProfile& RatModel::active_profile()
{
    static RatProfile prof = [] {
        RatProfile p = RatProfile::defaults();
        std::string path = rat_user_profile_path();
        if (!path.empty()) RatProfile::load(path.c_str(), p);  // keep defaults on failure
        return p;
    }();
    return prof;
}

bool RatModel::active_is_user()
{
    static bool is_user = [] {
        std::string path = rat_user_profile_path();
        RatProfile tmp;
        return !path.empty() && RatProfile::load(path.c_str(), tmp);
    }();
    return is_user;
}

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

// ── Spline path helper ──────────────────────────────────────────────────────
//
// Centripetal Catmull-Rom (alpha = 0.5): an interpolating cubic spline that
// passes through every control point and, unlike the uniform variant, never
// forms cusps or self-loops even when the control points are placed randomly.
// Sampling it densely is what makes the motion buttery-smooth.
namespace {
struct Pt { double x, y; };

Pt cr_eval(const Pt& p0, const Pt& p1, const Pt& p2, const Pt& p3, double u)
{
    auto tnext = [](double ti, const Pt& a, const Pt& b) {
        double d = std::hypot(b.x - a.x, b.y - a.y);
        return ti + std::sqrt(std::max(d, 1e-9)); // alpha = 0.5, guard coincident pts
    };
    double t0 = 0.0;
    double t1 = tnext(t0, p0, p1);
    double t2 = tnext(t1, p1, p2);
    double t3 = tnext(t2, p2, p3);
    double t = t1 + u * (t2 - t1);
    auto lerp = [](const Pt& a, const Pt& b, double f) {
        return Pt{a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f};
    };
    Pt A1 = lerp(p0, p1, (t - t0) / (t1 - t0));
    Pt A2 = lerp(p1, p2, (t - t1) / (t2 - t1));
    Pt A3 = lerp(p2, p3, (t - t2) / (t3 - t2));
    Pt B1 = lerp(A1, A2, (t - t0) / (t2 - t0));
    Pt B2 = lerp(A2, A3, (t - t1) / (t3 - t1));
    return lerp(B1, B2, (t - t1) / (t2 - t1));
}

// Sample the whole spline through `ctrl` at `samples` evenly-parameterised
// points (endpoints duplicated so the curve reaches the first/last control
// point). Returns positions only; timing is applied by the caller.
std::vector<Pt> catmull_rom(const std::vector<Pt>& ctrl, int samples)
{
    std::vector<Pt> out;
    int K = int(ctrl.size());
    if (K < 2) return ctrl;
    int segs = K - 1;
    int per = std::max(2, samples / segs);
    for (int s = 0; s < segs; ++s) {
        const Pt& p1 = ctrl[s];
        const Pt& p2 = ctrl[s + 1];
        const Pt& p0 = ctrl[s == 0 ? 0 : s - 1];
        const Pt& p3 = ctrl[s + 2 >= K ? K - 1 : s + 2];
        int start = (s == 0) ? 0 : 1;  // avoid duplicating shared segment endpoints
        for (int i = start; i <= per; ++i)
            out.push_back(cr_eval(p0, p1, p2, p3, double(i) / per));
    }
    return out;
}
} // namespace

// ── Trajectory generation ──────────────────────────────────────────────────────
//
// A smooth spline through a handful of randomly-jittered control points, sampled
// densely (~2px apart) so playback is continuous rather than stepped. The control
// points sit along the start->target line with Gaussian perpendicular offsets
// (bigger for longer moves), giving a natural bow/zigzag; an optional control
// point past the target produces a human overshoot-and-settle. Timing follows a
// minimum-jerk ease (slow start, fast middle, slow landing) over the Fitts'-law
// duration, so acceleration looks human without a trained model.
std::vector<Waypoint> RatModel::generate(int x0, int y0, int x1, int y1,
                                          double speed_mult, bool linear_mode,
                                          double duration_ms_override)
{
    std::vector<Waypoint> out;
    double dx = x1 - x0, dy = y1 - y0;
    double distance = std::hypot(dx, dy);
    if (distance < 1.0) { out.push_back({x1, y1, 0}); return out; }
    if (speed_mult <= 0.0) speed_mult = 1.0; // defensive; callers should validate

    const RatProfile& prof = active_profile();

    double duration_ms;
    if (duration_ms_override > 0.0) {
        // Explicit `mouse x, y 3s` — honor it directly (wider bound than the
        // Fitts default so long, deliberate moves are possible).
        duration_ms = std::clamp(duration_ms_override, 16.0, 60000.0);
    } else {
        duration_ms = (prof.fitts_a + prof.fitts_b *
                       std::log2(distance / RAT_TARGET_W_PX + 1.0)) / speed_mult;
        duration_ms = std::clamp(duration_ms, 60.0, 4000.0);
    }

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

    double hesitation_ms = 60.0 + rand01(rs) * 140.0; // 60-200ms pre-movement pause

    double ux = dx / distance, uy = dy / distance;
    double perpx = -uy, perpy = ux;

    // Control points: start, a few interior points on the line with Gaussian
    // perpendicular offsets (the "bow"/zigzag), then the target. Offset spread
    // grows with distance but is capped, matching how longer drags wander more.
    double variance = std::min(distance * (prof.curvature_scale * 2.2), 90.0);
    int n_interior = 2 + int(rand01(rs) * 4); // 2-5 interior control points
    std::vector<Pt> ctrl;
    ctrl.push_back({double(x0), double(y0)});
    for (int i = 1; i <= n_interior; ++i) {
        double f = double(i) / (n_interior + 1);
        double off = randn(rs) * variance;
        ctrl.push_back({x0 + dx * f + perpx * off, y0 + dy * f + perpy * off});
    }
    if (rand01(rs) < prof.overshoot_rate) {           // overshoot past the target
        double frac = 0.02 + rand01(rs) * 0.04;
        ctrl.push_back({x1 + dx * frac, y1 + dy * frac});
    }
    ctrl.push_back({double(x1), double(y1)});

    int samples = std::clamp(int(distance / 2.0), 24, 1200); // ~2px spacing, dense
    std::vector<Pt> pts = catmull_rom(ctrl, samples);
    int M = int(pts.size());

    // Timing: minimum-jerk ease (slow-fast-slow) over the Fitts duration. Points
    // are uniform along the curve; the eased time-of-arrival gives the velocity
    // profile. t_ms steps stay >=1ms so replay_waypoints renders every point.
    double t_cursor = hesitation_ms;
    out.push_back({x0, y0, uint32_t(std::lround(t_cursor))});
    for (int i = 1; i < M; ++i) {
        double u = double(i) / (M - 1);
        double s = 3 * u * u - 2 * u * u * u;         // min-jerk position fraction
        out.push_back({int(std::lround(pts[i].x)), int(std::lround(pts[i].y)),
                        uint32_t(std::lround(hesitation_ms + duration_ms * s))});
    }
    out.back().x = x1; out.back().y = y1;             // exact final landing
    return out;
}

RatModel::Preview RatModel::preview(int x0, int y0, int x1, int y1, double speed_mult)
{
    auto path = generate(x0, y0, x1, y1, speed_mult, false);
    double dur = path.empty() ? 0.0 : double(path.back().t_ms);
    return {dur, int(path.size())};
}

} // namespace syn
