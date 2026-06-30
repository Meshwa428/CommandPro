#include "synapse/runtime/chunk.h"
#include "synapse/runtime/opcodes.h"
#include <iostream>
#include <iomanip>
#include <cstdio>

namespace syn {

std::size_t Chunk::emit(uint64_t instr, uint32_t line)
{
    std::size_t idx = code.size();
    code.push_back(instr);
    lines.push_back(line);
    field_cache.push_back(0);  // 0 = uncached
    return idx;
}

std::size_t Chunk::emit_jump(uint64_t instr, uint32_t line)
{
    return emit(instr, line);
}

void Chunk::patch_jump(std::size_t jump_idx)
{
    // offset = target - (jump_idx + 1)  (relative to NEXT instruction)
    int64_t offset = int64_t(code.size()) - int64_t(jump_idx) - 1;
    uint64_t& w = code[jump_idx];
    Op op = INS_OP(w);

    // Replace the offset bits depending on format
    uint8_t a = INS_A(w);
    uint8_t b = INS_B(w);

    switch (op) {
    case Op::JUMP:
        // Format J: OP(8) flags(8) off48(48)
        w = enc_J(op, offset);
        break;
    case Op::JF: case Op::JT: case Op::JNIL: case Op::JNNIL:
        // Format RJ
        w = enc_RJ(op, a, offset);
        break;
    case Op::JEQ: case Op::JNEQ: case Op::JLT: case Op::JLTE:
    case Op::JGT: case Op::JGTE:
        // Format RRJ
        w = enc_RRJ(op, a, b, int32_t(offset));
        break;
    case Op::RANGE_STEP:
        // RJ format
        w = enc_RJ(op, a, offset);
        break;
    case Op::FOR_STEP:
        w = enc_RJ(op, a, offset);
        break;
    default:
        // Shouldn't happen
        break;
    }
}

uint16_t Chunk::add_constant(Value v)
{
    // deduplicate int/float
    for (std::size_t i = 0; i < constants.size(); ++i) {
        if (constants[i].raw == v.raw) return uint16_t(i);
    }
    constants.push_back(v);
    global_cache.push_back(nullptr);
    return uint16_t(constants.size() - 1);
}

uint16_t Chunk::add_string(const std::string& s)
{
    // deduplicate string constants
    for (std::size_t i = 0; i < constants.size(); ++i) {
        if (val_is_string(constants[i]) && as_str(constants[i]).data == s)
            return uint16_t(i);
    }
    auto* obj = new ObjString(s);
    constants.push_back(Value::from_ptr(obj));
    global_cache.push_back(nullptr);
    return uint16_t(constants.size() - 1);
}

static const char* op_name(Op op)
{
    switch (op) {
#define X(O) case Op::O: return #O;
    X(LOAD_INT) X(LOAD_FLOAT) X(LOAD_TRUE) X(LOAD_FALSE) X(LOAD_NONE)
    X(LOAD_CONST) X(MOVE) X(LOAD_DURATION)
    X(GET_GLOBAL) X(SET_GLOBAL) X(GET_UPVAL) X(SET_UPVAL) X(CLOSE_UPVAL)
    X(ADD) X(SUB) X(MUL) X(DIV) X(IDIV) X(MOD) X(POW) X(UNM)
    X(ADDI) X(SUBI) X(MULI) X(DIVI) X(IDIVI) X(MODI) X(POWI)
    X(JEQ) X(JNEQ) X(JLT) X(JLTE) X(JGT) X(JGTE) X(JT) X(JF)
    X(JUMP) X(JNIL) X(JNNIL)
    X(EQ) X(NEQ) X(LT) X(LTE) X(NOT)
    X(CONCAT) X(CONCAT_N) X(STR_LEN) X(INTERP)
    X(NEW_LIST) X(NEW_MAP) X(NEW_TUPLE) X(NEW_RANGE)
    X(GET_FIELD) X(SET_FIELD) X(GET_FIELDK) X(SET_FIELDK)
    X(GETI) X(SETI) X(APPEND) X(HAS_KEY) X(MAP_SETK)
    X(RANGE_PREP) X(RANGE_PREP_R) X(RANGE_PREP_RS) X(RANGE_STEP)
    X(FOR_PREP) X(FOR_STEP)
    X(CLOSURE) X(CALL) X(CALL_0) X(CALL_1) X(CALL_N)
    X(TAIL) X(RETURN) X(RETURN_1) X(RETURN_0)
    X(TYPEOF) X(TO_INT) X(TO_FLOAT) X(TO_STR) X(TO_BOOL)
    X(NULLC) X(TRY_PUSH) X(TRY_POP) X(THROW) X(ERR_NEW)
    X(NOP) X(BREAKPT) X(HALT)
#undef X
    default: return "???";
    }
}

void Chunk::disasm(const std::string& label) const
{
    std::cout << "=== " << (label.empty() ? name : label) << " ===\n";
    for (std::size_t i = 0; i < code.size(); ++i) disasm_instr(i);
    std::cout << '\n';
    // recurse into nested function chunks
    for (auto& c : constants)
        if (c.is_ptr() && c.as_ptr()->kind == ObjKind::Function) {
            auto* fn = static_cast<ObjFunction*>(c.as_ptr());
            if (fn->chunk && fn->chunk != this) fn->chunk->disasm(fn->name);
        }
}

void Chunk::disasm_instr(std::size_t i) const
{
    uint64_t w = code[i];
    Op       op = INS_OP(w);
    uint8_t  a = INS_A(w), b = INS_B(w), c = INS_C(w);

    std::printf("%04zu  %-14s ", i, op_name(op));

    switch (op) {
    case Op::LOAD_INT: case Op::LOAD_DURATION:
        std::printf("r%d, %lld\n", a, (long long)INS_IMM48(w)); break;
    case Op::LOAD_TRUE: case Op::LOAD_FALSE: case Op::LOAD_NONE:
        std::printf("r%d\n", a); break;
    case Op::LOAD_CONST:
        std::printf("r%d, K[%lld] = %s\n", a, (long long)INS_IMM48(w),
                    val_to_string(constants[INS_IMM48(w)]).c_str()); break;
    case Op::GET_GLOBAL: case Op::SET_GLOBAL: case Op::CLOSURE:
        std::printf("r%d, K[%lld]\n", a, (long long)INS_IMM48(w)); break;
    case Op::MOVE: case Op::UNM: case Op::NOT:
    case Op::TO_INT: case Op::TO_FLOAT: case Op::TO_STR: case Op::TO_BOOL:
    case Op::STR_LEN: case Op::TYPEOF: case Op::FOR_PREP:
        std::printf("r%d, r%d\n", a, b); break;
    case Op::ADD: case Op::SUB: case Op::MUL: case Op::DIV:
    case Op::IDIV: case Op::MOD: case Op::POW:
    case Op::EQ: case Op::NEQ: case Op::LT: case Op::LTE:
    case Op::CONCAT: case Op::GET_FIELD: case Op::HAS_KEY:
    case Op::APPEND: case Op::NULLC:
        std::printf("r%d, r%d, r%d\n", a, b, c); break;
    case Op::ADDI: case Op::SUBI: case Op::MULI: case Op::DIVI:
    case Op::IDIVI: case Op::MODI: case Op::POWI:
    case Op::GET_UPVAL: case Op::SET_UPVAL:
        std::printf("r%d, r%d, %lld\n", a, b, (long long)INS_IMM40(w)); break;
    case Op::JT: case Op::JF: case Op::JNIL: case Op::JNNIL: case Op::FOR_STEP:
        std::printf("r%d -> %zu\n", a, std::size_t(int64_t(i)+1+INS_IMM40(w))); break;
    case Op::JEQ: case Op::JNEQ: case Op::JLT: case Op::JLTE: case Op::JGT: case Op::JGTE:
        std::printf("r%d, r%d -> %zu\n", a, b,
                    std::size_t(int64_t(i)+1+INS_OFF32(w))); break;
    case Op::JUMP:
        std::printf("-> %zu\n", std::size_t(int64_t(i)+1+INS_IMM48(w))); break;
    case Op::RANGE_STEP:
        std::printf("r%d -> %zu\n", a, std::size_t(int64_t(i)+1+INS_IMM40(w))); break;
    case Op::RANGE_PREP:
        std::printf("r%d, %d to %d by %d\n", a, INS_S16_A(w), INS_S16_B(w), INS_S16_C(w)); break;
    case Op::RANGE_PREP_R:
        std::printf("r%d, r%d..r%d\n", a, b, c); break;
    case Op::CALL:
        std::printf("r%d, nargs=%d, nret=%d\n", a, b, c); break;
    case Op::CALL_0:
        std::printf("r%d, r%d()\n", a, b); break;
    case Op::CALL_1:
        std::printf("r%d, r%d(r%d)\n", a, b, c); break;
    case Op::CALL_N:
        std::printf("r%d(r%d..r%d)\n", a, a+1, a+b); break;
    case Op::RETURN: case Op::NEW_LIST: case Op::NEW_TUPLE: case Op::CONCAT_N: case Op::INTERP:
        std::printf("r%d..r%d (%d)\n", b, b+c-1, c); break;
    case Op::RETURN_1:
        std::printf("r%d\n", a); break;
    case Op::RETURN_0: case Op::HALT: case Op::NOP:
        std::printf("\n"); break;
    case Op::SET_FIELD:
        std::printf("r%d[r%d] = r%d\n", b, c, a); break;
    case Op::GET_FIELDK:
        std::printf("r%d, r%d[K[%lld]]\n", a, b, (long long)INS_IMM48(w)); break;
    case Op::SET_FIELDK:
        std::printf("r%d[K[%lld]] = r%d\n", b, (long long)INS_IMM48(w), a); break;
    case Op::GETI:
        std::printf("r%d = r%d[%lld]\n", a, b, (long long)INS_IMM40(w)); break;
    case Op::SETI:
        std::printf("r%d[%lld] = r%d\n", a, (long long)INS_IMM40(w), b); break;
    case Op::MAP_SETK:
        std::printf("r%d[K[%lld]] = r%d\n", a, (long long)INS_IMM48(w), b); break;
    case Op::TRY_PUSH:
        std::printf("r%d -> %zu\n", a, std::size_t(int64_t(i)+1+INS_IMM40(w))); break;
    case Op::THROW: case Op::ERR_NEW: case Op::TRY_POP:
        std::printf("r%d\n", a); break;
    default:
        std::printf("r%d r%d r%d\n", a, b, c); break;
    }
}

} // namespace syn
