#pragma once
#include <dlfcn.h>
#include <string>
#include <unordered_map>
#include "synapse/runtime/value.h"
#include "synapse/frontend/ast.h"

namespace syn {

// Named int/float functions: return raw int64_t/double (caller boxes with Value::from_int/from_float)
using JitIntFn   = int64_t(*)(int, Value*);
using JitFloatFn = double (*)(int, Value*);
// __main__ JIT: prints directly, returns none
using JitMainFn  = Value  (*)(int, Value*);
// Generic value-typed JIT: operates on NaN-boxed Values, handles tuples/maps/
// dynamic lists/recursion via runtime helpers. Can be called with any args.
using JitValueFn = Value  (*)(int, Value*);

struct JitEntry {
    JitIntFn   int_fn   = nullptr;
    JitFloatFn float_fn = nullptr;
    JitMainFn  main_fn  = nullptr;  // non-null only for "__main__"
    JitValueFn value_fn = nullptr;  // generic Value-typed compiled function
};

struct JitModule {
    void* dl_handle = nullptr;
    std::unordered_map<std::string, JitEntry> fns;
    ~JitModule() { if (dl_handle) dlclose(dl_handle); }
};

// Compile JIT functions from top-level FnDeclStmt nodes.
// Returns null if nothing could be compiled.
JitModule* jit_compile(const Program& prog);

} // namespace syn
