#pragma once
#include "synapse/platform/platform.h"
#include "uinput_device.h"

// Forward-declare Xlib types instead of including Xlib.h here — Xlib.h
// `#define`s generic names (Window, Font, Time, ...) that collide badly if
// this header is ever included alongside other code. Only linux_platform.cpp
// needs the real Xlib/XTest/EWMH calls.
struct _XDisplay;

namespace syn {

// Real automation backend: XTest (libXtst) for mouse/keyboard input, Xlib +
// EWMH for window management, XGetImage + libpng for screen capture,
// posix_spawn for app run/open/close. X11-only (matches DISPLAY-based
// desktops); Wayland/uinput backend is future work (see docs/design/002 §7).
class LinuxPlatform : public Platform {
public:
    LinuxPlatform();
    ~LinuxPlatform() override;

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
    std::pair<int,int> screen_size() override;

    void     wait(uint64_t ns) override;
    uint64_t now_ns() override;

    bool calibrate_rat(int movements, std::vector<CalibrationSample>& out) override;

private:
    // Lazily create the uinput device (mapped to the current X screen size) so
    // input moves the *visible* cursor on Wayland. Returns false if /dev/uinput
    // isn't writable, in which case callers fall back to XTest.
    bool use_uinput();

    _XDisplay*   m_display;
    UinputDevice m_uinput;
};

} // namespace syn
