#include "uinput_device.h"

#include <linux/uinput.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <vector>

namespace syn {

// ── US-layout key tables ────────────────────────────────────────────────────────

// Printable ASCII → (Linux key code, needs shift). Linux key codes are not
// alphabetical, so this is an explicit map.
static const std::unordered_map<char, std::pair<int,bool>>& printable_keys()
{
    static const std::unordered_map<char, std::pair<int,bool>> m = [] {
        std::unordered_map<char, std::pair<int,bool>> t;
        struct L { char c; int code; };
        const L letters[] = {
            {'a',KEY_A},{'b',KEY_B},{'c',KEY_C},{'d',KEY_D},{'e',KEY_E},{'f',KEY_F},
            {'g',KEY_G},{'h',KEY_H},{'i',KEY_I},{'j',KEY_J},{'k',KEY_K},{'l',KEY_L},
            {'m',KEY_M},{'n',KEY_N},{'o',KEY_O},{'p',KEY_P},{'q',KEY_Q},{'r',KEY_R},
            {'s',KEY_S},{'t',KEY_T},{'u',KEY_U},{'v',KEY_V},{'w',KEY_W},{'x',KEY_X},
            {'y',KEY_Y},{'z',KEY_Z},
        };
        for (auto& l : letters) {
            t[l.c]              = {l.code, false};
            t[char(std::toupper(l.c))] = {l.code, true};
        }
        // digits and their shifted symbols
        const char* digits = "1234567890";
        const int   dcodes[] = {KEY_1,KEY_2,KEY_3,KEY_4,KEY_5,KEY_6,KEY_7,KEY_8,KEY_9,KEY_0};
        const char* dshift = "!@#$%^&*()";
        for (int i = 0; i < 10; ++i) {
            t[digits[i]] = {dcodes[i], false};
            t[dshift[i]] = {dcodes[i], true};
        }
        // punctuation: {char, code, unshifted?}
        struct P { char c; int code; bool shift; };
        const P punct[] = {
            {' ',KEY_SPACE,false}, {'-',KEY_MINUS,false}, {'_',KEY_MINUS,true},
            {'=',KEY_EQUAL,false}, {'+',KEY_EQUAL,true},  {'[',KEY_LEFTBRACE,false},
            {'{',KEY_LEFTBRACE,true}, {']',KEY_RIGHTBRACE,false}, {'}',KEY_RIGHTBRACE,true},
            {'\\',KEY_BACKSLASH,false}, {'|',KEY_BACKSLASH,true}, {';',KEY_SEMICOLON,false},
            {':',KEY_SEMICOLON,true}, {'\'',KEY_APOSTROPHE,false}, {'"',KEY_APOSTROPHE,true},
            {'`',KEY_GRAVE,false}, {'~',KEY_GRAVE,true}, {',',KEY_COMMA,false},
            {'<',KEY_COMMA,true}, {'.',KEY_DOT,false}, {'>',KEY_DOT,true},
            {'/',KEY_SLASH,false}, {'?',KEY_SLASH,true},
        };
        for (auto& p : punct) t[p.c] = {p.code, p.shift};
        return t;
    }();
    return m;
}

// Named non-letter keys ("enter", "tab", ...) → key code.
static int named_key(const std::string& k)
{
    static const std::unordered_map<std::string,int> m = {
        {"enter",KEY_ENTER},{"return",KEY_ENTER},{"escape",KEY_ESC},{"esc",KEY_ESC},
        {"tab",KEY_TAB},{"space",KEY_SPACE},{"backspace",KEY_BACKSPACE},{"delete",KEY_DELETE},
        {"up",KEY_UP},{"down",KEY_DOWN},{"left",KEY_LEFT},{"right",KEY_RIGHT},
        {"home",KEY_HOME},{"end",KEY_END},{"pageup",KEY_PAGEUP},{"pagedown",KEY_PAGEDOWN},
    };
    auto it = m.find(k);
    return it == m.end() ? 0 : it->second;
}

static int modifier_key(const std::string& mod)
{
    std::string m = mod;
    for (char& c : m) c = char(std::tolower((unsigned char)c));
    if (m == "ctrl" || m == "control") return KEY_LEFTCTRL;
    if (m == "shift")                  return KEY_LEFTSHIFT;
    if (m == "alt")                    return KEY_LEFTALT;
    if (m == "super" || m == "win" || m == "cmd") return KEY_LEFTMETA;
    return 0;
}

// "ctrl+shift+a" → ([ctrl,shift], "a")
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

// Resolve a chord's key token to (code, needs_shift). Single printable chars go
// through the printable table; multi-char tokens are named keys.
static bool resolve_key(const std::string& key, int& code, bool& shift)
{
    shift = false;
    if (key.size() == 1) {
        auto& tbl = printable_keys();
        auto it = tbl.find(key[0]);
        if (it == tbl.end()) return false;
        code = it->second.first; shift = it->second.second;
        return true;
    }
    code = named_key(key);
    return code != 0;
}

// ── Device lifecycle ────────────────────────────────────────────────────────────

UinputDevice::~UinputDevice()
{
    if (m_fd >= 0) {
        ioctl(m_fd, UI_DEV_DESTROY);
        close(m_fd);
    }
    for (int fd : m_phys_fds) close(fd);
}

bool UinputDevice::ensure_open(int screen_w, int screen_h)
{
    if (m_fd >= 0) return true;
    if (screen_w > 0) m_sw = screen_w;
    if (screen_h > 0) m_sh = screen_h;

    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) return false;

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    ioctl(fd, UI_SET_EVBIT, EV_REL);
    ioctl(fd, UI_SET_EVBIT, EV_SYN);

    ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);
    ioctl(fd, UI_SET_KEYBIT, BTN_MIDDLE);
    ioctl(fd, UI_SET_ABSBIT, ABS_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_Y);
    ioctl(fd, UI_SET_RELBIT, REL_WHEEL);

    // Enable every key code we might emit.
    for (auto& kv : printable_keys()) ioctl(fd, UI_SET_KEYBIT, kv.second.first);
    for (int code : {KEY_ENTER,KEY_ESC,KEY_TAB,KEY_SPACE,KEY_BACKSPACE,KEY_DELETE,
                     KEY_UP,KEY_DOWN,KEY_LEFT,KEY_RIGHT,KEY_HOME,KEY_END,KEY_PAGEUP,
                     KEY_PAGEDOWN,KEY_LEFTCTRL,KEY_LEFTSHIFT,KEY_LEFTALT,KEY_LEFTMETA})
        ioctl(fd, UI_SET_KEYBIT, code);

    struct uinput_user_dev uidev;
    std::memset(&uidev, 0, sizeof(uidev));
    std::strncpy(uidev.name, "Synapse Virtual Input", UINPUT_MAX_NAME_SIZE - 1);
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor  = 0x1;
    uidev.id.product = 0x1;
    uidev.id.version = 1;
    uidev.absmin[ABS_X] = 0; uidev.absmax[ABS_X] = 65535;
    uidev.absmin[ABS_Y] = 0; uidev.absmax[ABS_Y] = 65535;

    if (write(fd, &uidev, sizeof(uidev)) != (ssize_t)sizeof(uidev)) { close(fd); return false; }
    if (ioctl(fd, UI_DEV_CREATE) < 0) { close(fd); return false; }

    m_fd = fd;
    // Give the compositor a moment to notice the new device before first use.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return true;
}

void UinputDevice::emit(unsigned type, unsigned code, int value)
{
    if (m_fd < 0) return;
    struct input_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.type = type; ev.code = code; ev.value = value;
    ssize_t n = write(m_fd, &ev, sizeof(ev));
    (void)n;
}

void UinputDevice::sync() { emit(EV_SYN, SYN_REPORT, 0); }

// ── Mouse ────────────────────────────────────────────────────────────────────

void UinputDevice::move_abs(int x, int y)
{
    if (x < 0) x = 0;
    if (x >= m_sw) x = m_sw - 1;
    if (y < 0) y = 0;
    if (y >= m_sh) y = m_sh - 1;
    int denom_x = m_sw > 1 ? m_sw - 1 : 1;
    int denom_y = m_sh > 1 ? m_sh - 1 : 1;
    emit(EV_ABS, ABS_X, int(int64_t(x) * 65535 / denom_x));
    emit(EV_ABS, ABS_Y, int(int64_t(y) * 65535 / denom_y));
    sync();
    m_last_x = x; m_last_y = y; m_have_pos = true;  // authoritative position
}

// Open every physical relative-pointer under /dev/input (once). A device is a
// mouse if it reports EV_REL with REL_X/REL_Y. Our own virtual device declares
// only REL_WHEEL, so it is skipped. Non-blocking so drain never stalls. Missing
// read permission on some nodes is fine — we just track fewer devices.
void UinputDevice::scan_physical_pointers()
{
    if (m_phys_scanned) return;
    m_phys_scanned = true;
    DIR* dir = opendir("/dev/input");
    if (!dir) return;
    for (dirent* de; (de = readdir(dir)); ) {
        if (std::strncmp(de->d_name, "event", 5) != 0) continue;
        std::string path = std::string("/dev/input/") + de->d_name;
        int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        unsigned long evbit = 0, relbit = 0;
        if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), &evbit) < 0 ||
            !(evbit & (1UL << EV_REL)) ||
            ioctl(fd, EVIOCGBIT(EV_REL, sizeof(relbit)), &relbit) < 0 ||
            !(relbit & (1UL << REL_X)) || !(relbit & (1UL << REL_Y))) {
            ::close(fd);
            continue;
        }
        m_phys_fds.push_back(fd);
    }
    closedir(dir);
}

// Fold any pending physical mouse motion into the tracked position. Raw REL
// deltas are device counts (the compositor applies its own acceleration to the
// visible cursor, which we can't see natively) — good enough to notice the user
// moved and re-anchor: the next absolute move corrects any drift exactly.
void UinputDevice::drain_physical()
{
    scan_physical_pointers();
    long dx = 0, dy = 0;
    input_event evs[64];
    for (int fd : m_phys_fds) {
        for (;;) {
            ssize_t n = ::read(fd, evs, sizeof(evs));
            if (n <= 0) break;
            for (size_t i = 0; i < n / sizeof(input_event); ++i) {
                if (evs[i].type != EV_REL) continue;
                if (evs[i].code == REL_X) dx += evs[i].value;
                else if (evs[i].code == REL_Y) dy += evs[i].value;
            }
            if (size_t(n) < sizeof(evs)) break;  // drained this device
        }
    }
    if (dx || dy) {
        m_last_x = std::clamp(m_last_x + int(dx), 0, m_sw - 1);
        m_last_y = std::clamp(m_last_y + int(dy), 0, m_sh - 1);
        m_have_pos = true;
    }
}

std::pair<int,int> UinputDevice::current_pos()
{
    drain_physical();
    return {m_last_x, m_last_y};
}

void UinputDevice::button(const std::string& name, bool down)
{
    int code = name == "right" ? BTN_RIGHT : name == "middle" ? BTN_MIDDLE : BTN_LEFT;
    emit(EV_KEY, code, down ? 1 : 0);
    sync();
}

void UinputDevice::scroll(const std::string& direction, int clicks)
{
    int dir = direction == "down" ? -1 : 1;  // REL_WHEEL: +1 up, -1 down
    for (int i = 0; i < clicks; ++i) { emit(EV_REL, REL_WHEEL, dir); sync(); }
}

// ── Keyboard ─────────────────────────────────────────────────────────────────

void UinputDevice::key_press(const std::string& chord)
{
    std::vector<std::string> mods; std::string key;
    parse_chord(chord, mods, key);
    std::vector<int> mod_codes;
    for (auto& m : mods) if (int c = modifier_key(m)) mod_codes.push_back(c);

    int code; bool shift;
    bool have = resolve_key(key, code, shift);

    for (int c : mod_codes) { emit(EV_KEY, c, 1); sync(); }
    if (have && shift) { emit(EV_KEY, KEY_LEFTSHIFT, 1); sync(); }
    if (have) { emit(EV_KEY, code, 1); sync(); emit(EV_KEY, code, 0); sync(); }
    if (have && shift) { emit(EV_KEY, KEY_LEFTSHIFT, 0); sync(); }
    for (auto it = mod_codes.rbegin(); it != mod_codes.rend(); ++it) { emit(EV_KEY, *it, 0); sync(); }
}

void UinputDevice::key_hold(const std::string& chord)
{
    std::vector<std::string> mods; std::string key;
    parse_chord(chord, mods, key);
    for (auto& m : mods) if (int c = modifier_key(m)) { emit(EV_KEY, c, 1); sync(); }
    int code; bool shift;
    if (resolve_key(key, code, shift)) { emit(EV_KEY, code, 1); sync(); }
}

void UinputDevice::key_release(const std::string& chord)
{
    std::vector<std::string> mods; std::string key;
    parse_chord(chord, mods, key);
    int code; bool shift;
    if (resolve_key(key, code, shift)) { emit(EV_KEY, code, 0); sync(); }
    for (auto& m : mods) if (int c = modifier_key(m)) { emit(EV_KEY, c, 0); sync(); }
}

void UinputDevice::key_type(const std::string& text)
{
    auto& tbl = printable_keys();
    for (char ch : text) {
        auto it = tbl.find(ch);
        if (it == tbl.end()) continue;  // unmappable char, skip
        int code = it->second.first; bool shift = it->second.second;
        if (shift) { emit(EV_KEY, KEY_LEFTSHIFT, 1); sync(); }
        emit(EV_KEY, code, 1); sync();
        emit(EV_KEY, code, 0); sync();
        if (shift) { emit(EV_KEY, KEY_LEFTSHIFT, 0); sync(); }
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }
}

}  // namespace syn
