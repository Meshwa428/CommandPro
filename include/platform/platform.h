#pragma once
#include <string>
#include <memory>

namespace Synapse {

enum class MouseButton {
    LEFT = 1,
    MIDDLE = 2,
    RIGHT = 3,
    SCROLL_UP = 4,
    SCROLL_DOWN = 5
};

class IPlatform {
public:
    virtual ~IPlatform() = default;

    // Mouse Automation
    virtual void mouseMove(int x, int y) = 0;
    virtual void mouseClick(MouseButton button) = 0;
    virtual void mousePress(MouseButton button) = 0;
    virtual void mouseRelease(MouseButton button) = 0;
    
    struct Point { int x, y; };
    virtual Point getMousePosition() = 0;

    // Keyboard Automation
    virtual void keyPress(const std::string& key) = 0;
    virtual void keyType(const std::string& text) = 0;

    // Factory method to get current OS platform
    static std::shared_ptr<IPlatform> create();
};

} // namespace Synapse
