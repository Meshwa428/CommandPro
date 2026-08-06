#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "synapse/rat/rat_model.h"
#include <cstdio>
#include <cmath>
#include <vector>

using namespace syn;

TEST_CASE("RatProfile save/load round-trips", "[rat]") {
    const char* path = "/tmp/syn_rat_profile_test.bin";
    std::remove(path);

    RatProfile p{ 150.0, 90.0, 0.08, 0.5, 0.25 };
    REQUIRE(p.save(path));

    RatProfile back{};
    REQUIRE(RatProfile::load(path, back));
    // Stored as float32, so compare with a tolerance.
    REQUIRE(back.fitts_a         == Catch::Approx(150.0));
    REQUIRE(back.fitts_b         == Catch::Approx(90.0));
    REQUIRE(back.curvature_scale == Catch::Approx(0.08));
    REQUIRE(back.tremor_sigma    == Catch::Approx(0.5));
    REQUIRE(back.overshoot_rate  == Catch::Approx(0.25));

    std::remove(path);
}

TEST_CASE("RatProfile::load rejects absent and corrupt files", "[rat]") {
    RatProfile out{};
    REQUIRE_FALSE(RatProfile::load("/tmp/syn_rat_does_not_exist.bin", out));

    const char* path = "/tmp/syn_rat_corrupt_test.bin";
    std::FILE* f = std::fopen(path, "wb");
    REQUIRE(f);
    std::fputs("XXXXnot a profile", f);
    std::fclose(f);
    REQUIRE_FALSE(RatProfile::load(path, out));  // bad magic
    std::remove(path);
}

TEST_CASE("RatProfile::load rejects out-of-range values", "[rat]") {
    const char* path = "/tmp/syn_rat_range_test.bin";
    RatProfile bad{ 150.0, 90.0, 0.08, 0.5, 5.0 };  // overshoot_rate > 1
    REQUIRE(bad.save(path));
    RatProfile out{};
    REQUIRE_FALSE(RatProfile::load(path, out));
    std::remove(path);
}

TEST_CASE("RatProfile::defaults matches baked constants", "[rat]") {
    RatProfile d = RatProfile::defaults();
    REQUIRE(d.overshoot_rate == Catch::Approx(0.391));
    REQUIRE(d.fitts_a == Catch::Approx(100.0));
}

TEST_CASE("estimate_profile recovers Fitts a/b from clean samples", "[rat]") {
    // Synthesize movements with time = 120 + 80 * log2(dist/W + 1).
    const double A = 120.0, B = 80.0;
    std::vector<CalibrationSample> s;
    for (int dist = 50; dist <= 1000; dist += 50) {
        double id = std::log2(dist / 24.0 + 1.0);
        s.push_back({ double(dist), 24.0, A + B * id, 0.05, 0.4, (dist % 150 == 0) });
    }
    RatProfile p = estimate_profile(s);
    REQUIRE(p.fitts_a == Catch::Approx(A).epsilon(0.02));
    REQUIRE(p.fitts_b == Catch::Approx(B).epsilon(0.02));
    REQUIRE(p.curvature_scale == Catch::Approx(0.05));
    REQUIRE(p.overshoot_rate >= 0.0);
    REQUIRE(p.overshoot_rate <= 1.0);
}

TEST_CASE("generate honors an explicit duration override", "[rat]") {
    // A 1000px move with a 2s override should take ~2s (+ pre-movement
    // hesitation of 100-300ms), and far longer than the Fitts default.
    auto with = RatModel::generate(0, 0, 1000, 0, 1.0, false, /*duration_ms=*/2000.0);
    REQUIRE(with.size() > 2);
    double t_override = with.back().t_ms;
    REQUIRE(t_override >= 2000.0);
    REQUIRE(t_override <= 2500.0);   // 2000 + hesitation, no more

    auto def = RatModel::generate(0, 0, 1000, 0);  // Fitts default
    REQUIRE(def.back().t_ms < t_override);
}

TEST_CASE("generate spline lands on target with monotonic time", "[rat]") {
    // The Catmull-Rom path may bow/overshoot mid-flight, but must terminate
    // exactly on the target, with non-decreasing timestamps and dense sampling.
    auto wp = RatModel::generate(100, 200, 900, 600);
    REQUIRE(wp.size() > 10);                 // dense, not a straight 2-point hop
    REQUIRE(wp.back().x == 900);             // endpoint pinned to target
    REQUIRE(wp.back().y == 600);
    for (size_t i = 1; i < wp.size(); ++i)
        REQUIRE(wp[i].t_ms >= wp[i - 1].t_ms);  // time never runs backward
}

TEST_CASE("estimate_profile falls back on degenerate input", "[rat]") {
    RatProfile def = RatProfile::defaults();
    REQUIRE(estimate_profile({}).fitts_a == Catch::Approx(def.fitts_a));
    // Single sample can't fit a line → keep default a/b.
    std::vector<CalibrationSample> one{ { 100.0, 24.0, 200.0, 0.05, 0.4, false } };
    REQUIRE(estimate_profile(one).fitts_a == Catch::Approx(def.fitts_a));
}
