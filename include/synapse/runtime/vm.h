#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include "synapse/runtime/value.h"
#include "synapse/runtime/opcodes.h"
#include "synapse/backend/jit.h"

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

    ObjList* alloc_list();
    ObjMap*  alloc_map();

    void collect_garbage();
    void mark_value(Value v);
    void mark_object(Obj* o);
    Value invoke_method(Value obj, MethodId mid, int nargs, Value* args);
    Value invoke_method_str(Value obj, const std::string& name, int nargs, Value* args);

private:
    Value run_frame(CallFrame& frame);
    Value call_value(Value callee, int nargs, Value* args);
    ObjUpvalue* capture_upvalue(Value* slot);
    void        close_upvalues(Value* last);

    static constexpr int REGISTER_STACK  = 256 * 64; // 64 frames × 256 regs
    static constexpr int MAX_FRAMES      = 64;

    Value      m_stack[REGISTER_STACK];
    CallFrame  m_frames[MAX_FRAMES];
    int        m_frame_count = 0;

    std::unordered_map<std::string, Value>    m_globals;
    ObjUpvalue* m_open_upvalues = nullptr;
    ObjString*  m_str_pool      = nullptr;
    ObjList*    m_list_pool     = nullptr;
    ObjMap*     m_map_pool      = nullptr;
    // public so main.cpp can populate before run()
public:
    std::unordered_map<std::string, JitEntry> m_jit;
private:
};

} // namespace syn
