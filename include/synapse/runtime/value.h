#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include <unordered_map>

namespace syn {

// ── NaN-boxing layout ─────────────────────────────────────────────────────────
// Non-NaN double   → stored as-is (bits 62..52 != 0x7FF or mantissa = 0)
// Tagged (quiet NaN space, bits 63..48 >= 0xFFF8):
//   bits 63..51 = 0x7FF (quiet NaN marker)
//   bits 50..48 = tag (3 bits)
//   bits 47..0  = payload (48 bits)
//
// Tag encoding (bits 50..48 of the 0xFFF8-based prefix):
//   0 → int48   (payload = signed 48-bit integer)
//   1 → true
//   2 → false
//   3 → none
//   4 → pointer (payload = heap Obj*)
//   5 → duration (payload = nanoseconds, signed 48-bit)

static constexpr uint64_t VNAN_BASE  = 0xFFF8000000000000ULL; // tag-0 base
static constexpr uint64_t VPAY_MASK  = 0x0000FFFFFFFFFFFFULL; // bits 47..0

static constexpr uint64_t V_TRUE     = VNAN_BASE | (1ULL << 48);
static constexpr uint64_t V_FALSE    = VNAN_BASE | (2ULL << 48);
static constexpr uint64_t V_NONE     = VNAN_BASE | (3ULL << 48);
static constexpr uint64_t V_PTR      = VNAN_BASE | (4ULL << 48); // OR with ptr
static constexpr uint64_t V_DUR      = VNAN_BASE | (5ULL << 48); // OR with ns
// tag 6: pointer to ObjInt (large int outside 48-bit signed range)
static constexpr uint64_t V_LARGEINT = VNAN_BASE | (6ULL << 48); // OR with ObjInt*

struct Obj; // forward

struct Value {
    uint64_t raw = V_NONE;

    // ── Type checks ──────────────────────────────────────────────────────────
    bool is_tagged()    const { return raw >= VNAN_BASE; }
    bool is_float()     const { return !is_tagged(); }
    bool is_small_int() const { return (raw >> 48) == 0xFFF8; }
    bool is_large_int() const { return (raw >> 48) == (0xFFF8 | 6); }  // pointer to ObjInt
    bool is_int()       const { return is_small_int() || is_large_int(); }
    bool is_true()      const { return raw == V_TRUE; }
    bool is_false()     const { return raw == V_FALSE; }
    bool is_bool()      const { return raw == V_TRUE || raw == V_FALSE; }
    bool is_none()      const { return raw == V_NONE; }
    bool is_ptr()       const { return (raw >> 48) == (0xFFF8 | 4); }
    bool is_duration()  const { return (raw >> 48) == (0xFFF8 | 5); }

    // ── Value access ─────────────────────────────────────────────────────────
    double as_float() const { double d; std::memcpy(&d, &raw, 8); return d; }

    int64_t as_int() const;  // inline below, after ObjInt is defined
    bool as_bool() const { return raw == V_TRUE; }

    Obj* as_ptr() const {
        return reinterpret_cast<Obj*>(raw & VPAY_MASK);
    }
    int64_t as_duration() const {
        uint64_t p = raw & VPAY_MASK;
        return (p & (1ULL<<47)) ? int64_t(p | ~VPAY_MASK) : int64_t(p);
    }

    // ── Truthiness ────────────────────────────────────────────────────────────
    bool truthy() const;

    // ── Factory ──────────────────────────────────────────────────────────────
    static Value from_float(double d) { Value v; std::memcpy(&v.raw, &d, 8); return v; }
    static Value from_int(int64_t i);  // inline below, after ObjInt + val_box_large_int
    static Value from_bool(bool b) { return {b ? V_TRUE : V_FALSE}; }
    static Value none_val()        { return {V_NONE}; }
    static Value from_ptr(Obj* o)  {
        return {V_PTR | (reinterpret_cast<uint64_t>(o) & VPAY_MASK)};
    }
    static Value from_duration(int64_t ns) {
        return {V_DUR | (uint64_t(ns) & VPAY_MASK)};
    }

    bool operator==(const Value& o) const { return raw == o.raw; }
    bool operator!=(const Value& o) const { return raw != o.raw; }
};
static_assert(sizeof(Value) == 8);

// ── Heap object types ─────────────────────────────────────────────────────────

enum class ObjKind : uint8_t {
    String, List, Tuple, Map, Function, Closure, Upvalue, Native, Error, Int
    // List and Tuple are adjacent so GETI can do a single range check:
    //   kind <= ObjKind::Tuple  →  kind is List or Tuple
};

struct Obj {
    ObjKind kind;
    bool    gc_mark = false;
    Obj*    gc_next = nullptr;
};

struct ObjInt : Obj {
    int64_t value;
    explicit ObjInt(int64_t v) : value(v) { kind = ObjKind::Int; }
};

struct ObjString : Obj {
    std::string data;
    uint32_t    hash        = 0;
    bool        is_interned = false;
    explicit ObjString(std::string s);
    ObjString(std::string s, uint32_t h);  // intern-path ctor (hash pre-computed)
};

// Small-buffer vector for ObjList: holds first 4 Values inline (no heap alloc
// for lists of ≤4 elements). 4 covers 2-element nodes (linked_list) and
// 3-element tuples (binary_trees) without spilling. ObjList = 64 bytes = 1 cache line.
struct ListItems {
    static constexpr uint32_t INLINE_CAP = 4;
    Value    _buf[INLINE_CAP];        // inline storage, always part of the struct
    Value*   _data;                   // points to _buf (small) or heap (large)
    uint32_t _size = 0;
    uint32_t _cap  = INLINE_CAP;

    ListItems() noexcept : _data(_buf) {}
    ListItems(const ListItems&) = delete;
    ListItems& operator=(const ListItems&) = delete;
    ~ListItems() { if (_data != _buf) delete[] _data; }

    bool     empty()  const noexcept { return _size == 0; }
    uint32_t size()   const noexcept { return _size; }
    Value*   begin()        noexcept { return _data; }
    Value*   end()          noexcept { return _data + _size; }
    const Value* begin() const noexcept { return _data; }
    const Value* end()   const noexcept { return _data + _size; }
    Value& operator[](size_t i)       noexcept { return _data[i]; }
    const Value& operator[](size_t i) const noexcept { return _data[i]; }
    Value& back()       noexcept { return _data[_size - 1]; }
    const Value& back() const noexcept { return _data[_size - 1]; }
    Value& at(size_t i) {
        if (i >= _size) throw std::out_of_range("list index out of range");
        return _data[i];
    }

    void clear()            noexcept { _size = 0; }
    void pop_back()         noexcept { if (_size) --_size; }
    void push_back(const Value& v)   { if (_size == _cap) _grow(); _data[_size++] = v; }

    void assign(const Value* first, const Value* last) {
        size_t n = size_t(last - first);
        if (n > _cap) _reserve_exact(n);
        if (n) std::memcpy(_data, first, n * sizeof(Value));
        _size = uint32_t(n);
    }
    void reserve(size_t n) { if (n > _cap) _reserve_exact(n); }

private:
    void _grow() { _reserve_exact(_cap == 0 ? INLINE_CAP : size_t(_cap) * 2); }
    void _reserve_exact(size_t nc) {
        Value* nd = new Value[nc];
        if (_size) std::memcpy(nd, _data, _size * sizeof(Value));
        if (_data != _buf) delete[] _data;
        _data = nd;
        _cap = uint32_t(nc);
    }
};

struct ObjList : Obj {
    ListItems items;
    ObjList() { kind = ObjKind::List; }
};

struct ObjMap : Obj {
    std::vector<std::pair<Value,Value>> pairs;  // canonical storage; GC/keys()/values() use this
    // Hash indexes: allocated lazily once map exceeds HASH_THRESHOLD entries.
    // ponytail: linear scan beats hash for ≤16 entries
    static constexpr size_t HASH_THRESHOLD = 16;
    std::unordered_map<std::string, size_t>* str_idx  = nullptr;
    // Flat open-addressing table for int keys: faster than std::unordered_map
    // (no pointer-chasing, sequential probing, cache-line friendly).
    // key == INT64_MIN is the empty-slot sentinel.
    // key == INT64_MIN is the empty-slot sentinel.
    struct FlatIntSlot { int64_t key; uint32_t pairs_idx; uint32_t _pad; };
    FlatIntSlot* int_flat      = nullptr;
    uint32_t     int_flat_cap  = 0;  // always power of 2
    uint32_t     int_flat_count = 0;
    ObjMap() { kind = ObjKind::Map; }
    ~ObjMap() { delete str_idx; delete[] int_flat; }
    Value get(Value key) const;
    void  set(Value key, Value val);
    void  build_index();  // called once when crossing HASH_THRESHOLD
};

struct Chunk; // forward

struct ObjFunction : Obj {
    std::string  name;
    int          arity         = 0;     // total param slots (incl. rest param)
    int          upvalue_count = 0;
    bool         has_rest      = false; // last param is *rest: extras packed into a list
    Chunk*       chunk         = nullptr;
    ObjFunction();
    ~ObjFunction();
};

using NativeFn = std::function<Value(int argc, Value* args)>;

struct ObjNative : Obj {
    NativeFn    fn;
    std::string name;
    ObjNative(std::string n, NativeFn f) : fn(std::move(f)), name(std::move(n)) {
        kind = ObjKind::Native;
    }
};

struct ObjUpvalue : Obj {
    Value*      location;   // points into register stack (open)
    Value       closed;     // value after scope exit (closed)
    bool        is_closed = false;
    ObjUpvalue* next_open = nullptr; // intrusive list of open upvalues

    explicit ObjUpvalue(Value* loc) : location(loc) { kind = ObjKind::Upvalue; }
    Value get()       const { return is_closed ? closed : *location; }
    void  set(Value v)      { if (is_closed) closed = v; else *location = v; }
    void  close()           { closed = *location; is_closed = true; }
};

struct ObjClosure : Obj {
    ObjFunction*              fn;
    std::vector<ObjUpvalue*>  upvalues;
    void*                     jit_cache = nullptr;  // nullptr=unchecked, -1=no JIT, else JitEntry*
    explicit ObjClosure(ObjFunction* f);
};

struct ObjError : Obj {
    Value       message;
    std::string type;
    ObjError(Value msg, std::string t = "RuntimeError")
        : message(msg), type(std::move(t)) { kind = ObjKind::Error; }
};

// ── Obj type helpers ──────────────────────────────────────────────────────────
inline bool val_is_string(Value v)  { return v.is_ptr() && v.as_ptr()->kind == ObjKind::String; }
inline bool val_is_list(Value v)    { return v.is_ptr() && v.as_ptr()->kind == ObjKind::List; }
inline bool val_is_map(Value v)     { return v.is_ptr() && v.as_ptr()->kind == ObjKind::Map; }
inline bool val_is_closure(Value v) { return v.is_ptr() && v.as_ptr()->kind == ObjKind::Closure; }
inline bool val_is_native(Value v)  { return v.is_ptr() && v.as_ptr()->kind == ObjKind::Native; }
inline bool val_is_callable(Value v){
    if (!v.is_ptr()) return false;
    auto k = v.as_ptr()->kind;
    return k == ObjKind::Closure || k == ObjKind::Native || k == ObjKind::Function;
}

inline ObjString&  as_str(Value v)     { return *static_cast<ObjString*>(v.as_ptr()); }
inline ObjList&    as_list(Value v)    { return *static_cast<ObjList*>(v.as_ptr()); }
inline ObjMap&     as_map(Value v)     { return *static_cast<ObjMap*>(v.as_ptr()); }
inline ObjClosure& as_closure(Value v) { return *static_cast<ObjClosure*>(v.as_ptr()); }
inline ObjNative&  as_native(Value v)  { return *static_cast<ObjNative*>(v.as_ptr()); }

// Inlined: called on every JF/JT/NOT. Fast paths (bool/none/int/float) avoid an
// out-of-line call; string/list emptiness is the rare tail.
inline bool Value::truthy() const
{
    if (raw == V_TRUE)  return true;
    if (raw == V_FALSE || raw == V_NONE) return false;
    if (is_int())   return as_int() != 0;
    if (is_float()) { double d = as_float(); return d != 0.0 && d == d; }  // d==d: not NaN
    if (val_is_string(*this)) return !as_str(*this).data.empty();
    if (val_is_list(*this))   return !as_list(*this).items.empty();
    return true;
}

// Thread-local current VM (set by VM::run; used by Value::from_int to alloc ObjInt)
class VM;
extern thread_local VM* tls_vm;
// Slow path for from_int when value exceeds signed 48-bit range
Value val_box_large_int(int64_t v);

// ── Arithmetic & comparison ───────────────────────────────────────────────────
// Slow-path helpers (defined in value.cpp, handle mixed/string cases)
Value val_add_slow(Value a, Value b);
Value val_sub_slow(Value a, Value b);
Value val_mul_slow(Value a, Value b);
Value val_div_slow(Value a, Value b);
bool  val_lt_slow(Value a, Value b);
bool  val_lte_slow(Value a, Value b);

// Fast-path inlines — int/float hot cases stay in the same TU as the caller
inline Value val_add(Value a, Value b) {
    if (a.is_int()   && b.is_int())   return Value::from_int(a.as_int()   + b.as_int());
    if (a.is_float() && b.is_float()) return Value::from_float(a.as_float() + b.as_float());
    return val_add_slow(a, b);
}
inline Value val_sub(Value a, Value b) {
    if (a.is_int()   && b.is_int())   return Value::from_int(a.as_int()   - b.as_int());
    if (a.is_float() && b.is_float()) return Value::from_float(a.as_float() - b.as_float());
    return val_sub_slow(a, b);
}
inline Value val_mul(Value a, Value b) {
    if (a.is_int()   && b.is_int())   return Value::from_int(a.as_int()   * b.as_int());
    if (a.is_float() && b.is_float()) return Value::from_float(a.as_float() * b.as_float());
    return val_mul_slow(a, b);
}
inline Value val_div(Value a, Value b) {
    if (a.is_float() && b.is_float()) {
        double db = b.as_float();
        if (__builtin_expect(db == 0.0, 0)) throw std::runtime_error("division by zero");
        return Value::from_float(a.as_float() / db);
    }
    return val_div_slow(a, b);
}
inline bool val_lt(Value a, Value b) {
    if (a.is_int()   && b.is_int())   return a.as_int()   < b.as_int();
    if (a.is_float() && b.is_float()) return a.as_float() < b.as_float();
    return val_lt_slow(a, b);
}
inline bool val_lte(Value a, Value b) {
    if (a.is_int()   && b.is_int())   return a.as_int()   <= b.as_int();
    if (a.is_float() && b.is_float()) return a.as_float() <= b.as_float();
    return val_lte_slow(a, b);
}

Value val_idiv(Value a, Value b);
Value val_mod(Value a, Value b);
Value val_pow(Value a, Value b);
Value val_unm(Value a);
bool  val_eq(Value a, Value b);

std::string val_to_string(Value v);

// Inline definitions that require ObjInt and val_box_large_int to be complete

inline int64_t Value::as_int() const
{
    if (__builtin_expect(is_small_int(), 1)) {
        uint64_t p = raw & VPAY_MASK;
        return (p & (1ULL<<47)) ? int64_t(p | ~VPAY_MASK) : int64_t(p);
    }
    return reinterpret_cast<ObjInt*>(raw & VPAY_MASK)->value;
}

inline Value Value::from_int(int64_t i)
{
    if (__builtin_expect(i >= -(1LL<<47) && i < (1LL<<47), 1))
        return {VNAN_BASE | (uint64_t(i) & VPAY_MASK)};
    return val_box_large_int(i);
}

} // namespace syn
