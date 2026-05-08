#include "platform/platform.h"
#include <stdexcept>

// Since X11 can be messy, we only compile this on Linux
#if defined(__linux__) && !defined(__ANDROID__)

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <unistd.h>
#include <iostream>

namespace Synapse {

class LinuxX11Platform : public IPlatform {
    Display* display;
public:
    LinuxX11Platform() {
        display = XOpenDisplay(NULL);
        if (!display) {
            throw std::runtime_error("Failed to open X11 display. Are you running under Wayland without XWayland?");
        }
    }

    ~LinuxX11Platform() override {
        if (display) {
            XCloseDisplay(display);
        }
    }

    void mouseMove(int x, int y) override {
        XTestFakeMotionEvent(display, -1, x, y, CurrentTime);
        XFlush(display);
    }

    void mousePress(MouseButton button) override {
        XTestFakeButtonEvent(display, static_cast<unsigned int>(button), True, CurrentTime);
        XFlush(display);
    }

    void mouseRelease(MouseButton button) override {
        XTestFakeButtonEvent(display, static_cast<unsigned int>(button), False, CurrentTime);
        XFlush(display);
    }

    void mouseClick(MouseButton button) override {
        mousePress(button);
        usleep(50000); // 50ms wait
        mouseRelease(button);
    }

    Point getMousePosition() override {
        Window root, child;
        int root_x, root_y, win_x, win_y;
        unsigned int mask;
        if (XQueryPointer(display, DefaultRootWindow(display), &root, &child, &root_x, &root_y, &win_x, &win_y, &mask)) {
            return {root_x, root_y};
        }
        return {0, 0};
    }

    void keyPress(const std::string& key) override {
        KeySym sym = XStringToKeysym(key.c_str());
        if (sym == NoSymbol) {
            throw std::runtime_error("Unknown key: " + key);
        }
        KeyCode code = XKeysymToKeycode(display, sym);
        if (code == 0) {
            throw std::runtime_error("No keycode found for: " + key);
        }

        XTestFakeKeyEvent(display, code, True, CurrentTime);
        XFlush(display);
        usleep(50000);
        XTestFakeKeyEvent(display, code, False, CurrentTime);
        XFlush(display);
    }

    void keyType(const std::string& text) override {
        for (char c : text) {
            std::string key(1, c);
            // Handle space specially
            if (c == ' ') key = "space";
            
            try {
                keyPress(key);
            } catch (...) {
                // If it fails (e.g. uppercase requiring shift), we might need more advanced mapping.
                // For MVP, we ignore unknown characters.
                std::cerr << "Warning: Could not type character '" << c << "'\n";
            }
            usleep(20000); // small delay between keystrokes
        }
    }
};

std::shared_ptr<IPlatform> IPlatform::create() {
    const char* sessionType = std::getenv("XDG_SESSION_TYPE");
    if (sessionType && std::string(sessionType) == "wayland") {
        try {
            extern std::shared_ptr<IPlatform> createUInputPlatform();
            return createUInputPlatform();
        } catch (const std::exception& e) {
            std::cerr << "[Warning] Wayland detected but uinput failed: " << e.what() << "\n";
            std::cerr << "[Warning] Falling back to X11 (will likely have limited functionality)\n";
        }
    }
    return std::make_shared<LinuxX11Platform>();
}

} // namespace Synapse

#else

// Stub for Windows/Mac (Will implement Windows later)
namespace Synapse {
class StubPlatform : public IPlatform {
public:
    void mouseMove(int, int) override {}
    void mouseClick(MouseButton) override {}
    void mousePress(MouseButton) override {}
    void mouseRelease(MouseButton) override {}
    void keyPress(const std::string&) override {}
    void keyType(const std::string&) override {}
};
std::shared_ptr<IPlatform> IPlatform::create() {
    return std::make_shared<StubPlatform>();
}
} // namespace Synapse

#endif
