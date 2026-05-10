#include "value.h"
#include <atomic>
#include <unordered_map>
#include <cmath>
#include <iomanip>
#include <cstring>

namespace Synapse {

#ifdef SYNAPSE_PROFILER
std::atomic<long long> Obj::totalAllocations{0};
StringTelemetry ObjString::telemetry{};
#endif

StringTelemetry& getStringTelemetry() {
#ifdef SYNAPSE_PROFILER
    return ObjString::telemetry;
#else
    static StringTelemetry dummy;
    return dummy;
#endif
}

Obj::Obj(ObjType t) : type(t), refCount(0) {
#ifdef SYNAPSE_PROFILER
    totalAllocations++;
#endif
}
Obj::~Obj() = default;

ObjTuple::~ObjTuple() { for (auto& e : elements) decref(e); }
ObjList::~ObjList()   { for (auto& e : elements) decref(e); }
ObjMap::~ObjMap()     { for (auto& pair : items) decref(pair.second); }
ObjFunction::~ObjFunction() { for (auto& c : constants) decref(c); }

void ObjString::ensureCapacity(size_t needed) {
    if (isSmall) {
        if (needed <= SSO_MAX_SIZE) return;
        char* newChars = new char[needed + 1];
        std::memcpy(newChars, small, length + 1);
        chars = newChars;
        capacity = needed + 1;
        isSmall = false;
    } else if (needed + 1 > capacity) {
        size_t newCap = std::max(needed + 1, capacity * 2);
        char* newChars = new char[newCap];
        std::memcpy(newChars, chars, length + 1);
        delete[] chars;
        chars = newChars;
        capacity = newCap;
    }
}

void ObjString::append(const char* str, size_t len) {
    ensureCapacity(length + len);
    if (isSmall) {
        std::memcpy(small + length, str, len);
        length += len;
        small[length] = '\0';
        hash = computeStringHash(small, length);
    } else {
        std::memcpy(chars + length, str, len);
        length += len;
        chars[length] = '\0';
        hash = computeStringHash(chars, length);
    }
}

StringBuilder::StringBuilder(size_t initialCapacity) : buffer(nullptr), length(0), capacity(0) {
    reserve(initialCapacity);
}

StringBuilder::~StringBuilder() {
    delete[] buffer;
}

void StringBuilder::reserve(size_t newCapacity) {
    if (newCapacity <= capacity) return;
    size_t newCap = std::max(newCapacity, capacity * 2);
    if (newCap < 64) newCap = 64;
    char* newBuffer = new char[newCap];
    if (buffer) {
        std::memcpy(newBuffer, buffer, length);
        delete[] buffer;
    }
    buffer = newBuffer;
    capacity = newCap;
}

void StringBuilder::clear() {
    length = 0;
}

void StringBuilder::append(const char* str, size_t len) {
    if (len == 0) return;
    reserve(length + len + 1);
    std::memcpy(buffer + length, str, len);
    length += len;
    buffer[length] = '\0';
}

ObjString::ObjString(const char* cstr) : Obj(ObjType::STR), length(std::strlen(cstr)), hash(0), isSmall(false) {
    if (length <= SSO_MAX_SIZE) {
        isSmall = true;
        std::memcpy(small, cstr, length);
        small[length] = '\0';
        hash = computeStringHash(small, length);
#ifdef SYNAPSE_PROFILER
        telemetry.ssoAllocations++;
        telemetry.totalAllocations++;
        telemetry.totalLength += length;
#endif
    } else {
        capacity = length + 1;
        chars = new char[capacity];
        std::memcpy(chars, cstr, length + 1);
        hash = computeStringHash(chars, length);
#ifdef SYNAPSE_PROFILER
        telemetry.heapAllocations++;
        telemetry.totalAllocations++;
        telemetry.totalLength += length;
#endif
    }
}

ObjString::ObjString(std::string_view sv) : Obj(ObjType::STR), length(sv.length()), hash(0), isSmall(false) {
    if (length <= SSO_MAX_SIZE) {
        isSmall = true;
        std::memcpy(small, sv.data(), length);
        small[length] = '\0';
        hash = computeStringHash(small, length);
#ifdef SYNAPSE_PROFILER
        telemetry.ssoAllocations++;
        telemetry.totalAllocations++;
        telemetry.totalLength += length;
#endif
    } else {
        capacity = length + 1;
        chars = new char[capacity];
        std::memcpy(chars, sv.data(), length);
        chars[length] = '\0';
        hash = computeStringHash(chars, length);
#ifdef SYNAPSE_PROFILER
        telemetry.heapAllocations++;
        telemetry.totalAllocations++;
        telemetry.totalLength += length;
#endif
    }
}

ObjString::ObjString(const std::string& s) : Obj(ObjType::STR), length(s.length()), hash(0), isSmall(false) {
    if (length <= SSO_MAX_SIZE) {
        isSmall = true;
        std::memcpy(small, s.data(), length);
        small[length] = '\0';
        hash = computeStringHash(small, length);
#ifdef SYNAPSE_PROFILER
        telemetry.ssoAllocations++;
        telemetry.totalAllocations++;
        telemetry.totalLength += length;
#endif
    } else {
        capacity = length + 1;
        chars = new char[capacity];
        std::memcpy(chars, s.data(), length + 1);
        hash = computeStringHash(chars, length);
#ifdef SYNAPSE_PROFILER
        telemetry.heapAllocations++;
        telemetry.totalAllocations++;
        telemetry.totalLength += length;
#endif
    }
}

ObjString::ObjString(char* buffer, size_t len) : Obj(ObjType::STR), length(len), hash(0), isSmall(false) {
    if (length <= SSO_MAX_SIZE) {
        isSmall = true;
        std::memcpy(small, buffer, length);
        small[length] = '\0';
        hash = computeStringHash(small, length);
        delete[] buffer;
#ifdef SYNAPSE_PROFILER
        telemetry.ssoAllocations++;
        telemetry.totalAllocations++;
        telemetry.totalLength += length;
#endif
    } else {
        chars = buffer;
        capacity = length + 1;
        hash = computeStringHash(chars, length);
#ifdef SYNAPSE_PROFILER
        telemetry.heapAllocations++;
        telemetry.totalAllocations++;
        telemetry.totalLength += length;
#endif
    }
}

ObjString::~ObjString() {
    if (!isSmall) {
        delete[] chars;
    }
}

SynapseValue makeString(std::string s) {
    auto obj = new ObjString(s);
    incref(obj);
    return SynapseValue(obj);
}

SynapseValue makeString(std::string_view sv) {
    auto obj = new ObjString(sv);
    incref(obj);
    return SynapseValue(obj);
}

SynapseValue makeString(const char* cstr) {
    auto obj = new ObjString(cstr);
    incref(obj);
    return SynapseValue(obj);
}

SynapseValue takeString(char* buffer, size_t length) {
    auto obj = new ObjString(buffer, length);
    incref(obj);
    return SynapseValue(obj);
}

SynapseValue makeTuple(std::vector<SynapseValue> elements) {
    for (auto& e : elements) incref(e);
    auto obj = new ObjTuple(std::move(elements));
    incref(obj);
    return SynapseValue(obj);
}

SynapseValue makeList(std::vector<SynapseValue> elements) {
    for (auto& e : elements) incref(e);
    auto obj = new ObjList();
    obj->elements = std::move(elements);
    incref(obj);
    return SynapseValue(obj);
}

SynapseValue makeMap() {
    auto obj = new ObjMap();
    incref(obj);
    return SynapseValue(obj);
}

std::string valueToString(const SynapseValue& v) {
    switch (v.type) {
        case ValueType::VAL_INT:   return std::to_string(v.as.i);
        case ValueType::VAL_FLOAT: {
            std::ostringstream ss;
            ss << std::setprecision(6) << v.as.f;
            return ss.str();
        }
        case ValueType::VAL_BOOL:  return v.as.b ? "true" : "false";
        case ValueType::VAL_NULL:  return "null";
        case ValueType::VAL_TIME:  return std::to_string(v.as.ms) + "ms";
        case ValueType::VAL_OBJ: {
            if (!v.as.obj) return "null-obj";
            switch (v.as.obj->type) {
                case ObjType::STR:   return std::string(static_cast<ObjString*>(v.as.obj)->c_str(), static_cast<ObjString*>(v.as.obj)->length);
                case ObjType::TUPLE: {
                    auto t = static_cast<ObjTuple*>(v.as.obj);
                    std::string s = "(";
                    for (size_t i = 0; i < t->elements.size(); ++i) {
                        s += valueToString(t->elements[i]);
                        if (i < t->elements.size() - 1) s += ", ";
                    }
                    return s + ")";
                }
                case ObjType::LIST: {
                    auto l = static_cast<ObjList*>(v.as.obj);
                    std::string s = "[";
                    for (size_t i = 0; i < l->elements.size(); ++i) {
                        s += valueToString(l->elements[i]);
                        if (i < l->elements.size() - 1) s += ", ";
                    }
                    return s + "]";
                }
                case ObjType::MAP:   return "map";
                case ObjType::FUNC:  return "function";
                case ObjType::NATIVE: return "native";
                default: return "obj";
            }
        }
        default: return "unknown";
    }
}

bool valueToBool(const SynapseValue& v) {
    switch (v.type) {
        case ValueType::VAL_BOOL:  return v.as.b;
        case ValueType::VAL_INT:   return v.as.i != 0;
        case ValueType::VAL_NULL:  return false;
        case ValueType::VAL_FLOAT: return v.as.f != 0.0;
        case ValueType::VAL_OBJ:   return true;
        default:                   return false;
    }
}

double valueToDouble(const SynapseValue& v) {
    switch (v.type) {
        case ValueType::VAL_FLOAT: return v.as.f;
        case ValueType::VAL_INT:   return static_cast<double>(v.as.i);
        case ValueType::VAL_BOOL:  return v.as.b ? 1.0 : 0.0;
        default:                   return 0.0;
    }
}

long long valueToInt(const SynapseValue& v) {
    switch (v.type) {
        case ValueType::VAL_INT:   return v.as.i;
        case ValueType::VAL_FLOAT: return static_cast<long long>(v.as.f);
        case ValueType::VAL_BOOL:  return v.as.b ? 1LL : 0LL;
        default:                   return 0LL;
    }
}

bool valuesAreEqual(const SynapseValue& a, const SynapseValue& b) {
    if (a.type != b.type) {
        if (a.type == ValueType::VAL_INT && b.type == ValueType::VAL_FLOAT) return (double)a.as.i == b.as.f;
        if (a.type == ValueType::VAL_FLOAT && b.type == ValueType::VAL_INT) return a.as.f == (double)b.as.i;
        return false;
    }
    if (a.type != ValueType::VAL_OBJ) return a == b;
    Obj* ao = a.as.obj; Obj* bo = b.as.obj;
    if (ao == bo) return true;
    if (!ao || !bo) return false;
    if (ao->type != bo->type) return false;
    switch (ao->type) {
        case ObjType::STR: {
            auto* sA = static_cast<ObjString*>(ao);
            auto* sB = static_cast<ObjString*>(bo);
            if (sA->isInterned && sB->isInterned) return sA == sB;
            return sA->stringEquals(sB);
        }
        case ObjType::TUPLE: {
            auto& ae = static_cast<ObjTuple*>(ao)->elements;
            auto& be = static_cast<ObjTuple*>(bo)->elements;
            if (ae.size() != be.size()) return false;
            for (size_t i = 0; i < ae.size(); ++i) if (!valuesAreEqual(ae[i], be[i])) return false;
            return true;
        }
        case ObjType::LIST: {
            auto& ae = static_cast<ObjList*>(ao)->elements;
            auto& be = static_cast<ObjList*>(bo)->elements;
            if (ae.size() != be.size()) return false;
            for (size_t i = 0; i < ae.size(); ++i) if (!valuesAreEqual(ae[i], be[i])) return false;
            return true;
        }
        default: return false;
    }
}

std::pair<int, int> extractCoord(const SynapseValue& v, int line, int col) {
    if (v.type != ValueType::VAL_OBJ) throw std::runtime_error("Coord must be an object");
    auto process = [&](auto size_fn, auto get_fn) -> std::pair<int, int> {
        if (size_fn() != 2) throw std::runtime_error("Coordinate must have 2 elements");
        return { (int)valueToInt(get_fn(0)), (int)valueToInt(get_fn(1)) };
    };
    if (v.as.obj->type == ObjType::TUPLE) {
        auto t = static_cast<ObjTuple*>(v.as.obj);
        return process([&]{ return t->elements.size(); }, [&](size_t i){ return t->elements[i]; });
    } else if (v.as.obj->type == ObjType::LIST) {
        auto l = static_cast<ObjList*>(v.as.obj);
        return process([&]{ return l->elements.size(); }, [&](size_t i){ return l->elements[i]; });
    }
    throw std::runtime_error("Invalid coordinate type");
}

} // namespace Synapse
