#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace syn {

struct CalibrationSample;  // rat_model.h

// One point on a mouse trajectory: absolute screen coords + time offset from
// the start of the movement. RatModel (Stage 4) produces these; Platform
// implementations replay them.
struct Waypoint {
    int      x, y;
    uint32_t t_ms;  // offset from movement start
};

// Pure interface between the automation stdlib (mouse/keyboard/window/app/
// screen/time natives) and the OS. The VM only ever sees this — never a
// concrete backend directly. MockPlatform (tests) and LinuxPlatform (real
// input/window/screen) are the two implementations; Windows/macOS are
// post-1.0 per PLAN.md.
class Platform {
public:
    virtual ~Platform() = default;

    // ── Mouse ────────────────────────────────────────────────────────────────
    virtual void mouse_move_to(int x, int y) = 0;
    virtual void replay_waypoints(const std::vector<Waypoint>& path) = 0;
    virtual void mouse_click(const std::string& button, int count) = 0;
    virtual void mouse_hold(const std::string& button) = 0;
    virtual void mouse_release(const std::string& button) = 0;
    virtual void mouse_scroll(const std::string& direction, int amount) = 0;
    virtual std::pair<int,int> mouse_position() = 0;

    // ── Keyboard ─────────────────────────────────────────────────────────────
    virtual void key_press(const std::string& chord) = 0;
    virtual void key_hold(const std::string& chord) = 0;
    virtual void key_release(const std::string& chord) = 0;
    virtual void key_type(const std::string& text) = 0;

    // ── Window ───────────────────────────────────────────────────────────────
    virtual void window_focus(const std::string& name) = 0;
    virtual void window_move(const std::string& name, int x, int y) = 0;
    virtual void window_resize(const std::string& name, int w, int h) = 0;
    virtual void window_maximize(const std::string& name) = 0;
    virtual void window_minimize(const std::string& name) = 0;

    // ── App ──────────────────────────────────────────────────────────────────
    virtual void app_run(const std::string& command) = 0;
    virtual void app_open(const std::string& name) = 0;
    virtual void app_close(const std::string& name) = 0;

    // ── Screen ───────────────────────────────────────────────────────────────
    // x1==x2==y1==y2==0 means full-screen capture.
    virtual void screen_capture(const std::string& file, int x1, int y1, int x2, int y2) = 0;

    // ── Clock ────────────────────────────────────────────────────────────────
    // MockPlatform advances a virtual clock instead of sleeping, so
    // `wait 2s` runs in microseconds under test.
    virtual void     wait(uint64_t ns) = 0;
    virtual uint64_t now_ns() = 0;

    // ── Calibration ──────────────────────────────────────────────────────────
    // Interactive `syn rat calibrate`: show a fullscreen overlay, collect
    // `movements` human clicks into `out`, return true on success. Default: no
    // overlay (mock/headless backends) — returns false.
    virtual bool calibrate_rat(int movements, std::vector<CalibrationSample>& out)
    { (void)movements; (void)out; return false; }
};

} // namespace syn
