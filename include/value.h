#pragma once
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <functional>
#include <iostream>
#include <cstdint>

namespace Synapse {

// ── Value Types ──────────────────────────────────────────────────────────
enum class ValueType {
    VAL_INT,
    VAL_FLOAT,
    VAL_BOOL,
    VAL_NULL,
    VAL_TIME,
    VAL_OBJ
};

// ── Object Types ─────────────────────────────────────────────────────────
enum class ObjType {
    STR,
    TUPLE,
    LIST,
    MAP,
    FUNC,
    NATIVE
};

// ── Base Object ──────────────────────────────────────────────────────────
struct Obj {
    ObjType type;
    int refCount = 0;
    virtual ~Obj() = default;
    explicit Obj(ObjType t) : type(t), refCount(0) {}
};

// ── Value Structure (16 bytes) ───────────────────────────────────────────
struct SynapseValue {
    ValueType type;
    union {
        long long i;
        double f;
        bool b;
        long long ms;
        Obj* obj;
    } as;

    SynapseValue() : type(ValueType::VAL_NULL) { as.i = 0; }
    SynapseValue(long long v) : type(ValueType::VAL_INT) { as.i = v; }
    SynapseValue(int v) : type(ValueType::VAL_INT) { as.i = (long long)v; }
    SynapseValue(double v) : type(ValueType::VAL_FLOAT) { as.f = v; }
    SynapseValue(bool v) : type(ValueType::VAL_BOOL) { as.b = v; }
    SynapseValue(Obj* o) : type(ValueType::VAL_OBJ) { as.obj = o; }

    inline bool operator==(const SynapseValue& o) const {
        if (type != o.type) return false;
        switch (type) {
            case ValueType::VAL_INT:   return as.i == o.as.i;
            case ValueType::VAL_FLOAT: return as.f == o.as.f;
            case ValueType::VAL_BOOL:  return as.b == o.as.b;
            case ValueType::VAL_NULL:  return true;
            case ValueType::VAL_TIME:  return as.ms == o.as.ms;
            case ValueType::VAL_OBJ:   return as.obj == o.as.obj;
            default: return false;
        }
    }
    size_t index() const { return static_cast<size_t>(type); }
};

// ── Memory Management ────────────────────────────────────────────────────
inline void incref(Obj* o) { if (o) o->refCount++; }
inline void decref(Obj* o) { if (o) { o->refCount--; if (o->refCount <= 0) delete o; } }

inline void incref(SynapseValue v) {
    if (v.type == ValueType::VAL_OBJ) incref(v.as.obj);
}

inline void decref(SynapseValue v) {
    if (v.type == ValueType::VAL_OBJ) decref(v.as.obj);
}

// ── Specialized Objects ──────────────────────────────────────────────────

struct ObjString : public Obj {
    std::string chars;
    bool isInterned = false;
    explicit ObjString(std::string s) : Obj(ObjType::STR), chars(std::move(s)) {}
};

struct ObjTuple : public Obj {
    std::vector<SynapseValue> elements;
    explicit ObjTuple(std::vector<SynapseValue> e) : Obj(ObjType::TUPLE), elements(std::move(e)) {}
    ~ObjTuple();
};

struct ObjList : public Obj {
    std::vector<SynapseValue> elements;
    explicit ObjList() : Obj(ObjType::LIST) {}
    ~ObjList();
};

struct ObjMap : public Obj {
    std::unordered_map<std::string, SynapseValue> items;
    explicit ObjMap() : Obj(ObjType::MAP) {}
    ~ObjMap();
};

struct ObjFunction : public Obj {
    std::string name;
    int arity = 0;
    int maxSlots = 0;
    std::vector<uint8_t> code;
    std::vector<SynapseValue> constants;
    explicit ObjFunction() : Obj(ObjType::FUNC) {}
    ~ObjFunction();
};

struct ObjNative : public Obj {
    using FuncType = std::function<SynapseValue(const std::vector<SynapseValue>&)>;
    FuncType func;
    explicit ObjNative(FuncType f) : Obj(ObjType::NATIVE), func(std::move(f)) {}
    SynapseValue operator()(const std::vector<SynapseValue>& args) const { return func(args); }
};

class BuiltinFunc {
    ObjNative* obj;
public:
    explicit BuiltinFunc(ObjNative::FuncType f) : obj(new ObjNative(std::move(f))) { incref(obj); }
    ~BuiltinFunc() { decref(obj); }
    SynapseValue operator()(const std::vector<SynapseValue>& args) const { return (*obj)(args); }
};

// ── Primitive structs for compat ─────────────────────────────────────────
struct SynapseTime {
    long long ms = 0;
};
struct SynapseNull {};

// ── Converters & Helpers ───────────────────────────────────────────────────
enum class DataType {
    NONE, INT, FLOAT, STR, BOOL, TUPLE, LIST, MAP, TIME
};

std::string valueToString(const SynapseValue& v);
bool        valueToBool(const SynapseValue& v);
double      valueToDouble(const SynapseValue& v);
long long   valueToInt(const SynapseValue& v);
bool        valuesAreEqual(const SynapseValue& a, const SynapseValue& b);

std::pair<int, int> extractCoord(const SynapseValue& v, int line = 0, int col = 0);

// ── Allocation Helpers ───────────────────────────────────────────────────
SynapseValue makeString(std::string s);
SynapseValue makeTuple(std::vector<SynapseValue> elements);
SynapseValue makeList(std::vector<SynapseValue> elements = {});
SynapseValue makeMap();

} // namespace Synapse
