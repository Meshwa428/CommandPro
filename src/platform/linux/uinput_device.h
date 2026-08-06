#pragma once
#include <string>
#include <utility>
#include <vector>

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

    // Authoritative cursor position: we set every absolute move, so we track it
    // ourselves rather than trust XQueryPointer (which doesn't follow the
    // uinput-moved cursor on Wayland). seed_pos primes it before the first move.
    void seed_pos(int x, int y) { if (!m_have_pos) { m_last_x = x; m_last_y = y; m_have_pos = true; } }
    bool has_pos() const { return m_have_pos; }
    std::pair<int,int> last_pos() const { return {m_last_x, m_last_y}; }

    // Real cursor position, Linux-native: our own absolute moves are tracked
    // exactly, and PHYSICAL mouse motion is folded in by draining the relative
    // (REL_X/REL_Y) evdev devices under /dev/input — so if the user nudges the
    // real mouse between commands, this reflects it. No compositor/X dependency.
    std::pair<int,int> current_pos();

    // Keyboard. `key_press` taps a chord ("ctrl+shift+a"); type sends text.
    void key_press(const std::string& chord);
    void key_hold(const std::string& chord);
    void key_release(const std::string& chord);
    void key_type(const std::string& text);

private:
    void emit(unsigned type, unsigned code, int value);
    void sync();
    void scan_physical_pointers();   // open REL mice under /dev/input (once)
    void drain_physical();           // fold pending physical deltas into m_last_*

    int  m_fd = -1;
    int  m_sw = 1920, m_sh = 1080;
    int  m_last_x = 0, m_last_y = 0;
    bool m_have_pos = false;
    std::vector<int> m_phys_fds;     // physical relative-pointer evdev fds
    bool m_phys_scanned = false;
};

}  // namespace syn
