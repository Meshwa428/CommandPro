#pragma once
#include <vector>
#include <cstdint>
#include "value.h"

namespace Synapse {

// Minimal wrapper for convenience in Compiler
struct VMFunctionHelper {
    static void write(ObjFunction* f, uint8_t byte, int line) {
        f->code.push_back(byte);
    }
    
    static void patch(ObjFunction* f, int offset, uint8_t byte) {
        f->code[offset] = byte;
    }

    static int addConstant(ObjFunction* f, SynapseValue value) {
        for (int i = 0; i < static_cast<int>(f->constants.size()); ++i) {
            if (valuesAreEqual(f->constants[i], value)) return i;
        }
        incref(value);
        f->constants.push_back(value);
        return static_cast<int>(f->constants.size() - 1);
    }
};

} // namespace Synapse
