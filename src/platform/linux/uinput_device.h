#pragma once
#include <string>

// Kernel-level virtual input device (/dev/uinput). Unlike XTest — which only
// updates XWayland's logical pointer and is ignored by Wayland compositors for
// the *visible* cursor — a uinput device is seen as real hardware, so Wayland
// (Hyprland/wlroots) and X11 both move the actual cursor and deliver keys.
//
// Requires write access to /dev/uinput (be in the `input` group). Opened
// lazily; if it can't be opened, is_open() stays false and the caller can fall
// back to XTest.
namespace syn {

class UinputDevice {
public:
    UinputDevice() = default;
    ~UinputDevice();

    // Lazily create the device. screen_w/h are the pixel dimensions the
    // absolute pointer maps onto (queried from X). Returns false on failure.
    bool ensure_open(int screen_w, int screen_h);
    bool is_open() const { return m_fd >= 0; }

    // Mouse. Coordinates are absolute pixels within the screen passed to open.
    void move_abs(int x, int y);
    void button(const std::string& name, bool down);  // "left"/"right"/"middle"
    void scroll(const std::string& direction, int clicks);

    // Keyboard. `key_press` taps a chord ("ctrl+shift+a"); type sends text.
    void key_press(const std::string& chord);
    void key_hold(const std::string& chord);
    void key_release(const std::string& chord);
    void key_type(const std::string& text);

private:
    void emit(unsigned type, unsigned code, int value);
    void sync();
    int  m_fd = -1;
    int  m_sw = 1920, m_sh = 1080;
};

}  // namespace syn
