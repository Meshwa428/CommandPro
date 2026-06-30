#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "synapse/runtime/value.h"

namespace syn {

// ── Chunk — one compiled function's bytecode ───────────────────────────────────
struct Chunk {
    std::string           name;
    std::vector<uint64_t> code;         // 64-bit instructions
    std::vector<Value>    constants;    // constant pool (strings, floats, protos)
    std::vector<uint32_t> lines;        // code[i] came from line lines[i]
    std::vector<Value*>   global_cache; // per-constant-slot inline cache for GET_GLOBAL
    std::vector<uint32_t> field_cache;  // per-instruction inline cache for GET_FIELDK/SET_FIELDK
                                        // stores last-seen pair index + 1 (0 = uncached)

    // Add an instruction; return its index
    std::size_t emit(uint64_t instr, uint32_t line = 0);

    // Emit a placeholder JUMP and return its index for later patching
    std::size_t emit_jump(uint64_t instr, uint32_t line = 0);
    void        patch_jump(std::size_t jump_idx);

    // Add a constant; return its pool index (16-bit max)
    uint16_t add_constant(Value v);
    uint16_t add_string(const std::string& s);

    // Disassemble to stdout
    void disasm(const std::string& label = "") const;
    void disasm_instr(std::size_t i) const;

    std::size_t size()   const { return code.size(); }
    std::size_t current_offset() const { return code.size(); }
};

} // namespace syn
