#pragma once

namespace syn {

class VM;
class Platform;

// Capability gate for automation natives (design 002 §7's Engine::Options,
// scoped down since there's no Engine wrapper yet — just a plain struct
// passed straight to registration). Embedders/AI sandboxes can run scripts
// with input/screen access disabled.
struct AutomationOptions {
    bool allow_input  = true;
    bool allow_screen = true;
};

// Registers `mouse`, `keyboard`, `window`, `app`, `screen`, `time` as global
// ObjMap namespaces of native methods, routed through `platform`. This is
// what makes the command-statement sugar (`mouse 300, 400`, `press ctrl+c`,
// ...), already desugared by the parser into calls like `mouse.move(...)`,
// actually do something at runtime.
void register_automation_stdlib(VM& vm, Platform* platform, const AutomationOptions& opts);

} // namespace syn
