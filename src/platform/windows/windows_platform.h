#pragma once
#include "synapse/platform/platform.h"

namespace syn {

// Real automation backend for Windows: SendInput for mouse/keyboard,
// EnumWindows + SetForegroundWindow/SetWindowPos/ShowWindow for window
// management, GDI BitBlt + GDI+ for PNG screen capture, CreateProcess/
// ShellExecute for app run/open/close. Mirrors LinuxPlatform semantics:
// case-insensitive substring window title match, chord syntax
// "ctrl+shift+a", named keys ("enter", "escape", ...).
//
// UNTESTED on real Windows yet — written on Linux against Win32 docs; first
// Windows CI run will shake out signature/linker issues.
class WindowsPlatform : public Platform {
public:
    WindowsPlatform() = default;
    ~WindowsPlatform() override = default;

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
};

} // namespace syn
