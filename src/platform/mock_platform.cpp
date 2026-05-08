#include "platform/platform.h"
#include <iostream>
#include <vector>
#include <string>

namespace Synapse {

class MockPlatform : public IPlatform {
    Point currentPos = {0, 0};
public:
    void mouseMove(int x, int y) override {
        currentPos = {x, y};
        std::cout << "[MOCK] MOUSE_MOVE " << x << " " << y << std::endl;
    }

    void mousePress(MouseButton button) override {
        std::cout << "[MOCK] MOUSE_PRESS " << static_cast<int>(button) << std::endl;
    }

    void mouseRelease(MouseButton button) override {
        std::cout << "[MOCK] MOUSE_RELEASE " << static_cast<int>(button) << std::endl;
    }

    void mouseClick(MouseButton button) override {
        std::cout << "[MOCK] MOUSE_CLICK " << static_cast<int>(button) << std::endl;
    }

    Point getMousePosition() override {
        return currentPos;
    }

    void keyPress(const std::string& key) override {
        std::cout << "[MOCK] KEY_PRESS " << key << std::endl;
    }

    void keyType(const std::string& text) override {
        std::cout << "[MOCK] KEY_TYPE " << text << std::endl;
    }

    std::vector<std::string> getAvailableApps() override {
        std::cout << "[MOCK] GET_AVAILABLE_APPS" << std::endl;
        return {"MockBrowser", "MockTerm", "MockEditor"};
    }

    void openApp(const std::string& nameOrPath) override {
        std::cout << "[MOCK] OPEN_APP " << nameOrPath << std::endl;
    }
};

// We will modify IPlatform::create() or add a new way to get the mock in main.cpp
// For simplicity, let's just create a static method in MockPlatform
std::shared_ptr<IPlatform> createMockPlatform() {
    return std::make_shared<MockPlatform>();
}

} // namespace Synapse
