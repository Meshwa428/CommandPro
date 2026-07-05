#include "windows_platform.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

// GDI+ needs min/max in scope and objidl before gdiplus.
#include <objidl.h>
#include <algorithm>
namespace Gdiplus { using std::min; using std::max; }
#include <gdiplus.h>

#include <cctype>
#include <chrono>
#include <cwctype>
#include <stdexcept>
#include <thread>
#include <vector>

namespace syn {

// ── Mouse ────────────────────────────────────────────────────────────────────
//
// SendInput absolute coordinates are normalized to 0..65535 over the VIRTUAL
// desktop (all monitors), so multi-monitor setups address every screen.

static void send_mouse(DWORD flags, DWORD data = 0, LONG dx = 0, LONG dy = 0)
{
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = flags;
    in.mi.mouseData = data;
    in.mi.dx = dx;
    in.mi.dy = dy;
    SendInput(1, &in, sizeof(INPUT));
}

static void move_abs(int x, int y)
{
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (vw <= 1) vw = 2;
    if (vh <= 1) vh = 2;
    LONG nx = LONG((double(x - vx) * 65535.0) / double(vw - 1));
    LONG ny = LONG((double(y - vy) * 65535.0) / double(vh - 1));
    send_mouse(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK, 0, nx, ny);
}

void WindowsPlatform::mouse_move_to(int x, int y) { move_abs(x, y); }

void WindowsPlatform::replay_waypoints(const std::vector<Waypoint>& path)
{
    uint32_t last_t = 0;
    for (const auto& wp : path) {
        if (wp.t_ms > last_t) {
            std::this_thread::sleep_for(std::chrono::milliseconds(wp.t_ms - last_t));
            last_t = wp.t_ms;
        }
        move_abs(wp.x, wp.y);
    }
}

static void button_flags(const std::string& button, DWORD& down, DWORD& up)
{
    if (button == "right")       { down = MOUSEEVENTF_RIGHTDOWN;  up = MOUSEEVENTF_RIGHTUP; }
    else if (button == "middle") { down = MOUSEEVENTF_MIDDLEDOWN; up = MOUSEEVENTF_MIDDLEUP; }
    else                         { down = MOUSEEVENTF_LEFTDOWN;   up = MOUSEEVENTF_LEFTUP; }
}

void WindowsPlatform::mouse_click(const std::string& button, int count)
{
    DWORD down, up;
    button_flags(button, down, up);
    for (int i = 0; i < count; ++i) {
        send_mouse(down);
        send_mouse(up);
        if (i + 1 < count) std::this_thread::sleep_for(std::chrono::milliseconds(60));
    }
}

void WindowsPlatform::mouse_hold(const std::string& button)
{
    DWORD down, up;
    button_flags(button, down, up);
    send_mouse(down);
}

void WindowsPlatform::mouse_release(const std::string& button)
{
    DWORD down, up;
    button_flags(button, down, up);
    send_mouse(up);
}

void WindowsPlatform::mouse_scroll(const std::string& direction, int amount)
{
    for (int i = 0; i < amount; ++i) {
        if (direction == "left")       send_mouse(MOUSEEVENTF_HWHEEL, DWORD(-WHEEL_DELTA));
        else if (direction == "right") send_mouse(MOUSEEVENTF_HWHEEL, DWORD(WHEEL_DELTA));
        else if (direction == "down")  send_mouse(MOUSEEVENTF_WHEEL,  DWORD(-WHEEL_DELTA));
        else                           send_mouse(MOUSEEVENTF_WHEEL,  DWORD(WHEEL_DELTA));
    }
}

std::pair<int,int> WindowsPlatform::mouse_position()
{
    POINT p{};
    GetCursorPos(&p);
    return {int(p.x), int(p.y)};
}

// ── Keyboard ─────────────────────────────────────────────────────────────────

static void send_key(WORD vk, bool down)
{
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    SendInput(1, &in, sizeof(INPUT));
}

static std::string to_lower(std::string s)
{
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static WORD modifier_vk(const std::string& mod)
{
    std::string m = to_lower(mod);
    if (m == "ctrl" || m == "control") return VK_CONTROL;
    if (m == "shift")                  return VK_SHIFT;
    if (m == "alt")                    return VK_MENU;
    if (m == "super" || m == "win" || m == "cmd") return VK_LWIN;
    return 0;
}

// Named non-letter keys the parser's key_chord grammar allows through.
static WORD named_key_vk(const std::string& key)
{
    static const std::pair<const char*, WORD> table[] = {
        {"enter", VK_RETURN}, {"return", VK_RETURN}, {"escape", VK_ESCAPE}, {"esc", VK_ESCAPE},
        {"tab", VK_TAB}, {"space", VK_SPACE}, {"backspace", VK_BACK}, {"delete", VK_DELETE},
        {"up", VK_UP}, {"down", VK_DOWN}, {"left", VK_LEFT}, {"right", VK_RIGHT},
        {"home", VK_HOME}, {"end", VK_END}, {"pageup", VK_PRIOR}, {"pagedown", VK_NEXT},
    };
    for (auto& [name, vk] : table) if (key == name) return vk;
    if (key.size() == 1) {
        SHORT scan = VkKeyScanA(key[0]);
        if (scan != -1) return WORD(scan & 0xFF);
    }
    // F-keys: "f1".."f24"
    if (key.size() >= 2 && (key[0] == 'f' || key[0] == 'F')) {
        int n = std::atoi(key.c_str() + 1);
        if (n >= 1 && n <= 24) return WORD(VK_F1 + n - 1);
    }
    return 0;
}

// Splits "ctrl+shift+a" into ([VK_CONTROL, VK_SHIFT], "a"). Same syntax as
// LinuxPlatform::parse_chord.
static void parse_chord(const std::string& chord, std::vector<WORD>& mods, std::string& key)
{
    size_t start = 0;
    while (true) {
        size_t plus = chord.find('+', start);
        std::string part = chord.substr(start, plus == std::string::npos ? std::string::npos : plus - start);
        if (plus == std::string::npos) { key = part; break; }
        if (WORD vk = modifier_vk(part)) mods.push_back(vk);
        start = plus + 1;
    }
}

void WindowsPlatform::key_press(const std::string& chord)
{
    std::vector<WORD> mods; std::string key;
    parse_chord(chord, mods, key);
    WORD vk = named_key_vk(to_lower(key));

    for (WORD m : mods) send_key(m, true);
    if (vk) { send_key(vk, true); send_key(vk, false); }
    for (auto it = mods.rbegin(); it != mods.rend(); ++it) send_key(*it, false);
}

void WindowsPlatform::key_hold(const std::string& chord)
{
    std::vector<WORD> mods; std::string key;
    parse_chord(chord, mods, key);
    for (WORD m : mods) send_key(m, true);
    if (WORD vk = named_key_vk(to_lower(key))) send_key(vk, true);
}

void WindowsPlatform::key_release(const std::string& chord)
{
    std::vector<WORD> mods; std::string key;
    parse_chord(chord, mods, key);
    for (WORD m : mods) send_key(m, false);
    if (WORD vk = named_key_vk(to_lower(key))) send_key(vk, false);
}

static std::wstring utf8_to_wide(const std::string& s)
{
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring w(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), n);
    return w;
}

static std::string wide_to_utf8(const std::wstring& w)
{
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(size_t(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

void WindowsPlatform::key_type(const std::string& text)
{
    // KEYEVENTF_UNICODE types arbitrary text (incl. non-ASCII) without
    // layout lookups — strictly better than the X11 backend's ASCII limit.
    std::wstring w = utf8_to_wide(text);
    for (wchar_t c : w) {
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wScan = WORD(c);
        in.ki.dwFlags = KEYEVENTF_UNICODE;
        SendInput(1, &in, sizeof(INPUT));
        in.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        SendInput(1, &in, sizeof(INPUT));
    }
}

// ── Window management ────────────────────────────────────────────────────────

struct FindCtx {
    std::string needle_lc; // lowercase utf-8
    HWND        found = nullptr;
};

static BOOL CALLBACK enum_proc(HWND hwnd, LPARAM lp)
{
    auto* ctx = reinterpret_cast<FindCtx*>(lp);
    if (!IsWindowVisible(hwnd)) return TRUE;
    wchar_t buf[512];
    int n = GetWindowTextW(hwnd, buf, 512);
    if (n <= 0) return TRUE;
    std::string title = to_lower(wide_to_utf8(std::wstring(buf, size_t(n))));
    if (title.find(ctx->needle_lc) != std::string::npos) {
        ctx->found = hwnd;
        return FALSE; // stop enumeration
    }
    return TRUE;
}

static HWND find_window_by_title(const std::string& needle)
{
    FindCtx ctx;
    ctx.needle_lc = to_lower(needle);
    EnumWindows(enum_proc, reinterpret_cast<LPARAM>(&ctx));
    return ctx.found;
}

static HWND require_window(const std::string& name)
{
    HWND w = find_window_by_title(name);
    if (!w) throw std::runtime_error("window not found: " + name);
    return w;
}

void WindowsPlatform::window_focus(const std::string& name)
{
    HWND w = require_window(name);
    if (IsIconic(w)) ShowWindow(w, SW_RESTORE);
    SetForegroundWindow(w);
}

void WindowsPlatform::window_move(const std::string& name, int x, int y)
{
    HWND w = require_window(name);
    SetWindowPos(w, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void WindowsPlatform::window_resize(const std::string& name, int wd, int ht)
{
    HWND w = require_window(name);
    SetWindowPos(w, nullptr, 0, 0, wd, ht, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void WindowsPlatform::window_maximize(const std::string& name)
{
    ShowWindow(require_window(name), SW_MAXIMIZE);
}

void WindowsPlatform::window_minimize(const std::string& name)
{
    ShowWindow(require_window(name), SW_MINIMIZE);
}

// ── App ──────────────────────────────────────────────────────────────────────

void WindowsPlatform::app_run(const std::string& command)
{
    // cmd /C mirrors the Linux backend's /bin/sh -c
    std::wstring cmdline = L"cmd.exe /C " + utf8_to_wide(command);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, cmdline.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        throw std::runtime_error("failed to spawn: " + command);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

void WindowsPlatform::app_open(const std::string& name)
{
    // ShellExecute mirrors xdg-open: files, URLs, and app names all work
    auto r = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open", utf8_to_wide(name).c_str(),
                      nullptr, nullptr, SW_SHOWNORMAL));
    if (r <= 32) throw std::runtime_error("failed to open: " + name);
}

void WindowsPlatform::app_close(const std::string& name)
{
    PostMessageW(require_window(name), WM_CLOSE, 0, 0);
}

// ── Screen capture (GDI BitBlt + GDI+ PNG encoder) ──────────────────────────

static void gdiplus_init()
{
    static ULONG_PTR token = 0;
    if (token) return;
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok)
        throw std::runtime_error("GDI+ startup failed");
    // ponytail: process-lifetime, never shut down
}

static CLSID png_encoder_clsid()
{
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) throw std::runtime_error("no GDI+ image encoders");
    std::vector<unsigned char> buf(size);
    auto* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
    Gdiplus::GetImageEncoders(num, size, codecs);
    for (UINT i = 0; i < num; ++i)
        if (wcscmp(codecs[i].MimeType, L"image/png") == 0)
            return codecs[i].Clsid;
    throw std::runtime_error("PNG encoder not found");
}

void WindowsPlatform::screen_capture(const std::string& file, int x1, int y1, int x2, int y2)
{
    gdiplus_init();

    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    int x = x1, y = y1;
    int w = (x2 > x1) ? (x2 - x1) : vw;
    int h = (y2 > y1) ? (y2 - y1) : vh;
    if (x1 == 0 && y1 == 0 && x2 == 0 && y2 == 0) { x = vx; y = vy; w = vw; h = vh; }

    HDC screen = GetDC(nullptr);
    HDC mem    = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, w, h);
    HGDIOBJ old = SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, w, h, screen, x, y, SRCCOPY | CAPTUREBLT);
    SelectObject(mem, old);

    {
        Gdiplus::Bitmap image(bmp, nullptr);
        CLSID png = png_encoder_clsid();
        if (image.Save(utf8_to_wide(file).c_str(), &png, nullptr) != Gdiplus::Ok) {
            DeleteObject(bmp); DeleteDC(mem); ReleaseDC(nullptr, screen);
            throw std::runtime_error("PNG encode failed: " + file);
        }
    }

    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
}

// ── Clock ────────────────────────────────────────────────────────────────────

void WindowsPlatform::wait(uint64_t ns)
{
    std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
}

uint64_t WindowsPlatform::now_ns()
{
    return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

} // namespace syn

// GetProcAddress'd by main.cpp — C linkage so the symbol name is predictable.
extern "C" __declspec(dllexport) syn::Platform* syn_create_windows_platform()
{
    return new syn::WindowsPlatform(); // ponytail: process-lifetime singleton, intentionally never deleted
}
