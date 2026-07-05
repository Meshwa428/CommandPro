#pragma once

// Native C-stack headroom guard. Deep *pure* Synapse recursion is bounded by
// the VM's MAX_FRAMES (heap frames, no C-stack growth), but recursion that
// re-enters the interpreter on the C++ stack (call_value → run_frame) or runs
// in JIT-compiled native code grows the real thread stack and would segfault.
// This guard lets those paths detect "close to the limit" and raise a clean
// E0102 instead of crashing (PLAN principle 7).

#include <cstddef>
#if defined(__linux__)
#include <pthread.h>
#endif

namespace syn {

// Lowest safe stack address (the stack grows down toward it). null until
// stack_guard_init() runs, in which case the checks conservatively pass.
inline char* g_stack_floor = nullptr;

// Record the current thread's usable stack bounds. Call once, early, on the
// thread that will run the VM / JIT'd code (they share one thread).
inline void stack_guard_init()
{
    const std::size_t margin = 256u * 1024;  // leave room for the diagnostic path
    std::size_t size = 8u * 1024 * 1024;     // fallback assumption
    char* low = nullptr;
#if defined(__linux__)
    pthread_attr_t attr;
    if (pthread_getattr_np(pthread_self(), &attr) == 0) {
        void* addr = nullptr; std::size_t sz = 0;
        if (pthread_attr_getstack(&attr, &addr, &sz) == 0) {
            low  = static_cast<char*>(addr);  // lowest address of the stack region
            size = sz;
        }
        pthread_attr_destroy(&attr);
    }
#endif
    if (low) {
        g_stack_floor = low + margin;
    } else {
        char probe;
        g_stack_floor = &probe - (size - margin);
    }
}

// True while there is still headroom to recurse further on the C stack.
inline bool stack_guard_ok()
{
    if (!g_stack_floor) return true;
    char probe;
    return &probe > g_stack_floor;
}

}  // namespace syn
