#include "synapse/runtime/value.h"
#include "synapse/runtime/vm.h"
#include "synapse/runtime/chunk.h"
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <charconv>

namespace syn {

// ── Large-int boxing ──────────────────────────────────────────────────────────

Value val_box_large_int(int64_t v)
{
    ObjInt* o;
    if (tls_vm) o = tls_vm->alloc_int(v);   // pooled
    else { o = new ObjInt(v); }  // outside VM (JIT __main__ path) — GC won't track this
    return {V_LARGEINT | (reinterpret_cast<uint64_t>(o) & VPAY_MASK)};
}

// ── ObjString ─────────────────────────────────────────────────────────────────

ObjString::ObjString(std::string s) : data(std::move(s))
{
    kind = ObjKind::String;
    hash = 2166136261u;
    for (unsigned char c : data) hash = (hash ^ c) * 16777619u;
}

ObjString::ObjString(std::string s, uint32_t h) : data(std::move(s)), hash(h)
{
    kind = ObjKind::String;
}

// ── ObjMap flat int hash helpers ──────────────────────────────────────────────

static inline uint32_t flat_int_hash(int64_t k)
{
    // Fibonacci/Knuth multiplicative hash — good distribution for sequential ints
    return uint32_t(uint64_t(k) * 11400714819323198485ULL >> 32);
}

// Sentinel: INT64_MIN (0x8000000000000000) = empty slot.
// Valid map keys go through val_from_int which NaN-boxes them; the actual int64_t
// stored in FlatIntSlot.key can be any value except INT64_MIN as empty marker.
static constexpr int64_t FLAT_EMPTY = INT64_MIN;

static inline uint32_t flat_int_find_slot(const ObjMap::FlatIntSlot* t,
                                           uint32_t cap, int64_t key)
{
    uint32_t h = flat_int_hash(key) & (cap - 1);
    while (t[h].key != FLAT_EMPTY && t[h].key != key)
        h = (h + 1) & (cap - 1);
    return h;
}

static void flat_int_grow(ObjMap* m)
{
    uint32_t new_cap = m->int_flat_cap * 2;
    auto* t = new ObjMap::FlatIntSlot[new_cap];
    for (uint32_t i = 0; i < new_cap; ++i) t[i].key = FLAT_EMPTY;
    for (uint32_t i = 0; i < m->int_flat_cap; ++i) {
        if (m->int_flat[i].key != FLAT_EMPTY) {
            uint32_t s = flat_int_find_slot(t, new_cap, m->int_flat[i].key);
            t[s] = m->int_flat[i];  // copies key, val, pairs_idx
        }
    }
    delete[] m->int_flat;
    m->int_flat     = t;
    m->int_flat_cap = new_cap;
}

// ── ObjMap methods ────────────────────────────────────────────────────────────

void ObjMap::build_index()
{
    str_idx = new std::unordered_map<std::string, size_t>();
    // Flat int table: power-of-2 capacity, start at 32 (> HASH_THRESHOLD=16, <75% load)
    int_flat_cap   = 32;
    int_flat_count = 0;
    int_flat = new FlatIntSlot[32];
    for (uint32_t i = 0; i < 32; ++i) int_flat[i].key = FLAT_EMPTY;
    for (size_t i = 0; i < pairs.size(); ++i) {
        Value k = pairs[i].first;
        if (val_is_string(k)) (*str_idx)[as_str(k).data] = i;
        else if (k.is_int()) {
            uint32_t s = flat_int_find_slot(int_flat, int_flat_cap, k.as_int());
            int_flat[s] = { k.as_int(), uint32_t(i), 0 };
            ++int_flat_count;
        }
    }
}

Value ObjMap::get(Value key) const
{
    if (str_idx && val_is_string(key)) {
        auto it = str_idx->find(as_str(key).data);
        return it == str_idx->end() ? Value::none_val() : pairs[it->second].second;
    }
    if (int_flat && key.is_int()) {
        uint32_t s = flat_int_find_slot(int_flat, int_flat_cap, key.as_int());
        return int_flat[s].key == FLAT_EMPTY ? Value::none_val() : pairs[int_flat[s].pairs_idx].second;
    }
    for (auto& [k, v] : pairs)
        if (val_eq(k, key)) return v;
    return Value::none_val();
}

void ObjMap::set(Value key, Value val)
{
    if (str_idx && val_is_string(key)) {
        auto& k = as_str(key).data;
        auto it = str_idx->find(k);
        if (it != str_idx->end()) { pairs[it->second].second = val; return; }
        (*str_idx)[k] = pairs.size();
        pairs.push_back({key, val});
        return;
    }
    if (int_flat && key.is_int()) {
        int64_t ki = key.as_int();
        uint32_t s = flat_int_find_slot(int_flat, int_flat_cap, ki);
        if (int_flat[s].key != FLAT_EMPTY) { pairs[int_flat[s].pairs_idx].second = val; return; }
        int_flat[s] = { ki, uint32_t(pairs.size()), 0 };
        pairs.push_back({key, val});
        if (++int_flat_count * 4 > int_flat_cap * 3) flat_int_grow(this);
        return;
    }
    // Linear scan (small map or unsupported key type)
    for (auto& [k, v] : pairs) {
        if (val_eq(k, key)) { v = val; return; }
    }
    pairs.push_back({key, val});
    // Build hash index once map crosses threshold
    if (!str_idx && pairs.size() == HASH_THRESHOLD + 1)
        build_index();
}

// ── ObjFunction ───────────────────────────────────────────────────────────────

ObjFunction::ObjFunction() { kind = ObjKind::Function; chunk = new Chunk(); }
ObjFunction::~ObjFunction() { delete chunk; }

// ── ObjClosure ───────────────────────────────────────────────────────────────

ObjClosure::ObjClosure(ObjFunction* f) : fn(f)
{
    kind = ObjKind::Closure;
    upvalues.resize(f->upvalue_count, nullptr);
}

// ── Truthiness ────────────────────────────────────────────────────────────────

// Value::truthy() is defined inline in value.h (hot path: JF/JT/NOT).

// ── Arithmetic ────────────────────────────────────────────────────────────────

static double to_num(Value v)
{
    if (v.is_int())   return static_cast<double>(v.as_int());
    if (v.is_float()) return v.as_float();
    throw std::runtime_error("value is not a number");
}

static bool both_int(Value a, Value b) { return a.is_int() && b.is_int(); }

// Slow paths — hot cases (both_int, both_float) handled inline in value.h

Value val_add_slow(Value a, Value b)
{
    if (val_is_string(a) && val_is_string(b)) {
        auto* s = new ObjString(as_str(a).data + as_str(b).data);
        return Value::from_ptr(s);
    }
    return Value::from_float(to_num(a) + to_num(b));
}

Value val_sub_slow(Value a, Value b) { return Value::from_float(to_num(a) - to_num(b)); }
Value val_mul_slow(Value a, Value b) { return Value::from_float(to_num(a) * to_num(b)); }

Value val_div_slow(Value a, Value b)
{
    double da = to_num(a), db = to_num(b);
    if (db == 0.0) throw std::runtime_error("division by zero");
    return Value::from_float(da / db);
}

bool val_lt_slow(Value a, Value b)
{
    if (val_is_string(a) && val_is_string(b)) return as_str(a).data < as_str(b).data;
    return to_num(a) < to_num(b);
}

bool val_lte_slow(Value a, Value b) { return to_num(a) <= to_num(b); }

Value val_idiv(Value a, Value b)
{
    if (both_int(a, b)) {
        int64_t ia = a.as_int(), ib = b.as_int();
        if (ib == 0) throw std::runtime_error("floor division by zero");
        int64_t q = ia / ib;
        // floor division: adjust if signs differ and remainder is non-zero
        if ((ia ^ ib) < 0 && q * ib != ia) q--;
        return Value::from_int(q);
    }
    double da = to_num(a), db = to_num(b);
    return Value::from_float(std::floor(da / db));
}

Value val_mod(Value a, Value b)
{
    if (both_int(a, b)) {
        int64_t ia = a.as_int(), ib = b.as_int();
        if (ib == 0) throw std::runtime_error("modulo by zero");
        int64_t r = ia % ib;
        if (r != 0 && (r ^ ib) < 0) r += ib; // Python-style mod
        return Value::from_int(r);
    }
    double da = to_num(a), db = to_num(b);
    return Value::from_float(std::fmod(da, db));
}

Value val_pow(Value a, Value b)
{
    if (both_int(a, b) && b.as_int() >= 0) {
        int64_t base = a.as_int(), exp = b.as_int(), result = 1;
        while (exp > 0) {
            if (exp & 1) result *= base;
            base *= base; exp >>= 1;
        }
        return Value::from_int(result);
    }
    return Value::from_float(std::pow(to_num(a), to_num(b)));
}

Value val_unm(Value a)
{
    if (a.is_int())   return Value::from_int(-a.as_int());
    return Value::from_float(-to_num(a));
}

// ── Comparison ────────────────────────────────────────────────────────────────

bool val_eq(Value a, Value b)
{
    if (a.raw == b.raw) return true;
    // int vs int (covers large-int ObjInt pointers with same value but different address)
    if (a.is_int() && b.is_int())     return a.as_int() == b.as_int();
    // float vs int comparison
    if (a.is_float() && b.is_float()) return a.as_float() == b.as_float();
    if (a.is_int() && b.is_float())   return double(a.as_int()) == b.as_float();
    if (a.is_float() && b.is_int())   return a.as_float() == double(b.as_int());
    // string equality: pointer eq catches interned pairs; hash fast-rejects others
    if (val_is_string(a) && val_is_string(b)) {
        ObjString& sa = as_str(a), &sb = as_str(b);
        if (!sa.hash) { sa.hash = 2166136261u; for (unsigned char c : sa.data) sa.hash = (sa.hash ^ c) * 16777619u; }
        if (!sb.hash) { sb.hash = 2166136261u; for (unsigned char c : sb.data) sb.hash = (sb.hash ^ c) * 16777619u; }
        if (sa.hash != sb.hash) return false;
        return sa.data == sb.data;
    }
    return false;
}


// ── String conversion ────────────────────────────────────────────────────────

std::string val_to_string(Value v)
{
    if (v.is_int())        return std::to_string(v.as_int());
    if (v.is_float()) {
        char buf[32];
        auto [p, ec] = std::to_chars(buf, buf + sizeof(buf), v.as_float(),
                                     std::chars_format::general);
        return std::string(buf, p);
    }
    if (v.is_true())       return "true";
    if (v.is_false())      return "false";
    if (v.is_none())       return "none";
    if (v.is_duration())   return std::to_string(v.as_duration()) + "ns";
    if (!v.is_ptr())       return "<unknown>";
    Obj* o = v.as_ptr();
    switch (o->kind) {
    case ObjKind::String:  return static_cast<ObjString*>(o)->data;
    case ObjKind::List: {
        auto* list = static_cast<ObjList*>(o);
        std::string s = "[";
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            if (i) s += ", ";
            s += val_to_string(list->items[i]);
        }
        return s + "]";
    }
    case ObjKind::Map:     return "<map>";
    case ObjKind::Tuple:   return "<tuple>";
    case ObjKind::Function:return "<fn " + static_cast<ObjFunction*>(o)->name + ">";
    case ObjKind::Closure: return "<fn " + static_cast<ObjClosure*>(o)->fn->name + ">";
    case ObjKind::Native:  return "<native " + static_cast<ObjNative*>(o)->name + ">";
    case ObjKind::Error:   return "<error: " + val_to_string(static_cast<ObjError*>(o)->message) + ">";
    default:               return "<obj>";
    }
}

} // namespace syn
