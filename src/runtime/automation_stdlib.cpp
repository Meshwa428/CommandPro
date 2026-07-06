#include "synapse/runtime/automation_stdlib.h"
#include "synapse/runtime/vm.h"
#include "synapse/platform/platform.h"
#include "synapse/rat/rat_model.h"
#include <cstring>
#include <initializer_list>
#include <utility>

namespace syn {

// ── Arg helpers ────────────────────────────────────────────────────────────────
// Natives get a flat positional Value* array (named args are already
// resolved to position by the parser/compiler — see parse_*_cmd() in
// parser.cpp and compile_call()'s field-call path, which compiles Arg.value
// in order and ignores Arg.name entirely).

static const std::string& arg_str(VM& vm, int argc, Value* args, int i, const char* who)
{
    if (i >= argc || !val_is_string(args[i]))
        vm.throw_runtime_error("E0200", std::string(who) + ": expected string argument");
    return as_str(args[i]).data;
}

static int64_t arg_int(VM& vm, int argc, Value* args, int i, const char* who)
{
    if (i >= argc || !args[i].is_int())
        vm.throw_runtime_error("E0201", std::string(who) + ": expected int argument");
    return args[i].as_int();
}

static double arg_num_or(int argc, Value* args, int i, double dflt)
{
    if (i >= argc) return dflt;
    if (args[i].is_int()) return double(args[i].as_int());
    if (args[i].is_float()) return args[i].as_float();
    return dflt;
}

static int64_t arg_int_or(int argc, Value* args, int i, int64_t dflt)
{
    if (i >= argc || !args[i].is_int()) return dflt;
    return args[i].as_int();
}

// ── Namespace builder ────────────────────────────────────────────────────────────

static void add_method(VM& vm, ObjMap* ns, const char* name, NativeFn fn)
{
    auto* native = vm.alloc<ObjNative>(std::string(name), std::move(fn));
    ns->set(Value::from_ptr(vm.intern_string(name, std::strlen(name))), Value::from_ptr(native));
}

// Mutable per-session state shared by mouse.mode / mouse.speed / mouse.move.
// One process = one script run, so plain statics are fine here (no VM
// instancing across threads).
struct MouseState {
    bool   linear_mode  = false; // mouse.mode "linear"
    double speed_bias   = 1.0;   // mouse.speed multiplier
};

void register_automation_stdlib(VM& vm, Platform* platform, const AutomationOptions& opts)
{
    auto require_input = [&vm, opts]() {
        if (!opts.allow_input)
            vm.throw_runtime_error("E0210", "automation: input disabled for this script (Engine::Options.allow_input=false)");
    };
    auto require_screen = [&vm, opts]() {
        if (!opts.allow_screen)
            vm.throw_runtime_error("E0211", "automation: screen access disabled for this script (Engine::Options.allow_screen=false)");
    };

    auto mstate = std::make_shared<MouseState>();

    // ── mouse ────────────────────────────────────────────────────────────────
    ObjMap* mouse = vm.alloc_map();

    add_method(vm, mouse, "move", [&vm, platform, mstate, require_input](int argc, Value* args) -> Value {
        require_input();
        int x = int(arg_int(vm, argc, args, 0, "mouse.move"));
        int y = int(arg_int(vm, argc, args, 1, "mouse.move"));
        // Optional trailing duration: `mouse x, y 3s` sets the movement time.
        double dur_ms = (argc > 2 && args[2].is_duration())
                            ? double(args[2].as_duration()) / 1e6 : 0.0;
        auto [cx, cy] = platform->mouse_position();
        auto path = RatModel::generate(cx, cy, x, y, mstate->speed_bias,
                                       mstate->linear_mode, dur_ms);
        platform->replay_waypoints(path);
        return Value::none_val();
    });

    add_method(vm, mouse, "click", [&vm, platform, require_input](int argc, Value* args) -> Value {
        require_input();
        const std::string& button = arg_str(vm, argc, args, 0, "mouse.click");
        if (argc >= 3) {
            int x = int(arg_int(vm, argc, args, 1, "mouse.click"));
            int y = int(arg_int(vm, argc, args, 2, "mouse.click"));
            platform->mouse_move_to(x, y);
        }
        int count = int(arg_int_or(argc, args, argc >= 3 ? 3 : 1, 1));
        platform->mouse_click(button, count);
        return Value::none_val();
    });

    add_method(vm, mouse, "drag", [&vm, platform, mstate, require_input](int argc, Value* args) -> Value {
        require_input();
        int x1 = int(arg_int(vm, argc, args, 0, "mouse.drag"));
        int y1 = int(arg_int(vm, argc, args, 1, "mouse.drag"));
        int x2 = int(arg_int(vm, argc, args, 2, "mouse.drag"));
        int y2 = int(arg_int(vm, argc, args, 3, "mouse.drag"));
        double speed = arg_num_or(argc, args, 4, 1.0) * mstate->speed_bias;
        platform->mouse_move_to(x1, y1);
        platform->mouse_hold("left");
        auto path = RatModel::generate(x1, y1, x2, y2, speed, mstate->linear_mode);
        platform->replay_waypoints(path);
        platform->mouse_release("left");
        return Value::none_val();
    });

    add_method(vm, mouse, "scroll", [&vm, platform, require_input](int argc, Value* args) -> Value {
        require_input();
        const std::string& dir = arg_str(vm, argc, args, 0, "mouse.scroll");
        int amount = int(arg_int_or(argc, args, 1, 1));
        platform->mouse_scroll(dir, amount);
        return Value::none_val();
    });

    add_method(vm, mouse, "hold", [&vm, platform, require_input](int argc, Value* args) -> Value {
        require_input();
        platform->mouse_hold(arg_str(vm, argc, args, 0, "mouse.hold"));
        return Value::none_val();
    });

    add_method(vm, mouse, "release", [&vm, platform, require_input](int argc, Value* args) -> Value {
        require_input();
        platform->mouse_release(arg_str(vm, argc, args, 0, "mouse.release"));
        return Value::none_val();
    });

    add_method(vm, mouse, "mode", [mstate](int argc, Value* args) -> Value {
        if (argc >= 1 && val_is_string(args[0]))
            mstate->linear_mode = (as_str(args[0]).data == "linear");
        return Value::none_val();
    });

    add_method(vm, mouse, "speed", [mstate](int argc, Value* args) -> Value {
        if (argc >= 1) mstate->speed_bias = arg_num_or(argc, args, 0, 1.0);
        return Value::none_val();
    });

    add_method(vm, mouse, "preview", [&vm, mstate, platform](int argc, Value* args) -> Value {
        int x = int(arg_int(vm, argc, args, 0, "mouse.preview"));
        int y = int(arg_int(vm, argc, args, 1, "mouse.preview"));
        double speed = arg_num_or(argc, args, 2, 1.0) * mstate->speed_bias;
        auto [cx, cy] = platform->mouse_position();
        auto pv = RatModel::preview(cx, cy, x, y, speed);
        ObjMap* m = vm.alloc_map();
        m->set(Value::from_ptr(vm.intern_string("duration_ms", 11)), Value::from_float(pv.duration_ms));
        m->set(Value::from_ptr(vm.intern_string("waypoints", 9)), Value::from_int(pv.waypoints));
        return Value::from_ptr(m);
    });

    vm.define_global("mouse", Value::from_ptr(mouse));

    // ── keyboard ─────────────────────────────────────────────────────────────
    ObjMap* keyboard = vm.alloc_map();

    add_method(vm, keyboard, "press", [&vm, platform, require_input](int argc, Value* args) -> Value {
        require_input();
        platform->key_press(arg_str(vm, argc, args, 0, "keyboard.press"));
        return Value::none_val();
    });
    add_method(vm, keyboard, "hold", [&vm, platform, require_input](int argc, Value* args) -> Value {
        require_input();
        platform->key_hold(arg_str(vm, argc, args, 0, "keyboard.hold"));
        return Value::none_val();
    });
    add_method(vm, keyboard, "release", [&vm, platform, require_input](int argc, Value* args) -> Value {
        require_input();
        platform->key_release(arg_str(vm, argc, args, 0, "keyboard.release"));
        return Value::none_val();
    });
    add_method(vm, keyboard, "type", [&vm, platform, require_input](int argc, Value* args) -> Value {
        require_input();
        platform->key_type(arg_str(vm, argc, args, 0, "keyboard.type"));
        return Value::none_val();
    });

    vm.define_global("keyboard", Value::from_ptr(keyboard));

    // ── window ───────────────────────────────────────────────────────────────
    ObjMap* window = vm.alloc_map();

    add_method(vm, window, "focus", [&vm, platform, require_input](int argc, Value* args) -> Value {
        require_input();
        platform->window_focus(arg_str(vm, argc, args, 0, "window.focus"));
        return Value::none_val();
    });
    add_method(vm, window, "move", [&vm, platform, require_input](int argc, Value* args) -> Value {
        require_input();
        const std::string& name = arg_str(vm, argc, args, 0, "window.move");
        int x = int(arg_int(vm, argc, args, 1, "window.move"));
        int y = int(arg_int(vm, argc, args, 2, "window.move"));
        platform->window_move(name, x, y);
        return Value::none_val();
    });
    add_method(vm, window, "resize", [&vm, platform, require_input](int argc, Value* args) -> Value {
        require_input();
        const std::string& name = arg_str(vm, argc, args, 0, "window.resize");
        int w = int(arg_int(vm, argc, args, 1, "window.resize"));
        int h = int(arg_int(vm, argc, args, 2, "window.resize"));
        platform->window_resize(name, w, h);
        return Value::none_val();
    });
    add_method(vm, window, "maximize", [&vm, platform, require_input](int argc, Value* args) -> Value {
        require_input();
        platform->window_maximize(arg_str(vm, argc, args, 0, "window.maximize"));
        return Value::none_val();
    });
    add_method(vm, window, "minimize", [&vm, platform, require_input](int argc, Value* args) -> Value {
        require_input();
        platform->window_minimize(arg_str(vm, argc, args, 0, "window.minimize"));
        return Value::none_val();
    });

    vm.define_global("window", Value::from_ptr(window));

    // ── app ──────────────────────────────────────────────────────────────────
    ObjMap* app = vm.alloc_map();

    add_method(vm, app, "run", [&vm, platform, require_input](int argc, Value* args) -> Value {
        require_input();
        platform->app_run(arg_str(vm, argc, args, 0, "app.run"));
        return Value::none_val();
    });
    add_method(vm, app, "open", [&vm, platform, require_input](int argc, Value* args) -> Value {
        require_input();
        platform->app_open(arg_str(vm, argc, args, 0, "app.open"));
        return Value::none_val();
    });
    add_method(vm, app, "close", [&vm, platform, require_input](int argc, Value* args) -> Value {
        require_input();
        platform->app_close(arg_str(vm, argc, args, 0, "app.close"));
        return Value::none_val();
    });

    vm.define_global("app", Value::from_ptr(app));

    // ── screen ───────────────────────────────────────────────────────────────
    ObjMap* screen = vm.alloc_map();

    add_method(vm, screen, "capture", [&vm, platform, require_screen](int argc, Value* args) -> Value {
        require_screen();
        const std::string& file = arg_str(vm, argc, args, 0, "screen.capture");
        int x1 = int(arg_int_or(argc, args, 1, 0));
        int y1 = int(arg_int_or(argc, args, 2, 0));
        int x2 = int(arg_int_or(argc, args, 3, 0));
        int y2 = int(arg_int_or(argc, args, 4, 0));
        platform->screen_capture(file, x1, y1, x2, y2);
        return Value::none_val();
    });

    add_method(vm, screen, "size", [&vm, platform](int, Value*) -> Value {
        auto [w, h] = platform->screen_size();
        ObjMap* m = vm.alloc_map();
        m->set(Value::from_ptr(vm.intern_string("width", 5)), Value::from_int(w));
        m->set(Value::from_ptr(vm.intern_string("height", 6)), Value::from_int(h));
        return Value::from_ptr(m);
    });

    vm.define_global("screen", Value::from_ptr(screen));

    // ── time ─────────────────────────────────────────────────────────────────
    ObjMap* time_ns = vm.alloc_map();

    add_method(vm, time_ns, "wait", [platform](int argc, Value* args) -> Value {
        if (argc < 1) return Value::none_val();
        uint64_t ns = args[0].is_duration() ? uint64_t(args[0].as_duration())
                    : args[0].is_int()      ? uint64_t(args[0].as_int()) * 1000000000ULL
                    : 0;
        platform->wait(ns);
        return Value::none_val();
    });

    vm.define_global("time", Value::from_ptr(time_ns));
}

} // namespace syn
