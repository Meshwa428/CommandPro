#include "platform/platform.h"
#include "platform/keycodes.h"
#include <iostream>
#include <vector>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <linux/uinput.h>
#include <cstring>
#include <stdexcept>

namespace Synapse {

class LinuxUInputPlatform : public IPlatform {
    int fd;
    Point currentPos = {0, 0};
    const int MAX_ABS = 32767;

    void sendEvent(int type, int code, int value) {
        struct input_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = type;
        ev.code = code;
        ev.value = value;
        write(fd, &ev, sizeof(ev));

        // Synchronize
        ev.type = EV_SYN;
        ev.code = SYN_REPORT;
        ev.value = 0;
        write(fd, &ev, sizeof(ev));
    }

public:
    LinuxUInputPlatform() {
        fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (fd < 0) {
            throw std::runtime_error("Could not open /dev/uinput. Try 'sudo chmod +0666 /dev/uinput'");
        }

        // Enable key events (mouse buttons and keyboard)
        ioctl(fd, UI_SET_EVBIT, EV_KEY);
        ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
        ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);
        ioctl(fd, UI_SET_KEYBIT, BTN_MIDDLE);
        
        for (auto const& [name, code] : LINUX_KEY_MAP) {
            ioctl(fd, UI_SET_KEYBIT, code);
        }

        // Enable absolute movement
        ioctl(fd, UI_SET_EVBIT, EV_ABS);
        ioctl(fd, UI_SET_ABSBIT, ABS_X);
        ioctl(fd, UI_SET_ABSBIT, ABS_Y);

        struct uinput_abs_setup abs_x, abs_y;
        memset(&abs_x, 0, sizeof(abs_x));
        abs_x.code = ABS_X;
        abs_x.absinfo.minimum = 0;
        abs_x.absinfo.maximum = MAX_ABS;
        ioctl(fd, UI_ABS_SETUP, &abs_x);

        memset(&abs_y, 0, sizeof(abs_y));
        abs_y.code = ABS_Y;
        abs_y.absinfo.minimum = 0;
        abs_y.absinfo.maximum = MAX_ABS;
        ioctl(fd, UI_ABS_SETUP, &abs_y);

        struct uinput_setup usetup;
        memset(&usetup, 0, sizeof(usetup));
        usetup.id.bustype = BUS_USB;
        usetup.id.vendor  = 0x1234;
        usetup.id.product = 0x5678;
        strcpy(usetup.name, "Synapse Virtual Input");

        ioctl(fd, UI_DEV_SETUP, &usetup);
        ioctl(fd, UI_DEV_CREATE);
        
        // Wait a bit for the OS to recognize the device
        usleep(100000);
    }

    ~LinuxUInputPlatform() override {
        if (fd >= 0) {
            ioctl(fd, UI_DEV_DESTROY);
            close(fd);
        }
    }

    void mouseMove(int x, int y) override {
        // We assume a virtual screen size for mapping. 
        // In real Wayland, absolute pointers are usually normalized.
        // We'll map (x,y) assuming a 1920x1080 target for now, 
        // but better would be to have user define resolution.
        // For MVP, we use 1920x1080 as base.
        int absX = (x * MAX_ABS) / 1920; 
        int absY = (y * MAX_ABS) / 1080;
        
        sendEvent(EV_ABS, ABS_X, absX);
        sendEvent(EV_ABS, ABS_Y, absY);
        currentPos = {x, y};
    }

    void mousePress(MouseButton button) override {
        int code = BTN_LEFT;
        if (button == MouseButton::RIGHT) code = BTN_RIGHT;
        if (button == MouseButton::MIDDLE) code = BTN_MIDDLE;
        sendEvent(EV_KEY, code, 1);
    }

    void mouseRelease(MouseButton button) override {
        int code = BTN_LEFT;
        if (button == MouseButton::RIGHT) code = BTN_RIGHT;
        if (button == MouseButton::MIDDLE) code = BTN_MIDDLE;
        sendEvent(EV_KEY, code, 0);
    }

    void mouseClick(MouseButton button) override {
        mousePress(button);
        usleep(50000);
        mouseRelease(button);
    }

    Point getMousePosition() override {
        return currentPos;
    }

    void keyPress(const std::string& key) override {
        auto it = LINUX_KEY_MAP.find(key);
        if (it == LINUX_KEY_MAP.end()) {
             std::cerr << "Warning: Unknown uinput key: " << key << std::endl;
             return;
        }
        sendEvent(EV_KEY, it->second, 1);
        usleep(20000);
        sendEvent(EV_KEY, it->second, 0);
    }

    void keyType(const std::string& text) override {
        for (char c : text) {
            std::string key(1, c);
            if (c == ' ') key = "space";
            keyPress(key);
            usleep(10000);
        }
    }
};

std::shared_ptr<IPlatform> createUInputPlatform() {
    return std::make_shared<LinuxUInputPlatform>();
}

} // namespace Synapse
