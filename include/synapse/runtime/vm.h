#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "synapse/runtime/value.h"
#include "synapse/runtime/opcodes.h"
#include "synapse/backend/jit.h"
#include "synapse/common/rt_error.h"

namespace syn {

// Set by VM::run so value.cpp's val_box_large_int can alloc ObjInt into the GC
extern thread_local VM* tls_vm;

struct Chunk;
struct ObjFunction;
struct ObjClosure;
struct ObjUpvalue;

// ── Call frame ────────────────────────────────────────────────────────────────
struct CallFrame {
    ObjClosure* closure = nullptr;
    Chunk*      chunk   = nullptr;
    uint64_t*   pc      = nullptr;
    Value*      regs    = nullptr;  // pointer into vm_stack
    int         base    = 0;       // register index of R[0] in this frame
};

// ── VM ────────────────────────────────────────────────────────────────────────
class VM {
public:
    VM();
    ~VM();

    // Register a native function as a global
    void define_native(const std::string& name, NativeFn fn);
    void define_global(const std::string& name, Value v);

    // Public entry point for natives (e.g. automation stdlib) to raise a
    // diagnostic-quality RuntimeError — same formatting as VM-internal
    // errors (see raise() below), best-effort line since natives run
    // outside run_frame's instruction loop.
    [[noreturn]] void throw_runtime_error(const char* code, std::string msg) { raise(code, std::move(msg)); }

    // Run a compiled top-level function
    // Returns the value of R[0] when HALT is hit, or none if program exits normally
    Value run(ObjFunction* fn);

    // GC root for heap-allocated objects
    Obj*     m_gc_list   = nullptr;
    size_t   m_alloc_count  = 0;
    size_t   m_gc_threshold = 1024;  // collect after this many allocations

    // GC runs only at the dispatch-loop top (instruction boundaries), where all
    // live values are in registers/frames. Allocators must NOT collect mid-op:
    // natives and multi-alloc opcodes hold freshly-made objects in C++ locals
    // that are not GC roots, so collecting there would free them.
    template<typename T, typename... Args>
    T* alloc(Args&&... args)
    {
        ++m_alloc_count;
        auto* obj = new T(std::forward<Args>(args)...);
        obj->gc_next = m_gc_list;
        m_gc_list = obj;
        return obj;
    }

    // String-specific allocators that reuse pooled ObjString objects
    ObjString* alloc_string(const char* data, size_t len);
    ObjString* alloc_string(const std::string& s) { return alloc_string(s.data(), s.size()); }
    ObjString* alloc_string_raw();  // get empty ObjString with capacity preserved

    // Intern table: one ObjString per unique content; pointer equality = content equality
    ObjString* intern_string(const char* data, size_t len);

    // Module loading support
    std::unordered_set<std::string> globals_snapshot() const;
    std::unordered_map<std::string, Value> new_globals_since(
        const std::unordered_set<std::string>& before) const;

    ObjList* alloc_list();
    ObjMap*  alloc_map();

    void collect_garbage();
    void mark_value(Value v);
    void mark_object(Obj* o);
    Value invoke_method(Value obj, MethodId mid, int nargs, Value* args);
    Value invoke_method_str(Value obj, const std::string& name, int nargs, Value* args);
    Value call_fn(Value callee, int nargs, Value* args);  // public bridge for JIT runtime helpers
    Value get_global(const std::string& name) const;      // global lookup for JIT helpers

private:
    Value run_frame(CallFrame& frame);
    Value call_value(Value callee, int nargs, Value* args);
    ObjUpvalue* capture_upvalue(Value* slot);
    void        close_upvalues(Value* last);

    // Zero-fills m_stack[m_stack_zeroed, base+256) on demand the first time a
    // frame's register window reaches that far — avoids eagerly zeroing all
    // REGISTER_STACK slots (2.56M Values / ~20MB) at construction, which cost
    // a real ~8ms/process in first-touch page faults regardless of how
    // shallow the actual recursion turns out to be. GC root-scanning still
    // never reads past a real frame's window, and only zeroed-or-written
    // slots are ever scanned, so this preserves the same safety invariant
    // incrementally instead of all at once.
    void ensure_stack_window(int base);

    // Pre-frame arg adjustment: pack *rest extras into a list, fill missing
    // params with none so the callee's default-value preamble can run.
    void fixup_args(ObjFunction* fn, int nargs, Value* regs);

    // Builds {"message": msg, "code": code} — the value a `catch e` binds
    // for a VM-raised (non-`throw`) error.
    Value make_error_payload(const std::string& code, const std::string& msg);

    // Throws a RuntimeError carrying a stable code, the source line the
    // currently-executing instruction came from, and a Synapse-level call
    // stack (see src/common/diag.cpp:print_runtime_error for how it's shown).
    [[noreturn]] void raise(const char* code, std::string msg, Chunk* ck, uint64_t* pc);
    // Best-effort overload for call sites with no local (ck, pc) — uses the
    // top call frame's last-saved pc, which is accurate except mid-run_frame.
    [[noreturn]] void raise(const char* code, std::string msg);
    // For `throw expr` — payload is the exact thrown value, not a wrapper.
    [[noreturn]] void raise_value(Value payload, Chunk* ck, uint64_t* pc);

    // A `return` inside a try body skips TRY_POP (no fallthrough to it), so
    // its handler would otherwise linger on m_try_handlers pointing at a
    // frame that no longer exists — a later, unrelated exception could then
    // wrongly resume at that dead catch dispatcher. Called right after
    // m_frame_count is decremented on every return.
    void drop_stale_try_handlers();

    // 1,000 frames (matches Python's default sys.getrecursionlimit()) — was
    // 64, which threw "stack overflow" on ordinary recursion depths (e.g. a
    // plain recursive sum to 100). Heap-allocated rather than fixed member
    // arrays: at larger sizes a fixed array would itself risk blowing the
    // *host* C++ thread's stack when VM is a local variable. Bigger than
    // 1000 is possible but costs real fixed startup time (~1.3ms at 128
    // frames vs ~2.5ms at 2000, measured) — a true dynamic-growth stack
    // would avoid that tradeoff entirely, but isn't safe with the current
    // design: call_value() recursively re-enters run_frame() on the C++
    // call stack for native/JIT calls, so growing (reallocating) m_stack
    // mid-recursion would leave already-suspended callers' local Value*
    // pointers dangling into the freed old buffer.
    static constexpr int REGISTER_STACK  = 256 * 1000;
    static constexpr int MAX_FRAMES      = 1000;

    Value*     m_stack;
    CallFrame* m_frames;
    int        m_frame_count = 0;
    int        m_stack_zeroed = 0;  // m_stack[0, m_stack_zeroed) is known-none/written

    // try/catch: one entry per currently-active `try` block, pushed by
    // TRY_PUSH and popped by TRY_POP on normal (non-exceptional) completion.
    // Global rather than per-frame because an exception thrown in a callee
    // must be catchable by a try/catch in any enclosing caller, not just the
    // frame it was thrown in — see run_frame()'s catch clause for how
    // frame_idx decides whether a given run_frame() invocation owns a
    // handler (its own frame or one pushed via the non-recursive CALL
    // opcode) versus must re-throw to an outer, already-suspended
    // invocation (reached only via call_value()'s C++ recursion).
    struct TryHandler {
        int        frame_idx;   // m_frame_count value when TRY_PUSH ran
        uint64_t*  catch_pc;    // resume point: start of the catch dispatcher
        Chunk*     catch_chunk;
        int        catch_reg;   // register to bind the caught value into
    };
    std::vector<TryHandler> m_try_handlers;

    std::unordered_map<std::string, Value>    m_globals;
    ObjUpvalue* m_open_upvalues = nullptr;
    ObjString*  m_str_pool      = nullptr;
    ObjList*    m_list_pool     = nullptr;
    ObjMap*     m_map_pool      = nullptr;
    ObjInt*     m_int_pool      = nullptr;
    ObjString** m_intern_table  = nullptr;
    uint32_t    m_intern_cap    = 0;
    uint32_t    m_intern_count  = 0;
public:
    // Pooled large-int allocation (values outside signed 48-bit range). Without
    // pooling, big-int arithmetic (e.g. RNG products) malloc/free per op.
    ObjInt* alloc_int(int64_t v);
private:
    // public so main.cpp can populate before run()
public:
    std::unordered_map<std::string, JitEntry> m_jit;
private:
};

} // namespace syn
