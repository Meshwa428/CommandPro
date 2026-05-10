#pragma once
#include <unordered_map>
#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <functional>
#include <iostream>
#include <cstdint>
#include <cstring>

namespace Synapse {
struct ASTNode;

static inline uint64_t computeStringHash(const char* str, size_t len) {
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint8_t>(str[i]);
        hash *= 1099511628211ULL;
    }
    return hash == 0 ? 1 : hash;
}

// ── String Telemetry ─────────────────────────────────────────────────────
struct StringTelemetry {
    std::atomic<long long> totalAllocations{0};
    std::atomic<long long> ssoAllocations{0};
    std::atomic<long long> heapAllocations{0};
    std::atomic<long long> concatOperations{0};
    std::atomic<long long> stringEqualityChecks{0};
    std::atomic<long long> hashComparisons{0};

    double averageLength() const {
        long long alloc = totalAllocations.load();
        return alloc > 0 ? (double)totalLength.load() / alloc : 0.0;
    }
    std::atomic<long long> totalLength{0};
};

StringTelemetry& getStringTelemetry();

// ── SSO Configuration ─────────────────────────────────────────────────────
static constexpr size_t SSO_MAX_SIZE = 15;

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
    INTERP_FUNC,
    NATIVE
};

// ── Base Object ──────────────────────────────────────────────────────────
struct Obj {
    ObjType type;
    int refCount = 0;
#ifdef SYNAPSE_PROFILER
    static std::atomic<long long> totalAllocations;
#endif
    virtual ~Obj();
    explicit Obj(ObjType t);
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
inline void incref(Obj* o) { if (o && o->refCount != -1) o->refCount++; }
inline void decref(Obj* o) { if (o && o->refCount != -1) { o->refCount--; if (o->refCount <= 0) delete o; } }

inline void incref(SynapseValue v) {
    if (v.type == ValueType::VAL_OBJ) incref(v.as.obj);
}

inline void decref(SynapseValue v) {
    if (v.type == ValueType::VAL_OBJ) decref(v.as.obj);
}

// ── StringBuilder for efficient concatenation ────────────────────────────
struct StringBuilder {
    char* buffer;
    size_t length;
    size_t capacity;

    explicit StringBuilder(size_t initialCapacity = 64);
    ~StringBuilder();

    void append(const char* str, size_t len);
    void reserve(size_t newCapacity);
    void clear();

    std::string build() { return std::string(buffer, length); }
    std::string_view view() const { return std::string_view(buffer, length); }

    StringBuilder(const StringBuilder&) = delete;
    StringBuilder& operator=(const StringBuilder&) = delete;
};

// ── Specialized Objects ──────────────────────────────────────────────────

struct ObjString : public Obj {
    size_t length;
    uint64_t hash;
    bool isInterned = false;
    bool isSmall = false;
    union {
        char small[SSO_MAX_SIZE];
        struct {
            char* chars;
            size_t capacity;
        };
    };

    explicit ObjString(const char* cstr);
    explicit ObjString(std::string_view sv);
    explicit ObjString(const std::string& s);
    explicit ObjString(char* buffer, size_t length); // Takes ownership
    ~ObjString();

    const char* c_str() const { return isSmall ? small : chars; }

    void ensureCapacity(size_t needed);
    void append(const char* str, size_t len);

    inline bool stringEquals(const ObjString* other) const {
        if (this == other) return true;
        if (isInterned && other->isInterned) return false;
        if (length != other->length) return false;
        if (hash != 0 && other->hash != 0 && hash != other->hash) return false;
        return std::memcmp(c_str(), other->c_str(), length) == 0;
    }

#ifdef SYNAPSE_PROFILER
    static StringTelemetry telemetry;
#endif
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

struct ObjInterpFunction : public Obj {
    std::string name;
    std::vector<std::string> params;
    ASTNode* body;
    std::shared_ptr<void> closure; // Use void* to avoid circular dep with Environment
    explicit ObjInterpFunction() : Obj(ObjType::INTERP_FUNC), body(nullptr) {}
};

struct ObjFunction : public Obj {
    std::string name;
    int arity = 0;
    int maxSlots = 0;
    std::vector<uint8_t> code;
    std::vector<int> lines;
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
SynapseValue makeString(std::string_view sv);
SynapseValue makeString(const char* cstr);
SynapseValue takeString(char* buffer, size_t length);
SynapseValue makeTuple(std::vector<SynapseValue> elements);
SynapseValue makeList(std::vector<SynapseValue> elements = {});
SynapseValue makeMap();

} // namespace Synapse
