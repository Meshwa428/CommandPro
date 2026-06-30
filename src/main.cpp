#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "synapse/frontend/lexer.h"
#include "synapse/frontend/parser.h"
#include "synapse/compiler/compiler.h"
#include "synapse/runtime/vm.h"
#include "synapse/runtime/value.h"
#include "synapse/backend/jit.h"

static void register_stdlib(syn::VM& vm)
{
    vm.define_native("print", [&vm](int argc, syn::Value* args) -> syn::Value {
        for (int i = 0; i < argc; ++i) {
            if (i) std::cout << ' ';
            std::cout << syn::val_to_string(args[i]);
        }
        std::cout << '\n';
        return syn::Value::none_val();
    });

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

    // JIT compile pure-int/float functions + top-level __main__ if possible
    static std::unique_ptr<syn::JitModule> jit_mod;
    if (!jit_mod) jit_mod.reset(syn::jit_compile(prog));
    if (jit_mod) {
        for (auto& [name, entry] : jit_mod->fns)
            vm.m_jit[name] = entry;

        // If __main__ is JIT'd, bypass interpreter entirely
        auto mit = vm.m_jit.find("__main__");
        if (mit != vm.m_jit.end() && mit->second.main_fn && !disasm_mode) {
            try { mit->second.main_fn(0, nullptr); } catch (const std::exception& ex) {
                std::cerr << "RuntimeError: " << ex.what() << '\n'; std::exit(1);
            }
            return syn::Value::none_val();
        }
    }

    syn::DiagEngine compile_diag;
    syn::ObjFunction* fn = syn::Compiler::compile(prog, source, compile_diag, vm);
    if (!fn || compile_diag.has_errors()) {
        compile_diag.print_all(source);
        std::exit(1);
    }

    if (disasm_mode) {
        fn->chunk->disasm(path);
        return syn::Value::none_val();
    }

    try {
        return vm.run(fn);
    } catch (const std::exception& ex) {
        std::cerr << "RuntimeError: " << ex.what() << '\n';
        std::exit(1);
    }
}

static void run_file(const std::string& path, bool disasm_mode = false)
{
    std::ifstream f(path);
    if (!f) {
        std::cerr << "syn: cannot open '" << path << "'\n";
        std::exit(1);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    run_source(path, ss.str(), disasm_mode);
}

static void repl()
{
    std::cout << "Synapse v0.2.0 — type 'exit' to quit\n";
    syn::VM vm;
    register_stdlib(vm);

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
        } catch (const std::exception& ex) {
            std::cerr << "RuntimeError: " << ex.what() << '\n';
        }
    }
}

int main(int argc, char* argv[])
{
    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        run_file(argv[1]);
    } else if (argc == 3 && std::string(argv[1]) == "--disasm") {
        run_file(argv[2], true);
    } else {
        std::cerr << "Usage: syn [--disasm] [script.syn]\n";
        return 1;
    }
    return 0;
}
