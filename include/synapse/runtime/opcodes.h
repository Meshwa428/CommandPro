#pragma once
#include <cstdint>

namespace syn {

enum class Op : uint8_t {
    // Load / Move (0x01–0x0F)
    LOAD_INT      = 0x01, // I:   R[A] = sign_extend(imm48)
    LOAD_FLOAT    = 0x02, // R:   R[A] = K[B]  (pool float)
    LOAD_TRUE     = 0x03, // I:   R[A] = true
    LOAD_FALSE    = 0x04, // I:   R[A] = false
    LOAD_NONE     = 0x05, // I:   R[A] = none
    LOAD_CONST    = 0x06, // RI:  R[A] = K[imm]
    MOVE          = 0x07, // R:   R[A] = R[B]
    LOAD_DURATION = 0x08, // I:   R[A] = Duration(imm48 ns)

    // Globals & Upvalues (0x10–0x14)
    GET_GLOBAL  = 0x10, // RI: R[A] = globals[imm]
    SET_GLOBAL  = 0x11, // RI: globals[imm] = R[A]
    GET_UPVAL   = 0x12, // R:  R[A] = upvalues[B]
    SET_UPVAL   = 0x13, // R:  upvalues[B] = R[A]
    CLOSE_UPVAL = 0x14, // R:  close upvalues >= R[A]

    // Arithmetic R×R (0x20–0x27)
    ADD  = 0x20, // R: R[A] = R[B] + R[C]
    SUB  = 0x21, // R: R[A] = R[B] - R[C]
    MUL  = 0x22, // R: R[A] = R[B] * R[C]
    DIV  = 0x23, // R: R[A] = R[B] / R[C]  (float result)
    IDIV = 0x24, // R: R[A] = R[B] // R[C] (floor div)
    MOD  = 0x25, // R: R[A] = R[B] % R[C]
    POW  = 0x26, // R: R[A] = R[B] ** R[C]
    UNM  = 0x27, // R: R[A] = -R[B]

    // Arithmetic R×imm (0x28–0x2E)
    ADDI  = 0x28, // RI: R[A] = R[B] + imm40
    SUBI  = 0x29, // RI: R[A] = R[B] - imm40
    MULI  = 0x2A, // RI: R[A] = R[B] * imm40
    DIVI  = 0x2B, // RI: R[A] = R[B] / imm40
    IDIVI = 0x2C, // RI: R[A] = R[B] // imm40
    MODI  = 0x2D, // RI: R[A] = R[B] % imm40
    POWI  = 0x2E, // RI: R[A] = R[B] ** imm40

    // Fused compare-and-branch (0x30–0x3D)
    JEQ  = 0x30, // RRJ: if R[A] == R[B]: pc += off32
    JNEQ = 0x31, // RRJ: if R[A] != R[B]: pc += off32
    JLT  = 0x32, // RRJ: if R[A] <  R[B]: pc += off32
    JLTE = 0x33, // RRJ: if R[A] <= R[B]: pc += off32
    JGT  = 0x34, // RRJ: if R[A] >  R[B]: pc += off32
    JGTE = 0x35, // RRJ: if R[A] >= R[B]: pc += off32
    JT   = 0x36, // RJ:  if truthy(R[A]): pc += off40
    JF   = 0x37, // RJ:  if falsy(R[A]):  pc += off40
    JUMP = 0x38, // J:   pc += off48
    JNIL = 0x39, // RJ:  if R[A] == none: pc += off40
    JNNIL= 0x3A, // RJ:  if R[A] != none: pc += off40

    // Comparison → register (0x40–0x44)
    EQ   = 0x40, // R: R[A] = (R[B] == R[C])
    NEQ  = 0x41, // R: R[A] = (R[B] != R[C])
    LT   = 0x42, // R: R[A] = (R[B] <  R[C])
    LTE  = 0x43, // R: R[A] = (R[B] <= R[C])
    NOT  = 0x44, // R: R[A] = !truthy(R[B])

    // Strings (0x50–0x53)
    CONCAT   = 0x50, // R:  R[A] = R[B] + R[C]
    CONCAT_N = 0x51, // R:  R[A] = concat(R[B..B+C-1])
    STR_LEN  = 0x52, // R:  R[A] = len(R[B])
    INTERP   = 0x53, // R:  R[A] = interpolate(R[B..B+C-1])

    // Collections (0x60–0x6C)
    NEW_LIST  = 0x60, // R:  R[A] = List(R[B..B+C-1])
    NEW_MAP   = 0x61, // R:  R[A] = Map()
    NEW_TUPLE = 0x62, // R:  R[A] = Tuple(R[B..B+C-1])
    NEW_RANGE = 0x63, // R:  R[A] = Range(R[B], R[C])
    GET_FIELD = 0x64, // R:  R[A] = R[B][R[C]]
    SET_FIELD = 0x65, // R:  R[B][R[C]] = R[A]  (A=val, B=obj, C=key)
    GET_FIELDK= 0x66, // RI: R[A] = R[B][K[imm]]
    SET_FIELDK= 0x67, // RI: R[B][K[imm]] = R[A]
    GETI      = 0x68, // RI: R[A] = R[B][imm]
    SETI      = 0x69, // RI: R[A][imm] = R[B]
    APPEND    = 0x6A, // R:  R[A].push(R[B])
    HAS_KEY   = 0x6B, // R:  R[A] = (R[C] in R[B])
    MAP_SETK  = 0x6C, // RI: R[A][K[imm]] = R[B]

    // Loops (0x70–0x77)
    RANGE_PREP   = 0x70, // RANGE: R[A]=start(s16), R[A+1]=stop(s16), R[A+2]=step(s16)
    RANGE_PREP_R = 0x71, // R:     R[A]=R[B]; R[A+1]=R[C]; R[A+2]=1
    RANGE_PREP_RS= 0x72, // R:     R[A]=R[B]; R[A+1]=R[C]; R[A+2]=R[D]  (D in bits 23..16)
    RANGE_STEP   = 0x73, // RJ:    R[A]+=R[A+2]; if R[A]>R[A+1]: pc+=off
    FOR_PREP     = 0x74, // R:     R[A]=iterator(R[B])
    FOR_STEP     = 0x75, // RJ:    R[A+1]=next(R[A]); if done: pc+=off

    // Functions (0x80–0x89)
    CLOSURE  = 0x80, // RI:   R[A] = Closure(K[imm])
    CALL     = 0x81, // CALL: R[A..ret] = R[A](R[A+1..A+nargs])
    CALL_0   = 0x82, // R:    R[A] = R[B]()
    CALL_1   = 0x83, // R:    R[A] = R[B](R[C])
    CALL_N   = 0x84, // R:    R[A](R[A+1..A+B])  discard result
    TAIL     = 0x85, // R:    tail call
    RETURN   = 0x86, // R:    return R[A..A+B-1]
    RETURN_1 = 0x87, // R:    return R[A]
    RETURN_0 = 0x88, // I:    return none
    INVOKE   = 0x89, // CALL: R[A] = R[A].method_id[key](R[A+1..A+nargs]) — key is method enum

    // Type ops (0x90–0x9B)
    TYPEOF   = 0x90,
    TO_INT   = 0x98,
    TO_FLOAT = 0x99,
    TO_STR   = 0x9A,
    TO_BOOL  = 0x9B,

    // Null coalescing (0xA0)
    NULLC    = 0xA0,

    // Error handling (0xB0–0xB6)
    TRY_PUSH = 0xB0,
    TRY_POP  = 0xB1,
    THROW    = 0xB2,
    ERR_NEW  = 0xB3,

    // VM control (0xFD–0xFF)
    NOP    = 0xFD,
    BREAKPT= 0xFE,
    HALT   = 0xFF,
};

// ── Method IDs (resolved at compile time, stored in INVOKE key field) ────────
enum class MethodId : uint32_t {
    Unknown = 0,
    // List
    Append, Pop, Reverse, Sort, Index, Contains, Extend,
    // String
    Upper, Lower, Trim, Strip, Split, Join, Find, Replace,
    StartsWith, EndsWith, Substr, Ord, Count,
    // Shared (list + string + map)
    Len,
    // Map
    Keys, Values, Has, Get,
};

inline MethodId resolve_method_id(const std::string& name)
{
    switch (name.size()) {
    case 3:
        if (name == "pop") return MethodId::Pop;
        if (name == "len") return MethodId::Len;
        if (name == "ord") return MethodId::Ord;
        if (name == "get") return MethodId::Get;
        if (name == "has") return MethodId::Has;
        break;
    case 4:
        if (name == "find") return MethodId::Find;
        if (name == "join") return MethodId::Join;
        if (name == "sort") return MethodId::Sort;
        if (name == "trim") return MethodId::Trim;
        if (name == "keys") return MethodId::Keys;
        break;
    case 5:
        if (name == "upper") return MethodId::Upper;
        if (name == "lower") return MethodId::Lower;
        if (name == "strip") return MethodId::Strip;
        if (name == "split") return MethodId::Split;
        if (name == "index") return MethodId::Index;
        if (name == "count") return MethodId::Count;
        break;
    case 6:
        if (name == "append") return MethodId::Append;
        if (name == "substr") return MethodId::Substr;
        if (name == "extend") return MethodId::Extend;
        if (name == "values") return MethodId::Values;
        break;
    case 7:
        if (name == "replace") return MethodId::Replace;
        if (name == "reverse") return MethodId::Reverse;
        break;
    case 8:
        if (name == "endswith") return MethodId::EndsWith;
        if (name == "contains") return MethodId::Contains;
        break;
    case 9:
        if (name == "ends_with") return MethodId::EndsWith;
        break;
    case 10:
        if (name == "startswith") return MethodId::StartsWith;
        break;
    case 11:
        if (name == "starts_with") return MethodId::StartsWith;
        break;
    default: break;
    }
    return MethodId::Unknown;
}

// ── Instruction encoding helpers ──────────────────────────────────────────────

inline uint64_t enc_I(Op op, uint8_t a, int64_t imm48)
{
    return (uint64_t(op) << 56) | (uint64_t(a) << 48) |
           (uint64_t(imm48) & 0x0000FFFFFFFFFFFFULL);
}

inline uint64_t enc_R(Op op, uint8_t a, uint8_t b, uint8_t c = 0)
{
    return (uint64_t(op) << 56) | (uint64_t(a) << 48) |
           (uint64_t(b) << 40) | (uint64_t(c) << 32);
}

inline uint64_t enc_RI(Op op, uint8_t a, uint8_t b, int64_t imm40)
{
    return (uint64_t(op) << 56) | (uint64_t(a) << 48) | (uint64_t(b) << 40) |
           (uint64_t(imm40) & 0x000000FFFFFFFFFFULL);
}

inline uint64_t enc_J(Op op, int64_t off48)
{
    return (uint64_t(op) << 56) | (uint64_t(off48) & 0x0000FFFFFFFFFFFFULL);
}

inline uint64_t enc_RJ(Op op, uint8_t a, int64_t off40)
{
    return (uint64_t(op) << 56) | (uint64_t(a) << 48) |
           (uint64_t(off40) & 0x000000FFFFFFFFFFULL);
}

// RRJ: OP(8) A(8) B(8) C(8) off32(32)
inline uint64_t enc_RRJ(Op op, uint8_t a, uint8_t b, int32_t off32)
{
    return (uint64_t(op) << 56) | (uint64_t(a) << 48) | (uint64_t(b) << 40) |
           (uint64_t(uint32_t(off32)));
}

// RANGE: OP(8) A(8) start(16) stop(16) step(16)
inline uint64_t enc_RANGE(Op op, uint8_t a, int16_t start, int16_t stop, int16_t step)
{
    return (uint64_t(op) << 56) | (uint64_t(a) << 48) |
           (uint64_t(uint16_t(start)) << 32) | (uint64_t(uint16_t(stop)) << 16) |
           uint64_t(uint16_t(step));
}

// CALL: OP(8) A(8) nargs(8) nret(8) key(32)
inline uint64_t enc_CALL(Op op, uint8_t a, uint8_t nargs, uint8_t nret, uint32_t key = 0)
{
    return (uint64_t(op) << 56) | (uint64_t(a) << 48) | (uint64_t(nargs) << 40) |
           (uint64_t(nret) << 32) | uint64_t(key);
}

// ── Instruction decode helpers ────────────────────────────────────────────────
inline Op      INS_OP(uint64_t w)    { return Op(w >> 56); }
inline uint8_t INS_A(uint64_t w)     { return uint8_t(w >> 48); }
inline uint8_t INS_B(uint64_t w)     { return uint8_t(w >> 40); }
inline uint8_t INS_C(uint64_t w)     { return uint8_t(w >> 32); }
inline uint8_t INS_D(uint64_t w)     { return uint8_t(w >> 24); }

inline int64_t INS_IMM48(uint64_t w) {
    uint64_t raw = w & 0x0000FFFFFFFFFFFFULL;
    return (raw & (1ULL<<47)) ? int64_t(raw | 0xFFFF000000000000ULL) : int64_t(raw);
}
inline int64_t INS_IMM40(uint64_t w) {
    uint64_t raw = w & 0x000000FFFFFFFFFFULL;
    return (raw & (1ULL<<39)) ? int64_t(raw | 0xFFFFFF0000000000ULL) : int64_t(raw);
}
inline int32_t INS_OFF32(uint64_t w) { return int32_t(uint32_t(w)); }
inline int16_t INS_S16_A(uint64_t w) { return int16_t(uint16_t(w >> 32)); } // start
inline int16_t INS_S16_B(uint64_t w) { return int16_t(uint16_t(w >> 16)); } // stop
inline int16_t INS_S16_C(uint64_t w) { return int16_t(uint16_t(w));       } // step

} // namespace syn
