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

// ── ObjMap ────────────────────────────────────────────────────────────────────

void ObjMap::build_index()
{
    str_idx = new std::unordered_map<std::string, size_t>();
    int_idx = new std::unordered_map<int64_t, size_t>();
    for (size_t i = 0; i < pairs.size(); ++i) {
        Value k = pairs[i].first;
        if (val_is_string(k)) (*str_idx)[as_str(k).data] = i;
        else if (k.is_int())  (*int_idx)[k.as_int()]     = i;
    }
}

Value ObjMap::get(Value key) const
{
    if (str_idx && val_is_string(key)) {
        auto it = str_idx->find(as_str(key).data);
        return it == str_idx->end() ? Value::none_val() : pairs[it->second].second;
    }
    if (int_idx && key.is_int()) {
        auto it = int_idx->find(key.as_int());
        return it == int_idx->end() ? Value::none_val() : pairs[it->second].second;
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
        if (pairs.size() == 1) { /* just started */ }
        return;
    }
    if (int_idx && key.is_int()) {
        int64_t ki = key.as_int();
        auto it = int_idx->find(ki);
        if (it != int_idx->end()) { pairs[it->second].second = val; return; }
        (*int_idx)[ki] = pairs.size();
        pairs.push_back({key, val});
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
