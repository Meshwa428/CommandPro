#include "linux_platform.h"
#include "synapse/rat/rat_model.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <png.h>
#include <spawn.h>
#include <unistd.h>
#include <sys/wait.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

extern char** environ;

namespace syn {

// ── Construction ─────────────────────────────────────────────────────────────
//
// The X11 connection is opened lazily on first real use, not in the
// constructor: automation_stdlib registers a Platform on every script run
// (including plain-compute scripts with zero automation calls), so an eager
// XOpenDisplay would add a socket round-trip to every process — measured as
// a ~1-1.5ms fixed cost on every benchmark run, unrelated to what's actually
// being benchmarked.

LinuxPlatform::LinuxPlatform() : m_display(nullptr) {}

LinuxPlatform::~LinuxPlatform()
{
    if (m_display) XCloseDisplay(reinterpret_cast<Display*>(m_display));
}

static Display* dpy(_XDisplay*& d)
{
    if (!d) {
        d = reinterpret_cast<_XDisplay*>(XOpenDisplay(nullptr));
        if (!d) throw std::runtime_error("LinuxPlatform: cannot open X display (is $DISPLAY set?)");
    }
    return reinterpret_cast<Display*>(d);
}

// ── Mouse ────────────────────────────────────────────────────────────────────

bool LinuxPlatform::use_uinput()
{
    if (m_uinput.is_open()) return true;
    Display* d = dpy(m_display);
    int scr = DefaultScreen(d);
    return m_uinput.ensure_open(DisplayWidth(d, scr), DisplayHeight(d, scr));
}

void LinuxPlatform::mouse_move_to(int x, int y)
{
    if (use_uinput()) { m_uinput.move_abs(x, y); return; }
    XTestFakeMotionEvent(dpy(m_display), -1, x, y, CurrentTime);
    XFlush(dpy(m_display));
}

void LinuxPlatform::replay_waypoints(const std::vector<Waypoint>& path)
{
    bool uinput = use_uinput();
    uint32_t last_t = 0;
    for (const auto& wp : path) {
        if (wp.t_ms > last_t) {
            std::this_thread::sleep_for(std::chrono::milliseconds(wp.t_ms - last_t));
            last_t = wp.t_ms;
        }
        if (uinput) {
            m_uinput.move_abs(wp.x, wp.y);
        } else {
            XTestFakeMotionEvent(dpy(m_display), -1, wp.x, wp.y, CurrentTime);
            XFlush(dpy(m_display));
        }
    }
}

static unsigned int button_code(const std::string& button)
{
    if (button == "right")  return Button3;
    if (button == "middle") return Button2;
    return Button1; // "left" and unrecognized default to left
}

void LinuxPlatform::mouse_click(const std::string& button, int count)
{
    if (use_uinput()) {
        for (int i = 0; i < count; ++i) {
            m_uinput.button(button, true);
            m_uinput.button(button, false);
            if (i + 1 < count) std::this_thread::sleep_for(std::chrono::milliseconds(60));
        }
        return;
    }
    unsigned int b = button_code(button);
    for (int i = 0; i < count; ++i) {
        XTestFakeButtonEvent(dpy(m_display), b, True, CurrentTime);
        XTestFakeButtonEvent(dpy(m_display), b, False, CurrentTime);
        XFlush(dpy(m_display));
        if (i + 1 < count) std::this_thread::sleep_for(std::chrono::milliseconds(60));
    }
}

void LinuxPlatform::mouse_hold(const std::string& button)
{
    if (use_uinput()) { m_uinput.button(button, true); return; }
    XTestFakeButtonEvent(dpy(m_display), button_code(button), True, CurrentTime);
    XFlush(dpy(m_display));
}

void LinuxPlatform::mouse_release(const std::string& button)
{
    if (use_uinput()) { m_uinput.button(button, false); return; }
    XTestFakeButtonEvent(dpy(m_display), button_code(button), False, CurrentTime);
    XFlush(dpy(m_display));
}

void LinuxPlatform::mouse_scroll(const std::string& direction, int amount)
{
    if (use_uinput()) { m_uinput.scroll(direction, amount); return; }
    // X11 scroll wheel is button 4 (up) / 5 (down) / 6 (left) / 7 (right).
    unsigned int b = (direction == "down") ? 5 : (direction == "left") ? 6
                    : (direction == "right") ? 7 : 4;
    for (int i = 0; i < amount; ++i) {
        XTestFakeButtonEvent(dpy(m_display), b, True, CurrentTime);
        XTestFakeButtonEvent(dpy(m_display), b, False, CurrentTime);
    }
    XFlush(dpy(m_display));
}

std::pair<int,int> LinuxPlatform::mouse_position()
{
    Window root = DefaultRootWindow(dpy(m_display));
    Window ret_root, ret_child;
    int root_x, root_y, win_x, win_y;
    unsigned int mask;
    XQueryPointer(dpy(m_display), root, &ret_root, &ret_child,
                  &root_x, &root_y, &win_x, &win_y, &mask);
    return {root_x, root_y};
}

// ── Keyboard ─────────────────────────────────────────────────────────────────
//
// ponytail: ASCII-printable US-layout only — XStringToKeysym/XKeysymToKeycode
// can't type a character that has no keycode in the current layout (e.g. most
// Unicode). Upgrade path: temporarily remap a spare keycode via
// XChangeKeyboardMapping for characters not found (what xdotool --clearmodifiers
// does), or route through XTestFakeKeyEvent with an input-method layer.

static KeyCode keycode_for(Display* d, const std::string& name)
{
    KeySym ks = XStringToKeysym(name.c_str());
    if (ks == NoSymbol) return 0;
    return XKeysymToKeycode(d, ks);
}

static KeyCode modifier_keycode(Display* d, const std::string& mod)
{
    std::string m = mod;
    for (char& c : m) c = char(std::tolower(static_cast<unsigned char>(c)));
    if (m == "ctrl" || m == "control") return keycode_for(d, "Control_L");
    if (m == "shift")                 return keycode_for(d, "Shift_L");
    if (m == "alt")                   return keycode_for(d, "Alt_L");
    if (m == "super" || m == "win" || m == "cmd") return keycode_for(d, "Super_L");
    return 0;
}

// Splits "ctrl+shift+a" into ([Control_L, Shift_L], "a").
static void parse_chord(const std::string& chord, std::vector<std::string>& mods, std::string& key)
{
    size_t start = 0;
    while (true) {
        size_t plus = chord.find('+', start);
        std::string part = chord.substr(start, plus == std::string::npos ? std::string::npos : plus - start);
        if (plus == std::string::npos) { key = part; break; }
        mods.push_back(part);
        start = plus + 1;
    }
}

// Named non-letter keys the parser's key_chord grammar allows through as-is
// ("enter", "escape", "tab", ...) map onto their X11 keysym names.
static std::string keysym_name_for_key(const std::string& key)
{
    static const std::pair<const char*, const char*> table[] = {
        {"enter", "Return"}, {"return", "Return"}, {"escape", "Escape"}, {"esc", "Escape"},
        {"tab", "Tab"}, {"space", "space"}, {"backspace", "BackSpace"}, {"delete", "Delete"},
        {"up", "Up"}, {"down", "Down"}, {"left", "Left"}, {"right", "Right"},
        {"home", "Home"}, {"end", "End"}, {"pageup", "Prior"}, {"pagedown", "Next"},
    };
    for (auto& [name, sym] : table) if (key == name) return sym;
    return key; // single letters/digits: XStringToKeysym accepts them as-is
}

void LinuxPlatform::key_press(const std::string& chord)
{
    if (use_uinput()) { m_uinput.key_press(chord); return; }
    Display* d = dpy(m_display);
    std::vector<std::string> mods; std::string key;
    parse_chord(chord, mods, key);

    std::vector<KeyCode> mod_codes;
    for (auto& m : mods) if (KeyCode kc = modifier_keycode(d, m)) mod_codes.push_back(kc);
    KeyCode key_code = keycode_for(d, keysym_name_for_key(key));

    for (auto kc : mod_codes) XTestFakeKeyEvent(d, kc, True, CurrentTime);
    if (key_code) {
        XTestFakeKeyEvent(d, key_code, True, CurrentTime);
        XTestFakeKeyEvent(d, key_code, False, CurrentTime);
    }
    for (auto it = mod_codes.rbegin(); it != mod_codes.rend(); ++it)
        XTestFakeKeyEvent(d, *it, False, CurrentTime);
    XFlush(d);
}

void LinuxPlatform::key_hold(const std::string& chord)
{
    if (use_uinput()) { m_uinput.key_hold(chord); return; }
    Display* d = dpy(m_display);
    std::vector<std::string> mods; std::string key;
    parse_chord(chord, mods, key);
    for (auto& m : mods) if (KeyCode kc = modifier_keycode(d, m)) XTestFakeKeyEvent(d, kc, True, CurrentTime);
    if (KeyCode kc = keycode_for(d, keysym_name_for_key(key))) XTestFakeKeyEvent(d, kc, True, CurrentTime);
    XFlush(d);
}

void LinuxPlatform::key_release(const std::string& chord)
{
    if (use_uinput()) { m_uinput.key_release(chord); return; }
    Display* d = dpy(m_display);
    std::vector<std::string> mods; std::string key;
    parse_chord(chord, mods, key);
    for (auto& m : mods) if (KeyCode kc = modifier_keycode(d, m)) XTestFakeKeyEvent(d, kc, False, CurrentTime);
    if (KeyCode kc = keycode_for(d, keysym_name_for_key(key))) XTestFakeKeyEvent(d, kc, False, CurrentTime);
    XFlush(d);
}

void LinuxPlatform::key_type(const std::string& text)
{
    if (use_uinput()) { m_uinput.key_type(text); return; }
    Display* d = dpy(m_display);
    KeyCode shift = keycode_for(d, "Shift_L");
    for (unsigned char c : text) {
        std::string s(1, char(c));
        KeySym ks = XStringToKeysym(s.c_str());
        bool need_shift = false;
        if (ks == NoSymbol && c >= 'A' && c <= 'Z') {
            s[0] = char(std::tolower(c));
            ks = XStringToKeysym(s.c_str());
            need_shift = true;
        }
        if (ks == NoSymbol) continue; // ponytail: unmappable char, skip (see file header)
        KeyCode kc = XKeysymToKeycode(d, ks);
        if (!kc) continue;
        if (need_shift) XTestFakeKeyEvent(d, shift, True, CurrentTime);
        XTestFakeKeyEvent(d, kc, True, CurrentTime);
        XTestFakeKeyEvent(d, kc, False, CurrentTime);
        if (need_shift) XTestFakeKeyEvent(d, shift, False, CurrentTime);
    }
    XFlush(d);
}

// ── Window management (Xlib + EWMH) ──────────────────────────────────────────

static std::string window_title(Display* d, Window w)
{
    Atom utf8 = XInternAtom(d, "UTF8_STRING", False);
    Atom net_wm_name = XInternAtom(d, "_NET_WM_NAME", False);
    Atom type; int format; unsigned long nitems, after; unsigned char* prop = nullptr;
    if (XGetWindowProperty(d, w, net_wm_name, 0, 1024, False, utf8,
                           &type, &format, &nitems, &after, &prop) == Success && prop) {
        std::string s(reinterpret_cast<char*>(prop), nitems);
        XFree(prop);
        return s;
    }
    char* name = nullptr;
    if (XFetchName(d, w, &name) && name) {
        std::string s(name);
        XFree(name);
        return s;
    }
    return "";
}

static std::string to_lower(std::string s) { for (char& c : s) c = char(std::tolower((unsigned char)c)); return s; }

static Window find_window_by_title(Display* d, const std::string& needle)
{
    Window root = DefaultRootWindow(d);
    Atom client_list = XInternAtom(d, "_NET_CLIENT_LIST", False);
    Atom type; int format; unsigned long nitems, after; unsigned char* prop = nullptr;
    if (XGetWindowProperty(d, root, client_list, 0, ~0L, False, XA_WINDOW,
                           &type, &format, &nitems, &after, &prop) != Success || !prop)
        return 0;
    Window* windows = reinterpret_cast<Window*>(prop);
    std::string needle_lc = to_lower(needle);
    Window found = 0;
    for (unsigned long i = 0; i < nitems; ++i) {
        std::string title = to_lower(window_title(d, windows[i]));
        if (title.find(needle_lc) != std::string::npos) { found = windows[i]; break; }
    }
    XFree(prop);
    return found;
}

static void send_client_message(Display* d, Window w, const char* atom_name,
                                 long l0, long l1 = 0, long l2 = 0, long l3 = 0, long l4 = 0)
{
    XEvent ev{};
    ev.xclient.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = XInternAtom(d, atom_name, False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = l0; ev.xclient.data.l[1] = l1; ev.xclient.data.l[2] = l2;
    ev.xclient.data.l[3] = l3; ev.xclient.data.l[4] = l4;
    XSendEvent(d, DefaultRootWindow(d), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(d);
}

void LinuxPlatform::window_focus(const std::string& name)
{
    Display* d = dpy(m_display);
    Window w = find_window_by_title(d, name);
    if (!w) throw std::runtime_error("window not found: " + name);
    XRaiseWindow(d, w);
    XSetInputFocus(d, w, RevertToParent, CurrentTime);
    send_client_message(d, w, "_NET_ACTIVE_WINDOW", 1 /*source: application*/);
}

void LinuxPlatform::window_move(const std::string& name, int x, int y)
{
    Display* d = dpy(m_display);
    Window w = find_window_by_title(d, name);
    if (!w) throw std::runtime_error("window not found: " + name);
    XMoveWindow(d, w, x, y);
    XFlush(d);
}

void LinuxPlatform::window_resize(const std::string& name, int wd, int ht)
{
    Display* d = dpy(m_display);
    Window w = find_window_by_title(d, name);
    if (!w) throw std::runtime_error("window not found: " + name);
    XResizeWindow(d, w, (unsigned)wd, (unsigned)ht);
    XFlush(d);
}

void LinuxPlatform::window_maximize(const std::string& name)
{
    Display* d = dpy(m_display);
    Window w = find_window_by_title(d, name);
    if (!w) throw std::runtime_error("window not found: " + name);
    Atom horz = XInternAtom(d, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    Atom vert = XInternAtom(d, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    send_client_message(d, w, "_NET_WM_STATE", 1 /*_NET_WM_STATE_ADD*/, long(horz), long(vert), 1);
}

void LinuxPlatform::window_minimize(const std::string& name)
{
    Display* d = dpy(m_display);
    Window w = find_window_by_title(d, name);
    if (!w) throw std::runtime_error("window not found: " + name);
    XIconifyWindow(d, w, DefaultScreen(d));
    XFlush(d);
}

// ── App ──────────────────────────────────────────────────────────────────────

static void spawn_detached(const char* file, char* const argv[])
{
    pid_t pid;
    if (posix_spawnp(&pid, file, nullptr, nullptr, argv, environ) != 0)
        throw std::runtime_error(std::string("failed to spawn: ") + file);
}

void LinuxPlatform::app_run(const std::string& command)
{
    char* argv[] = {const_cast<char*>("/bin/sh"), const_cast<char*>("-c"),
                    const_cast<char*>(command.c_str()), nullptr};
    spawn_detached("/bin/sh", argv);
}

void LinuxPlatform::app_open(const std::string& name)
{
    char* argv[] = {const_cast<char*>("xdg-open"), const_cast<char*>(name.c_str()), nullptr};
    spawn_detached("xdg-open", argv);
}

void LinuxPlatform::app_close(const std::string& name)
{
    Display* d = dpy(m_display);
    Window w = find_window_by_title(d, name);
    if (!w) throw std::runtime_error("window not found: " + name);
    send_client_message(d, w, "_NET_CLOSE_WINDOW", 0);
}

// ── Screen capture ───────────────────────────────────────────────────────────

static void write_png(const std::string& file, int w, int h, const std::vector<unsigned char>& rgb)
{
    FILE* fp = std::fopen(file.c_str(), "wb");
    if (!fp) throw std::runtime_error("cannot open file for screen capture: " + file);

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png ? png_create_info_struct(png) : nullptr;
    if (!png || !info || setjmp(png_jmpbuf(png))) {
        if (fp) std::fclose(fp);
        throw std::runtime_error("PNG encode failed: " + file);
    }
    png_init_io(png, fp);
    png_set_IHDR(png, info, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    std::vector<png_bytep> rows(h);
    for (int i = 0; i < h; ++i) rows[i] = const_cast<png_bytep>(&rgb[size_t(i) * w * 3]);
    png_write_image(png, rows.data());
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    std::fclose(fp);
}

void LinuxPlatform::screen_capture(const std::string& file, int x1, int y1, int x2, int y2)
{
    Display* d = dpy(m_display);
    Window root = DefaultRootWindow(d);
    int screen = DefaultScreen(d);
    int full_w = DisplayWidth(d, screen), full_h = DisplayHeight(d, screen);

    int x = x1, y = y1, w = (x2 > x1) ? (x2 - x1) : full_w, h = (y2 > y1) ? (y2 - y1) : full_h;
    if (x1 == 0 && y1 == 0 && x2 == 0 && y2 == 0) { x = 0; y = 0; w = full_w; h = full_h; }

    XImage* img = XGetImage(d, root, x, y, (unsigned)w, (unsigned)h, AllPlanes, ZPixmap);
    if (!img) throw std::runtime_error("XGetImage failed for screen capture");

    // ponytail: assumes the common 24/32bpp TrueColor mask layout
    // (0xFF0000/0xFF00/0xFF for R/G/B) rather than reading img->{red,green,blue}_mask
    // shift widths generically — true on virtually every modern X server.
    std::vector<unsigned char> rgb(size_t(w) * h * 3);
    for (int py = 0; py < h; ++py) {
        for (int px = 0; px < w; ++px) {
            unsigned long pixel = XGetPixel(img, px, py);
            size_t off = (size_t(py) * w + px) * 3;
            rgb[off + 0] = (pixel & img->red_mask)   >> 16 & 0xFF;
            rgb[off + 1] = (pixel & img->green_mask) >> 8  & 0xFF;
            rgb[off + 2] = (pixel & img->blue_mask)        & 0xFF;
        }
    }
    XDestroyImage(img);
    write_png(file, w, h, rgb);
}

// ── Clock ────────────────────────────────────────────────────────────────────

void LinuxPlatform::wait(uint64_t ns)
{
    std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
}

uint64_t LinuxPlatform::now_ns()
{
    return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

// ── Calibration overlay (`syn rat calibrate`) ───────────────────────────────────
//
// Fullscreen override-redirect window; a target dot appears at a random spot,
// the user clicks it, and the mouse path in between is sampled. Per movement we
// record distance, dot size, movement time (first motion → click), path
// curvature, whether it overshot, and a tremor proxy — the raw material
// estimate_profile() turns into a RatProfile. Esc aborts.
bool LinuxPlatform::calibrate_rat(int movements, std::vector<CalibrationSample>& out)
{
    Display* d = dpy(m_display);
    if (!d) { std::fprintf(stderr, "rat calibrate: cannot open X display\n"); return false; }

    int screen = DefaultScreen(d);
    int sw = DisplayWidth(d, screen), sh = DisplayHeight(d, screen);
    unsigned long black = BlackPixel(d, screen), white = WhitePixel(d, screen);

    XSetWindowAttributes swa;
    swa.override_redirect = True;         // cover the WM, no decorations
    swa.background_pixel  = black;
    swa.event_mask = ExposureMask | ButtonPressMask | PointerMotionMask | KeyPressMask;
    Window win = XCreateWindow(d, RootWindow(d, screen), 0, 0, sw, sh, 0,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
    XMapRaised(d, win);
    XGrabKeyboard(d, win, True, GrabModeAsync, GrabModeAsync, CurrentTime);
    XGrabPointer(d, win, True, ButtonPressMask | PointerMotionMask,
                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

    GC gc = XCreateGC(d, win, 0, nullptr);
    // Use the GC's built-in default font only. Loading a named font (10x20,
    // fixed, ...) raises a fatal BadName X error on systems with an empty font
    // path (e.g. XWayland with no bitmap fonts installed). The progress bar
    // carries the visual "how much left" cue; the text is a secondary readout.
    std::mt19937 rng(std::random_device{}());
    auto randint = [&](int lo, int hi) { return int(std::uniform_int_distribution<int>(lo, hi)(rng)); };

    auto draw_dot = [&](int cx, int cy, int r, int idx, int total) {
        XClearWindow(d, win);
        XSetForeground(d, gc, white);
        XFillArc(d, win, gc, cx - r, cy - r, 2 * r, 2 * r, 0, 360 * 64);

        // Progress bar (top center): outline + fill for completed movements.
        int bw = sw / 2, bx = (sw - bw) / 2, by = 48, bh = 26;
        int done = idx - 1;                       // idx is 1-based current dot
        XDrawRectangle(d, win, gc, bx, by, bw, bh);
        int fill = int(double(bw - 2) * done / (total > 0 ? total : 1));
        if (fill > 0) XFillRectangle(d, win, gc, bx + 1, by + 1, fill, bh - 1);

        // Counter above the bar, and remaining count. ASCII only — XDrawString
        // is 8-bit and renders multi-byte UTF-8 (e.g. em-dash) as garbage.
        char msg[96];
        std::snprintf(msg, sizeof(msg), "Dot %d of %d  -  %d left  (Esc to cancel)",
                      idx, total, total - done);
        XDrawString(d, win, gc, bx, by - 12, msg, int(std::strlen(msg)));
        XFlush(d);
    };

    const int margin = 80;
    int start_x = sw / 2, start_y = sh / 2;   // first movement starts from center
    bool aborted = false;

    for (int i = 0; i < movements && !aborted; ++i) {
        int r  = randint(8, 22);                       // dot radius (px)
        int tx = randint(margin, sw - margin);
        int ty = randint(margin, sh - margin);
        draw_dot(tx, ty, r, i + 1, movements);

        std::vector<std::pair<double,double>> path;    // sampled cursor positions
        double t_first = -1, t_click = 0;

        bool clicked = false;
        while (!clicked && !aborted) {
            XEvent ev;
            XNextEvent(d, &ev);
            if (ev.type == KeyPress) {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                if (ks == XK_Escape) aborted = true;
            } else if (ev.type == Expose) {
                draw_dot(tx, ty, r, i + 1, movements);
            } else if (ev.type == MotionNotify) {
                // Movement time starts when the cursor actually leaves the start
                // dot, not on the first tremor event — otherwise reaction/aiming
                // dwell inflates it and (since generate() adds hesitation
                // separately) hesitation gets double-counted into fitts_a.
                double ddx = ev.xmotion.x - start_x, ddy = ev.xmotion.y - start_y;
                if (t_first < 0 && std::hypot(ddx, ddy) > 6.0)
                    t_first = double(ev.xmotion.time);
                path.emplace_back(double(ev.xmotion.x), double(ev.xmotion.y));
            } else if (ev.type == ButtonPress) {
                // Only a click that actually lands on the dot counts — a miss
                // isn't a real target acquisition and would pollute the fit, so
                // ignore it and keep waiting on the same dot.
                double mx = ev.xbutton.x - tx, my = ev.xbutton.y - ty;
                if (std::hypot(mx, my) <= r + 6.0) {
                    t_click = double(ev.xbutton.time);
                    path.emplace_back(double(ev.xbutton.x), double(ev.xbutton.y));
                    clicked = true;
                }
            }
        }
        if (aborted) break;

        double dx = tx - start_x, dy = ty - start_y;
        double dist = std::hypot(dx, dy);
        double t_ms = (t_first >= 0) ? (t_click - t_first) : 0.0;
        if (dist < 1.0 || t_ms <= 0.0) { start_x = tx; start_y = ty; continue; }

        // Shape metrics: perpendicular deviation and along-axis projection.
        double ax = dx / dist, ay = dy / dist;         // unit start→target
        double px = -ay, py = ax;                       // unit perpendicular
        double max_perp = 0, sum_perp2 = 0, max_proj = 0;
        int nprev = 0;
        for (auto& pt : path) {
            double rx = pt.first - start_x, ry = pt.second - start_y;
            double perp = rx * px + ry * py;
            double proj = rx * ax + ry * ay;
            max_perp = std::max(max_perp, std::fabs(perp));
            sum_perp2 += perp * perp;
            max_proj = std::max(max_proj, proj);
            ++nprev;
        }
        CalibrationSample s;
        s.distance       = dist;
        s.target_w       = 2.0 * r;
        s.time_ms        = t_ms;
        s.curvature_frac = std::clamp(max_perp / dist, 0.0, 0.5);
        s.tremor         = nprev > 0 ? std::clamp(std::sqrt(sum_perp2 / nprev) / dist * 4.0, 0.05, 1.5) : 0.4;
        s.overshot       = max_proj > dist * 1.02;
        out.push_back(s);

        start_x = tx; start_y = ty;
    }

    XUngrabPointer(d, CurrentTime);
    XUngrabKeyboard(d, CurrentTime);
    XFreeGC(d, gc);
    XDestroyWindow(d, win);
    XFlush(d);
    return !aborted && !out.empty();
}

} // namespace syn

// dlsym'd by main.cpp — C linkage so the symbol name is predictable and
// unmangled. Built into a separate plugin (syn_linux_platform.so) rather
// than linked into `syn` directly; see CMakeLists.txt for why.
extern "C" syn::Platform* syn_create_linux_platform()
{
    return new syn::LinuxPlatform(); // ponytail: process-lifetime singleton, intentionally never deleted
}
