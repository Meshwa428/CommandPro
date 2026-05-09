#pragma once
#include "chunk.h"
#include "platform/platform.h"
#include <vector>
#include <unordered_map>

namespace Synapse {

enum class InterpretResult {
    OK,
    COMPILE_ERROR,
    RUNTIME_ERROR
};

struct CallFrame {
    ObjFunction* chunk;
    uint8_t*    ip;
    int         frameStart;
};

class VM {
public:
    VM(std::shared_ptr<IPlatform> platform = nullptr);
    ~VM();

    InterpretResult interpret(ObjFunction* function);
    void setGlobals(const std::vector<std::string>& names);

private:
    InterpretResult run();
    void registerBuiltins();
    void dumpProfile();

    static constexpr int MAX_FRAMES = 2048;
    static constexpr int MAX_STACK = 65536;
    
    CallFrame callStack[MAX_FRAMES];
    int frameCount = 0;

    std::vector<SynapseValue> stack;
    SynapseValue* stackTop;

    std::vector<SynapseValue> globals;
    std::vector<bool>         globalsDefined;
    std::unordered_map<std::string, int> globalNameMap;
    std::unordered_map<std::string, SynapseValue>  builtins;
    std::shared_ptr<IPlatform> platform;
    
    // Profiling
#ifdef SYNAPSE_PROFILER
    long long opcodeCounts[256];
    bool profilingEnabled = false;
    
    // Telemetry
    long long totalAllocations = 0;
    int maxStackDepth = 0;
    int currentStackDepth = 0;
#endif

    inline void pushV(SynapseValue value) {
        incref(value);
        *stackTop++ = value;
#ifdef SYNAPSE_PROFILER
        if (profilingEnabled) {
            int depth = (int)(stackTop - stack.data());
            if (depth > maxStackDepth) maxStackDepth = depth;
        }
#endif
    }

    inline SynapseValue popV() {
        SynapseValue v = *(--stackTop);
        // We don't decref here, caller must handle it or repush it.
        // Actually, to prevent leaks, we often move it.
        return v;
    }
    
    inline SynapseValue peekV(int distance = 0) {
        return stackTop[-1 - distance];
    }
};

} // namespace Synapse
