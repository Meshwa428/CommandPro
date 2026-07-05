#include "mock_platform.h"
#include <sstream>

namespace syn {

void MockPlatform::mouse_move_to(int x, int y)
{
    m_cursor_x = x; m_cursor_y = y;
    push("MouseMove(" + std::to_string(x) + ", " + std::to_string(y) + ")");
}

void MockPlatform::replay_waypoints(const std::vector<Waypoint>& path)
{
    // Tests assert on the destination, not RAT's randomized intermediate
    // points — logging every waypoint would make goldens nondeterministic.
    if (path.empty()) return;
    m_cursor_x = path.back().x; m_cursor_y = path.back().y;
    push("MouseMove(" + std::to_string(m_cursor_x) + ", " + std::to_string(m_cursor_y) + ")");
}

void MockPlatform::mouse_click(const std::string& button, int count)
{
    std::ostringstream os;
    os << "MouseClick(" << button;
    if (count > 1) os << ", count: " << count;
    os << ")";
    push(os.str());
}

void MockPlatform::mouse_hold(const std::string& button)    { push("MouseHold(" + button + ")"); }
void MockPlatform::mouse_release(const std::string& button) { push("MouseRelease(" + button + ")"); }

void MockPlatform::mouse_scroll(const std::string& direction, int amount)
{
    push("MouseScroll(" + direction + ", " + std::to_string(amount) + ")");
}

std::pair<int,int> MockPlatform::mouse_position() { return {m_cursor_x, m_cursor_y}; }

void MockPlatform::key_press(const std::string& chord)   { push("KeyPress(" + chord + ")"); }
void MockPlatform::key_hold(const std::string& chord)    { push("KeyHold(" + chord + ")"); }
void MockPlatform::key_release(const std::string& chord) { push("KeyRelease(" + chord + ")"); }
void MockPlatform::key_type(const std::string& text)     { push("KeyType(" + text + ")"); }

void MockPlatform::window_focus(const std::string& name) { push("WindowFocus(" + name + ")"); }

void MockPlatform::window_move(const std::string& name, int x, int y)
{
    push("WindowMove(" + name + ", " + std::to_string(x) + ", " + std::to_string(y) + ")");
}

void MockPlatform::window_resize(const std::string& name, int w, int h)
{
    push("WindowResize(" + name + ", " + std::to_string(w) + ", " + std::to_string(h) + ")");
}

void MockPlatform::window_maximize(const std::string& name) { push("WindowMaximize(" + name + ")"); }
void MockPlatform::window_minimize(const std::string& name) { push("WindowMinimize(" + name + ")"); }

void MockPlatform::app_run(const std::string& command)  { push("AppRun(" + command + ")"); }
void MockPlatform::app_open(const std::string& name)     { push("AppOpen(" + name + ")"); }
void MockPlatform::app_close(const std::string& name)    { push("AppClose(" + name + ")"); }

void MockPlatform::screen_capture(const std::string& file, int x1, int y1, int x2, int y2)
{
    std::ostringstream os;
    os << "ScreenCapture(" << file;
    if (x1 || y1 || x2 || y2)
        os << ", " << x1 << ", " << y1 << ", " << x2 << ", " << y2;
    os << ")";
    push(os.str());
}

void MockPlatform::wait(uint64_t ns)
{
    m_virtual_clock_ns += ns;
    push("Wait(" + std::to_string(ns) + ")");
}

uint64_t MockPlatform::now_ns() { return m_virtual_clock_ns; }

} // namespace syn
