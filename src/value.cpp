#include "value.h"
#include <sstream>
#include <cmath>
#include <iomanip>

namespace Synapse {



ObjTuple::~ObjTuple() { for (auto& e : elements) decref(e); }
ObjList::~ObjList()   { for (auto& e : elements) decref(e); }
ObjMap::~ObjMap()     { for (auto& pair : items) decref(pair.second); }
ObjFunction::~ObjFunction() { for (auto& c : constants) decref(c); }

SynapseValue makeString(std::string s) {
    auto obj = new ObjString(std::move(s));
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
                case ObjType::STR:   return static_cast<ObjString*>(v.as.obj)->chars;
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
    if (a.type != b.type) return false;
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
            return sA->chars == sB->chars;
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
