#include <climits>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

#include "synapse/common/stack_guard.h"
#include "synapse/rat/rat_model.h"
#include "synapse/frontend/ast.h"
#include "synapse/frontend/lexer.h"
#include "synapse/frontend/parser.h"
#include "synapse/compiler/compiler.h"
#include "synapse/runtime/vm.h"
#include "synapse/runtime/value.h"
#include "synapse/backend/jit.h"
#include "synapse/common/diag.h"
#include "synapse/runtime/automation_stdlib.h"
#include "synapse/platform/platform.h"
#include "platform/mock/mock_platform.h"

extern "C" void syn_rt_drain_jit_pool();

static void register_stdlib(syn::VM& vm)
{
    vm.define_native("say", [](int argc, syn::Value* args) -> syn::Value {
        for (int i = 0; i < argc; ++i) {
            if (i) std::cout << ' ';
            std::cout << syn::val_to_string(args[i]);
        }
        std::cout << '\n';
        return syn::Value::none_val();
    });

    vm.define_native("len", [](int argc, syn::Value* args) -> syn::Value {
        if (argc < 1) return syn::Value::from_int(0);
        syn::Value v = args[0];
        if (syn::val_is_string(v)) return syn::Value::from_int(int64_t(syn::as_str(v).data.size()));
        if (syn::val_is_list(v))   return syn::Value::from_int(int64_t(syn::as_list(v).items.size()));
        return syn::Value::from_int(0);
    });

    vm.define_native("int", [](int argc, syn::Value* args) -> syn::Value {
        if (argc < 1) return syn::Value::from_int(0);
        syn::Value v = args[0];
        if (v.is_int()) return v;
        if (v.is_float()) return syn::Value::from_int(int64_t(v.as_float()));
        if (syn::val_is_string(v)) {
            try { return syn::Value::from_int(std::stoll(syn::as_str(v).data)); }
            catch (...) {}
        }
        return syn::Value::from_int(0);
    });

    vm.define_native("float", [](int argc, syn::Value* args) -> syn::Value {
        if (argc < 1) return syn::Value::from_float(0.0);
        syn::Value v = args[0];
        if (v.is_float()) return v;
        if (v.is_int()) return syn::Value::from_float(double(v.as_int()));
        return syn::Value::from_float(0.0);
    });

    vm.define_native("str", [&vm](int argc, syn::Value* args) -> syn::Value {
        if (argc < 1) return syn::Value::from_ptr(vm.alloc_string("", 0));
        return syn::Value::from_ptr(vm.alloc_string(syn::val_to_string(args[0])));
    });

    vm.define_native("append", [](int argc, syn::Value* args) -> syn::Value {
        if (argc < 2 || !syn::val_is_list(args[0])) return syn::Value::none_val();
        syn::as_list(args[0]).items.push_back(args[1]);
        return syn::Value::none_val();
    });

    vm.define_native("range", [&vm](int argc, syn::Value* args) -> syn::Value {
        // range(stop) or range(start, stop) or range(start, stop, step)
        int64_t start = 0, stop = 0, step = 1;
        if (argc == 1) { stop = args[0].is_int() ? args[0].as_int() : 0; }
        else if (argc >= 2) {
            start = args[0].is_int() ? args[0].as_int() : 0;
            stop  = args[1].is_int() ? args[1].as_int() : 0;
            if (argc >= 3) step = args[2].is_int() ? args[2].as_int() : 1;
        }
        auto* list = vm.alloc_list();
        for (int64_t i = start; step > 0 ? i < stop : i > stop; i += step)
            list->items.push_back(syn::Value::from_int(i));
        return syn::Value::from_ptr(list);
    });

    // xs[a to b] compiles to __slice(xs, a, b). End-exclusive, negative indices
    // count from the back, none bounds default to 0 / len.
    vm.define_native("__slice", [&vm](int argc, syn::Value* args) -> syn::Value {
        if (argc < 3) return syn::Value::none_val();
        syn::Value obj = args[0];
        bool is_seq = obj.is_ptr() && (obj.as_ptr()->kind == syn::ObjKind::List ||
                                       obj.as_ptr()->kind == syn::ObjKind::Tuple);
        int64_t n;
        if (syn::val_is_string(obj)) n = int64_t(syn::as_str(obj).data.size());
        else if (is_seq)             n = int64_t(static_cast<syn::ObjList*>(obj.as_ptr())->items.size());
        else return syn::Value::none_val();

        auto bound = [n](syn::Value v, int64_t dflt) {
            if (!v.is_int()) return dflt;
            int64_t i = v.as_int();
            if (i < 0) i += n;
            if (i < 0) i = 0;
            if (i > n) i = n;
            return i;
        };
        int64_t lo = bound(args[1], 0), hi = bound(args[2], n);
        if (hi < lo) hi = lo;

        if (syn::val_is_string(obj)) {
            const std::string& s = syn::as_str(obj).data;
            return syn::Value::from_ptr(vm.alloc_string(s.data() + lo, std::size_t(hi - lo)));
        }
        auto& items = static_cast<syn::ObjList*>(obj.as_ptr())->items;
        auto* out = vm.alloc_list();
        out->kind = obj.as_ptr()->kind;  // tuple slice stays a tuple
        out->items.assign(items.begin() + lo, items.begin() + hi);
        return syn::Value::from_ptr(out);
    });

    vm.define_native("abs", [](int argc, syn::Value* args) -> syn::Value {
        if (argc < 1) return syn::Value::from_int(0);
        if (args[0].is_int()) return syn::Value::from_int(args[0].as_int() < 0 ? -args[0].as_int() : args[0].as_int());
        return syn::Value::from_float(std::abs(args[0].as_float()));
    });

    // ── String operations ──────────────────────────────────────────────────────

    vm.define_native("split", [&vm](int argc, syn::Value* args) -> syn::Value {
        if (argc < 1 || !syn::val_is_string(args[0])) return syn::Value::from_ptr(vm.alloc_list());
        const std::string& s = syn::as_str(args[0]).data;
        const std::string& sep = (argc >= 2 && syn::val_is_string(args[1]))
                                 ? syn::as_str(args[1]).data
                                 : ([] -> const std::string& { static const std::string sp=" "; return sp; })();
        auto* list = vm.alloc_list();
        if (sep.empty()) {
            for (char c : s) {
                list->items.push_back(syn::Value::from_ptr(vm.alloc_string(&c, 1)));
            }
            return syn::Value::from_ptr(list);
        }
        size_t pos = 0, found;
        while ((found = s.find(sep, pos)) != std::string::npos) {
            list->items.push_back(syn::Value::from_ptr(vm.alloc_string(s.data() + pos, found - pos)));
            pos = found + sep.size();
        }
        list->items.push_back(syn::Value::from_ptr(vm.alloc_string(s.data() + pos, s.size() - pos)));
        return syn::Value::from_ptr(list);
    });

    vm.define_native("join", [&vm](int argc, syn::Value* args) -> syn::Value {
        if (argc < 2 || !syn::val_is_string(args[0]) || !syn::val_is_list(args[1]))
            return syn::Value::from_ptr(vm.alloc_string("", 0));
        const std::string& sep = syn::as_str(args[0]).data;
        const auto& items = syn::as_list(args[1]).items;
        auto* obj = vm.alloc_string_raw();
        for (size_t i = 0; i < items.size(); ++i) {
            if (i) obj->data += sep;
            if (syn::val_is_string(items[i])) obj->data += syn::as_str(items[i]).data;
            else obj->data += syn::val_to_string(items[i]);
        }
        return syn::Value::from_ptr(obj);
    });

    vm.define_native("find", [](int argc, syn::Value* args) -> syn::Value {
        if (argc < 2 || !syn::val_is_string(args[0]) || !syn::val_is_string(args[1]))
            return syn::Value::from_int(-1);
        const std::string& s   = syn::as_str(args[0]).data;
        const std::string& sub = syn::as_str(args[1]).data;
        int64_t start = (argc >= 3 && args[2].is_int()) ? args[2].as_int() : 0;
        auto pos = s.find(sub, size_t(start));
        return syn::Value::from_int(pos == std::string::npos ? -1 : int64_t(pos));
    });

    vm.define_native("replace", [&vm](int argc, syn::Value* args) -> syn::Value {
        if (argc < 3 || !syn::val_is_string(args[0]) || !syn::val_is_string(args[1]) || !syn::val_is_string(args[2]))
            return argc >= 1 ? args[0] : syn::Value::from_ptr(vm.alloc_string("", 0));
        auto* obj = vm.alloc_string_raw();
        obj->data = syn::as_str(args[0]).data;
        const std::string& old_s = syn::as_str(args[1]).data;
        const std::string& new_s = syn::as_str(args[2]).data;
        if (!old_s.empty()) {
            size_t pos = 0;
            while ((pos = obj->data.find(old_s, pos)) != std::string::npos) {
                obj->data.replace(pos, old_s.size(), new_s);
                pos += new_s.size();
            }
        }
        return syn::Value::from_ptr(obj);
    });

    vm.define_native("upper", [&vm](int argc, syn::Value* args) -> syn::Value {
        if (argc < 1 || !syn::val_is_string(args[0])) return syn::Value::from_ptr(vm.alloc_string("", 0));
        auto* obj = vm.alloc_string_raw();
        obj->data = syn::as_str(args[0]).data;
        for (char& c : obj->data) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return syn::Value::from_ptr(obj);
    });

    vm.define_native("lower", [&vm](int argc, syn::Value* args) -> syn::Value {
        if (argc < 1 || !syn::val_is_string(args[0])) return syn::Value::from_ptr(vm.alloc_string("", 0));
        auto* obj = vm.alloc_string_raw();
        obj->data = syn::as_str(args[0]).data;
        for (char& c : obj->data) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return syn::Value::from_ptr(obj);
    });

    vm.define_native("trim", [&vm](int argc, syn::Value* args) -> syn::Value {
        if (argc < 1 || !syn::val_is_string(args[0])) return syn::Value::from_ptr(vm.alloc_string("", 0));
        const std::string& s = syn::as_str(args[0]).data;
        size_t lo = s.find_first_not_of(" \t\r\n");
        if (lo == std::string::npos) return syn::Value::from_ptr(vm.alloc_string("", 0));
        size_t hi = s.find_last_not_of(" \t\r\n");
        return syn::Value::from_ptr(vm.alloc_string(s.data() + lo, hi - lo + 1));
    });

    vm.define_native("starts_with", [](int argc, syn::Value* args) -> syn::Value {
        if (argc < 2 || !syn::val_is_string(args[0]) || !syn::val_is_string(args[1]))
            return syn::Value::from_bool(false);
        const std::string& s = syn::as_str(args[0]).data;
        const std::string& p = syn::as_str(args[1]).data;
        return syn::Value::from_bool(s.size() >= p.size() && s.compare(0, p.size(), p) == 0);
    });

    vm.define_native("ends_with", [](int argc, syn::Value* args) -> syn::Value {
        if (argc < 2 || !syn::val_is_string(args[0]) || !syn::val_is_string(args[1]))
            return syn::Value::from_bool(false);
        const std::string& s = syn::as_str(args[0]).data;
        const std::string& p = syn::as_str(args[1]).data;
        return syn::Value::from_bool(s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0);
    });

    vm.define_native("substr", [&vm](int argc, syn::Value* args) -> syn::Value {
        if (argc < 1 || !syn::val_is_string(args[0])) return syn::Value::from_ptr(vm.alloc_string("", 0));
        const std::string& s = syn::as_str(args[0]).data;
        int64_t len = int64_t(s.size());
        int64_t lo = (argc >= 2 && args[1].is_int()) ? args[1].as_int() : 0;
        int64_t hi = (argc >= 3 && args[2].is_int()) ? args[2].as_int() : len;
        if (lo < 0) lo += len;
        if (hi < 0) hi += len;
        lo = std::max(int64_t(0), std::min(lo, len));
        hi = std::max(int64_t(0), std::min(hi, len));
        if (lo >= hi) return syn::Value::from_ptr(vm.alloc_string("", 0));
        return syn::Value::from_ptr(vm.alloc_string(s.data() + size_t(lo), size_t(hi - lo)));
    });

    vm.define_native("ord", [](int argc, syn::Value* args) -> syn::Value {
        if (argc < 1 || !syn::val_is_string(args[0])) return syn::Value::from_int(0);
        const std::string& s = syn::as_str(args[0]).data;
        return syn::Value::from_int(s.empty() ? 0 : int64_t(static_cast<unsigned char>(s[0])));
    });

    vm.define_native("chr", [&vm](int argc, syn::Value* args) -> syn::Value {
        if (argc < 1 || !args[0].is_int()) return syn::Value::from_ptr(vm.alloc_string("", 0));
        char c = static_cast<char>(args[0].as_int() & 0xFF);
        return syn::Value::from_ptr(vm.alloc_string(&c, 1));
    });

    // math
    auto math1f = [](auto fn) {
        return [fn](int argc, syn::Value* args) -> syn::Value {
            double x = args[0].is_int() ? double(args[0].as_int()) : args[0].as_float();
            return syn::Value::from_float(fn(x));
        };
    };
    vm.define_native("sqrt",  math1f([](double x){ return std::sqrt(x); }));
    vm.define_native("floor", math1f([](double x){ return std::floor(x); }));
    vm.define_native("ceil",  math1f([](double x){ return std::ceil(x); }));
    vm.define_native("sin",   math1f([](double x){ return std::sin(x); }));
    vm.define_native("cos",   math1f([](double x){ return std::cos(x); }));
    vm.define_native("exp",   math1f([](double x){ return std::exp(x); }));
    vm.define_native("log",   math1f([](double x){ return std::log(x); }));
    vm.define_native("pow", [](int argc, syn::Value* args) -> syn::Value {
        double b = args[0].is_int() ? double(args[0].as_int()) : args[0].as_float();
        double e = args[1].is_int() ? double(args[1].as_int()) : args[1].as_float();
        return syn::Value::from_float(std::pow(b, e));
    });
    vm.define_native("min", [](int argc, syn::Value* args) -> syn::Value {
        if (argc < 2) return args[0];
        if (args[0].is_int() && args[1].is_int())
            return syn::Value::from_int(std::min(args[0].as_int(), args[1].as_int()));
        double a = args[0].is_int() ? double(args[0].as_int()) : args[0].as_float();
        double b = args[1].is_int() ? double(args[1].as_int()) : args[1].as_float();
        return syn::Value::from_float(std::min(a, b));
    });
    vm.define_native("max", [](int argc, syn::Value* args) -> syn::Value {
        if (argc < 2) return args[0];
        if (args[0].is_int() && args[1].is_int())
            return syn::Value::from_int(std::max(args[0].as_int(), args[1].as_int()));
        double a = args[0].is_int() ? double(args[0].as_int()) : args[0].as_float();
        double b = args[1].is_int() ? double(args[1].as_int()) : args[1].as_float();
        return syn::Value::from_float(std::max(a, b));
    });
}

// Directory containing the running `syn` binary — the sibling
// syn_linux_platform.so, if built, lives right next to it.
static std::string exe_dir()
{
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return ".";
    std::string path(buf, n);
    auto pos = path.find_last_of("\\/");
    return pos == std::string::npos ? "." : path.substr(0, pos);
#else
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return ".";
    buf[n] = '\0';
    std::string path(buf);
    auto pos = path.find_last_of('/');
    return pos == std::string::npos ? "." : path.substr(0, pos);
#endif
}

// Picks the automation backend: MockPlatform under tests/run.py
// (SYN_MOCK_PLATFORM=1) or when this build has no real backend, otherwise
// the real Linux (X11/XTest) backend — dlopen'd on demand from the sibling
// syn_linux_platform.so plugin (see CMakeLists.txt for why it's a separate
// plugin rather than linked directly: avoids paying X11/PNG's shared-library
// load cost on every process that never touches automation).
static syn::Platform* make_platform()
{
    if (!std::getenv("SYN_MOCK_PLATFORM")) {
        using FactoryFn = syn::Platform* (*)();
#ifdef _WIN32
        std::string dll_path = exe_dir() + "\\syn_windows_platform.dll";
        if (HMODULE handle = LoadLibraryA(dll_path.c_str())) {
            if (auto* factory = reinterpret_cast<FactoryFn>(
                    GetProcAddress(handle, "syn_create_windows_platform")))
                return factory();
        }
#else
        std::string so_path = exe_dir() + "/syn_linux_platform.so";
        if (void* handle = dlopen(so_path.c_str(), RTLD_NOW | RTLD_LOCAL)) {
            if (auto* factory = reinterpret_cast<FactoryFn>(dlsym(handle, "syn_create_linux_platform")))
                return factory();
        }
#endif
    }
    static syn::MockPlatform platform;
    return &platform;
}

// tests/run.py (automation goldens) sets SYN_DUMP_PLATFORM_LOG=1 to read back
// what the mock backend recorded, one "PLATFORM: <call>" line per command.
static void maybe_dump_platform_log(syn::Platform* platform)
{
    if (!std::getenv("SYN_DUMP_PLATFORM_LOG")) return;
    if (auto* mock = dynamic_cast<syn::MockPlatform*>(platform))
        for (auto& line : mock->log()) std::cout << "PLATFORM: " << line << "\n";
}

// Registering the automation stdlib allocates ~35 ObjNative closures across
// 6 namespace maps, and (on Linux) the first real Platform call opens an X11
// connection — real cost that showed up as a flat ~1ms/process tax on every
// benchmark, including pure-compute scripts that never touch automation.
// Skip it unless the script can actually reach mouse/keyboard/window/app/
// screen/time, via either command-statement sugar (dedicated keyword tokens)
// or a direct `mouse.move(...)`-style call (plain identifier).
static bool script_uses_automation(const std::vector<syn::Token>& tokens, const syn::Source& source)
{
    using syn::TokenKind;
    static const std::unordered_set<std::string> kNamespaceNames = {
        "mouse", "keyboard", "window", "app", "screen", "time"
    };
    for (const auto& tok : tokens) {
        switch (tok.kind) {
        case TokenKind::Mouse: case TokenKind::Click: case TokenKind::Drag:
        case TokenKind::Scroll: case TokenKind::Hold: case TokenKind::Release:
        case TokenKind::Press: case TokenKind::Type: case TokenKind::Run:
        case TokenKind::Open: case TokenKind::Close: case TokenKind::Focus:
        case TokenKind::Move: case TokenKind::Resize: case TokenKind::Maximize:
        case TokenKind::Minimize: case TokenKind::Capture: case TokenKind::Wait:
        case TokenKind::Find: case TokenKind::See: case TokenKind::Tap:
        case TokenKind::Check: case TokenKind::Uncheck: case TokenKind::Select:
        case TokenKind::Read:
            return true;
        case TokenKind::Ident:
            if (kNamespaceNames.count(std::string(tok.text(source)))) return true;
            break;
        default: break;
        }
    }
    return false;
}

// ── Module loading ────────────────────────────────────────────────────────────

static std::string dir_of(const std::string& path)
{
    auto pos = path.find_last_of("/\\");
    return pos == std::string::npos ? "" : path.substr(0, pos);
}

static std::string path_join(const std::string& dir, const std::string& name)
{
    return dir.empty() ? name : dir + "/" + name;
}

static std::string abs_path(const std::string& p)
{
    char buf[4096];
#ifdef _WIN32
    return _fullpath(buf, p.c_str(), sizeof(buf)) ? std::string(buf) : p;
#else
    return realpath(p.c_str(), buf) ? std::string(buf) : p;
#endif
}

static std::string read_file_str(const std::string& path)
{
    std::ifstream f(path);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Resolve a UseEntry to an absolute file path; returns "" if not found.
//
// Directory packages: if `<rel>.syn` doesn't exist but `<rel>/` is a
// directory, fall back to `<rel>/__init__.syn` (Python's `__init__.py`
// convention). That file runs like any other module — its own `use`
// statements resolve relative to its own directory, so it typically
// `use`s its sibling files and re-exports whatever globals it needs;
// there's no separate export-list syntax, a module's globals *are* its
// public surface (see load_modules' aliased-import diffing).
static std::string resolve_module(const syn::UseEntry& entry, const std::string& base_dir)
{
    auto try_path = [](const std::string& p) -> std::string {
        std::ifstream f(p);
        return f ? p : "";
    };
    auto try_path_or_package = [&](const std::string& dir, const std::string& rel) -> std::string {
        if (auto r = try_path(path_join(dir, rel + ".syn")); !r.empty()) return r;
        return try_path(path_join(dir, rel + "/__init__.syn"));
    };
    // Dot in the final path component only — a bare "." or ".." traversal
    // segment earlier in a string-ref path (e.g. "../../fixtures/pkg") isn't
    // a file extension and shouldn't block the directory-package fallback.
    auto basename_has_dot = [](const std::string& p) {
        auto slash = p.find_last_of('/');
        std::string base = slash == std::string::npos ? p : p.substr(slash + 1);
        return base.find('.') != std::string::npos;
    };

    if (entry.is_str_ref) {
        // Strip surrounding quotes from raw token text
        std::string name = entry.module_str;
        if (name.size() >= 2 && name.front() == '"') name = name.substr(1, name.size() - 2);
        if (basename_has_dot(name)) return try_path(path_join(base_dir, name));
        return try_path_or_package(base_dir, name);
    } else {
        // Dotted ident: utils.io → utils/io.syn (or utils/io/__init__.syn)
        std::string rel;
        for (size_t i = 0; i < entry.module_path.size(); ++i) {
            if (i) rel += "/";
            rel += entry.module_path[i];
        }
        // 1. Relative to current file's dir
        if (auto r = try_path_or_package(base_dir, rel); !r.empty()) return r;
        // 2. std/ subdir next to current file
        if (auto r = try_path_or_package(base_dir + "/std", rel); !r.empty()) return r;
        return "";
    }
}

// Forward declaration
static void load_modules(const syn::Program& prog, const std::string& base_dir,
                         syn::VM& vm, std::unordered_set<std::string>& loaded);
static void run_in_vm(const std::string& path, const std::string& src,
                      syn::VM& vm, std::unordered_set<std::string>& loaded);

// Execute all UseStmt entries in prog, recursively loading dependencies.
static void load_modules(const syn::Program& prog, const std::string& base_dir,
                         syn::VM& vm, std::unordered_set<std::string>& loaded)
{
    for (auto& stmt : prog.stmts) {
        auto* use = dynamic_cast<const syn::UseStmt*>(stmt.get());
        if (!use) continue;
        for (auto& entry : use->entries) {
            std::string mod_path = resolve_module(entry, base_dir);
            if (mod_path.empty()) {
                std::string name = entry.is_str_ref
                    ? entry.module_str
                    : (entry.module_path.empty() ? "?" : entry.module_path[0]);
                std::cerr << "syn: module '" << name << "' not found\n";
                continue;
            }
            std::string key = abs_path(mod_path);
            if (loaded.count(key)) continue;  // already loaded or circular

            std::string src = read_file_str(mod_path);
            if (src.empty()) {
                std::cerr << "syn: cannot read '" << mod_path << "'\n";
                continue;
            }

            // Snapshot before for aliased import
            auto before = entry.alias.empty() ? std::unordered_set<std::string>{}
                                              : vm.globals_snapshot();
            loaded.insert(key);
            run_in_vm(mod_path, src, vm, loaded);

            // Aliased: collect new globals into a map, bind as alias
            if (!entry.alias.empty()) {
                auto ng = vm.new_globals_since(before);
                syn::ObjMap* ns = vm.alloc_map();
                for (auto& [k, v] : ng)
                    ns->set(syn::Value::from_ptr(vm.intern_string(k.data(), k.size())), v);
                vm.define_global(entry.alias, syn::Value::from_ptr(ns));
            }
        }
    }
}

// Parse, load dependencies, compile and interpret a single file into vm.
static void run_in_vm(const std::string& path, const std::string& src,
                      syn::VM& vm, std::unordered_set<std::string>& loaded)
{
    syn::Source    source(path, src);
    syn::Lexer     lexer(source);
    auto           tokens = lexer.tokenize();
    if (lexer.has_errors()) { lexer.diag().print_all(source); std::exit(1); }

    syn::Parser  parser(tokens, source);
    syn::Program prog = parser.parse();
    if (parser.has_errors()) { parser.diag().print_all(source); std::exit(1); }

    load_modules(prog, dir_of(path), vm, loaded);

    syn::DiagEngine compile_diag;
    // module_mode=true: top-level fn/let/const also emit SET_GLOBAL
    syn::ObjFunction* fn = syn::Compiler::compile(prog, source, compile_diag, vm, true);
    if (!fn || compile_diag.has_errors()) { compile_diag.print_all(source); std::exit(1); }

    try { vm.run(fn); }
    catch (const syn::RuntimeError& err) {
        syn::print_runtime_error(err, source);
        std::exit(1);
    }
    catch (const std::exception& ex) {
        std::cerr << "RuntimeError: " << ex.what() << '\n';
        std::exit(1);
    }
}

// ── Main entry point ──────────────────────────────────────────────────────────

static syn::Value run_source(const std::string& path, const std::string& src,
                              bool disasm_mode = false)
{
    syn::Source    source(path, src);
    syn::Lexer     lexer(source);
    auto           tokens = lexer.tokenize();

    if (lexer.has_errors()) {
        lexer.diag().print_all(source);
        std::exit(1);
    }

    syn::Parser  parser(tokens, source);
    syn::Program prog = parser.parse();
    if (parser.has_errors()) {
        parser.diag().print_all(source);
        std::exit(1);
    }

    syn::VM vm;
    register_stdlib(vm);
    syn::Platform* platform = nullptr;
    if (script_uses_automation(tokens, source)) {
        platform = make_platform();
        syn::register_automation_stdlib(vm, platform, syn::AutomationOptions{});
    }

    // Load modules before compiling main file
    std::unordered_set<std::string> loaded;
    if (!path.empty() && path != "<repl>") loaded.insert(abs_path(path));
    load_modules(prog, dir_of(path), vm, loaded);

    // Bytecode compile (UseStmts are silently skipped by compiler)
    syn::DiagEngine compile_diag;
    syn::ObjFunction* fn = syn::Compiler::compile(prog, source, compile_diag, vm);
    if (!fn || compile_diag.has_errors()) {
        compile_diag.print_all(source);
        std::exit(1);
    }

    // JIT compile pure-int/float functions + top-level __main__ if possible
    static const bool no_jit = std::getenv("SYN_NO_JIT") != nullptr;
    static std::unique_ptr<syn::JitModule> jit_mod;
    if (!no_jit && !jit_mod) jit_mod.reset(syn::jit_compile(prog));
    if (jit_mod) {
        for (auto& [name, entry] : jit_mod->fns)
            vm.m_jit[name] = entry;

        auto mit = vm.m_jit.find("__main__");
        if (mit != vm.m_jit.end() && mit->second.main_fn && !disasm_mode) {
            {
                using syn::ObjKind; using syn::ObjFunction;
                for (auto& cv : fn->chunk->constants) {
                    if (!cv.is_ptr()) continue;
                    if (cv.as_ptr()->kind != ObjKind::Function) continue;
                    ObjFunction* pfn = static_cast<ObjFunction*>(cv.as_ptr());
                    if (pfn->upvalue_count != 0 || pfn->name.empty()) continue;
                    syn::ObjClosure* cl = vm.alloc<syn::ObjClosure>(pfn);
                    vm.define_global(pfn->name, syn::Value::from_ptr(cl));
                }
            }
            syn::tls_vm = &vm;
            try { mit->second.main_fn(0, nullptr); }
            catch (const syn::RuntimeError& err) {
                syn::tls_vm = nullptr;
                syn::print_runtime_error(err, source);
                std::exit(1);
            }
            catch (const std::exception& ex) {
                syn::tls_vm = nullptr;
                std::cerr << "RuntimeError: " << ex.what() << '\n'; std::exit(1);
            }
            syn_rt_drain_jit_pool();
            syn::tls_vm = nullptr;
            maybe_dump_platform_log(platform);
            return syn::Value::none_val();
        }
    }

    if (disasm_mode) {
        fn->chunk->disasm(path);
        return syn::Value::none_val();
    }

    try {
        syn::Value result = vm.run(fn);
        maybe_dump_platform_log(platform);
        return result;
    } catch (const syn::RuntimeError& err) {
        syn::print_runtime_error(err, source);
        std::exit(1);
    } catch (const std::exception& ex) {
        std::cerr << "RuntimeError: " << ex.what() << '\n';
        std::exit(1);
    }
}

static void run_file(const std::string& path, bool disasm_mode = false)
{
    std::string src = read_file_str(path);
    if (src.empty() && !std::ifstream(path)) {
        std::cerr << "syn: cannot open '" << path << "'\n";
        std::exit(1);
    }
    run_source(path, src, disasm_mode);
}

static void repl()
{
    std::cout << "Synapse v0.2.0 — type 'exit' to quit\n";
    syn::VM vm;
    register_stdlib(vm);
    syn::register_automation_stdlib(vm, make_platform(), syn::AutomationOptions{});

    std::string line;
    while (true) {
        std::cout << ">>> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "exit" || line == "quit") break;
        if (line.empty()) continue;

        syn::Source    source("<repl>", line);
        syn::Lexer     lexer(source);
        auto           tokens = lexer.tokenize();
        if (lexer.has_errors()) { lexer.diag().print_all(source); continue; }

        syn::Parser  parser(tokens, source);
        syn::Program prog = parser.parse();
        if (parser.has_errors()) { parser.diag().print_all(source); continue; }

        syn::DiagEngine compile_diag;
        syn::ObjFunction* fn = syn::Compiler::compile(prog, source, compile_diag, vm);
        if (!fn || compile_diag.has_errors()) { compile_diag.print_all(source); continue; }

        try {
            syn::Value res = vm.run(fn);
            if (!res.is_none())
                std::cout << syn::val_to_string(res) << '\n';
        } catch (const syn::RuntimeError& err) {
            syn::print_runtime_error(err, source);
        } catch (const std::exception& ex) {
            std::cerr << "RuntimeError: " << ex.what() << '\n';
        }
    }
}

// `syn rat <subcommand>` — mouse-model calibration (design 005 §6).
static int rat_command(int argc, char* argv[])
{
    std::string sub = argc >= 3 ? argv[2] : "status";

    if (sub == "status") {
        const syn::RatProfile& p = syn::RatModel::active_profile();
        std::string path = syn::rat_user_profile_path();
        std::cout << "RAT mouse model\n";
        std::cout << "  profile: " << (syn::RatModel::active_is_user() ? "user calibration" : "baked defaults") << "\n";
        std::cout << "  file:    " << (path.empty() ? "($HOME unset)" : path) << "\n";
        std::cout << "  fitts_a         " << p.fitts_a << " ms\n";
        std::cout << "  fitts_b         " << p.fitts_b << " ms/bit\n";
        std::cout << "  curvature_scale " << p.curvature_scale << "\n";
        std::cout << "  tremor_sigma    " << p.tremor_sigma << "\n";
        std::cout << "  overshoot_rate  " << p.overshoot_rate << "\n";
        return 0;
    }
    if (sub == "reset") {
        std::string path = syn::rat_user_profile_path();
        if (path.empty()) { std::cerr << "rat reset: $HOME unset\n"; return 1; }
        if (std::remove(path.c_str()) == 0) {
            std::cout << "Deleted " << path << " — back to baked defaults.\n";
        } else {
            std::cout << "No user calibration to reset (" << path << " absent).\n";
        }
        return 0;
    }
    if (sub == "calibrate") {
        bool quick = (argc >= 4 && std::string(argv[3]) == "--quick");
        int movements = quick ? 30 : 100;
        std::string path = syn::rat_user_profile_path();
        if (path.empty()) { std::cerr << "rat calibrate: $HOME unset\n"; return 1; }

        syn::Platform* platform = make_platform();
        std::cout << "Click each dot as it appears (" << movements
                  << " movements). Esc to cancel.\n";
        std::vector<syn::CalibrationSample> samples;
        if (!platform->calibrate_rat(movements, samples) || samples.empty()) {
            std::cerr << "Calibration cancelled or unavailable (needs an X11 display).\n";
            return 1;
        }

        syn::RatProfile prof = syn::estimate_profile(samples);

        // Ensure ~/.config/synapse exists before writing.
        std::string dir = path.substr(0, path.find_last_of('/'));
        std::string mkdir_cmd = "mkdir -p '" + dir + "'";
        (void)std::system(mkdir_cmd.c_str());

        if (!prof.save(path.c_str())) {
            std::cerr << "Calibration measured but failed to write " << path << "\n";
            return 1;
        }
        std::cout << "Calibrated from " << samples.size() << " movements → " << path << "\n"
                  << "  fitts_a " << prof.fitts_a << "  fitts_b " << prof.fitts_b
                  << "  overshoot " << prof.overshoot_rate << "\n"
                  << "Run 'syn rat status' to review.\n";
        return 0;
    }
    std::cerr << "Usage: syn rat [status|reset|calibrate [--quick]]\n";
    return 1;
}

int main(int argc, char* argv[])
{
    syn::stack_guard_init();  // record this thread's C-stack bounds (VM + JIT)
    if (argc >= 2 && std::string(argv[1]) == "rat") {
        return rat_command(argc, argv);
    }
    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        run_file(argv[1]);
    } else if (argc == 3 && std::string(argv[1]) == "--disasm") {
        run_file(argv[2], true);
    } else {
        std::cerr << "Usage: syn [--disasm] [script.syn]\n       syn rat [status|reset|calibrate]\n";
        return 1;
    }
    return 0;
}
