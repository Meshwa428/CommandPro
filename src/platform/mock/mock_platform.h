#pragma once
#include "synapse/platform/platform.h"
#include <cstdint>
#include <string>
#include <vector>

namespace syn {

// Records every call as a formatted log line instead of touching the OS.
// This is the backend tests/automation/*.syn goldens assert against via
// `# expect-platform: MouseMove(300, 400)` comments (see docs/design/003).
// The clock is virtual: wait() advances a counter instead of sleeping, so
// `wait 2s` runs in microseconds under test.
class MockPlatform : public Platform {
public:
    void mouse_move_to(int x, int y) override;
    void replay_waypoints(const std::vector<Waypoint>& path) override;
    void mouse_click(const std::string& button, int count) override;
    void mouse_hold(const std::string& button) override;
    void mouse_release(const std::string& button) override;
    void mouse_scroll(const std::string& direction, int amount) override;
    std::pair<int,int> mouse_position() override;

    void key_press(const std::string& chord) override;
    void key_hold(const std::string& chord) override;
    void key_release(const std::string& chord) override;
    void key_type(const std::string& text) override;

    void window_focus(const std::string& name) override;
    void window_move(const std::string& name, int x, int y) override;
    void window_resize(const std::string& name, int w, int h) override;
    void window_maximize(const std::string& name) override;
    void window_minimize(const std::string& name) override;

    void app_run(const std::string& command) override;
    void app_open(const std::string& name) override;
    void app_close(const std::string& name) override;

    void screen_capture(const std::string& file, int x1, int y1, int x2, int y2) override;

    void     wait(uint64_t ns) override;
    uint64_t now_ns() override;

    // ── Test inspection ──────────────────────────────────────────────────────
    const std::vector<std::string>& log() const { return m_log; }
    void clear_log() { m_log.clear(); }

private:
    void push(std::string entry) { m_log.push_back(std::move(entry)); }

    std::vector<std::string> m_log;
    int      m_cursor_x = 0, m_cursor_y = 0;
    uint64_t m_virtual_clock_ns = 0;
};

} // namespace syn
