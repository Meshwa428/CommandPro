#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "synapse/rat/rat_model.h"
#include <cstdio>

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
