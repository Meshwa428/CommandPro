#include "vm/vm.h"
#include "vm/opcode.h"
#include "vm/intern.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Synapse {

VM::VM(std::shared_ptr<IPlatform> plt) : platform(std::move(plt)) {
    stack.resize(MAX_STACK);
    stackTop = stack.data();
    for (int i = 0; i < MAX_STACK; ++i) {
        stack[i].type = ValueType::VAL_NULL;
        stack[i].as.i = 0;
    }
#ifdef SYNAPSE_PROFILER
    for (int i = 0; i < 256; ++i) opcodeCounts[i] = 0;
    const char* prof = std::getenv("SYNAPSE_PROFILE");
    if (prof) profilingEnabled = true;
#endif
    registerBuiltins();
}

VM::~VM() {
    while (stackTop > stack.data()) {
        decref(*(--stackTop));
    }
    for (auto& g : globals) decref(g);
    for (auto& pair : builtins) decref(pair.second);
    clearConcatCache();
}

void VM::clearConcatCache() {
    for (int i = 0; i < CONCAT_CACHE_SIZE; ++i) {
        if (concatCache[i].a) decref(concatCache[i].a);
        if (concatCache[i].b) decref(concatCache[i].b);
        if (concatCache[i].res) decref(concatCache[i].res);
        concatCache[i] = {nullptr, nullptr, nullptr};
    }
}

void VM::registerBuiltins() {
    auto addBuiltin = [this](const std::string& name, std::function<SynapseValue(const std::vector<SynapseValue>&)> f) {
        auto obj = new ObjNative(std::move(f));
        incref(obj);
        builtins[name] = SynapseValue(obj);
    };

    addBuiltin("now", [](const std::vector<SynapseValue>&) -> SynapseValue {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
        return static_cast<double>(duration.count()) / 1000.0;
    });
    
    addBuiltin("size", [](const std::vector<SynapseValue>& args) -> SynapseValue {
        if (args.empty()) return 0LL;
        const SynapseValue& v = args[0];
        if (v.type == ValueType::VAL_OBJ) {
            if (v.as.obj->type == ObjType::TUPLE) return (long long)static_cast<ObjTuple*>(v.as.obj)->elements.size();
            if (v.as.obj->type == ObjType::LIST) return (long long)static_cast<ObjList*>(v.as.obj)->elements.size();
            if (v.as.obj->type == ObjType::MAP) return (long long)static_cast<ObjMap*>(v.as.obj)->items.size();
            if (v.as.obj->type == ObjType::STR) return (long long)static_cast<ObjString*>(v.as.obj)->length;
        }
        return 0LL;
    });

    addBuiltin("app_open", [this](const std::vector<SynapseValue>& args) -> SynapseValue {
        if (args.empty()) return false;
        if (platform) platform->openApp(valueToString(args[0]));
        return true;
    });

    addBuiltin("app_list", [this](const std::vector<SynapseValue>&) -> SynapseValue {
        auto apps = platform ? platform->getAvailableApps() : std::vector<std::string>{};
        std::vector<SynapseValue> el;
        for (const auto& a : apps) el.push_back(makeString(a));
        return makeList(std::move(el));
    });

    addBuiltin("get_mouse_pos", [this](const std::vector<SynapseValue>&) -> SynapseValue {
        auto pos = platform ? platform->getMousePosition() : IPlatform::Point{0, 0};
        std::vector<SynapseValue> el;
        el.push_back((long long)pos.x);
        el.push_back((long long)pos.y);
        return makeTuple(std::move(el));
    });

    addBuiltin("assert", [](const std::vector<SynapseValue>& args) -> SynapseValue {
        if (!args.empty() && !valueToBool(args[0])) {
            throw std::runtime_error(args.size() > 1 ? valueToString(args[1]) : "Assertion failed");
        }
        return true;
    });

    addBuiltin("string_telemetry", [](const std::vector<SynapseValue>&) -> SynapseValue {
        SynapseValue res = makeMap();
        auto* m = static_cast<ObjMap*>(res.as.obj);
#ifdef SYNAPSE_PROFILER
        auto& t = getStringTelemetry();
        m->items["total_allocations"] = (long long)t.totalAllocations.load();
        m->items["sso_allocations"] = (long long)t.ssoAllocations.load();
        m->items["heap_allocations"] = (long long)t.heapAllocations.load();
        m->items["total_length"] = (long long)t.totalLength.load();
#else
        m->items["profiling"] = false;
#endif
        return res;
    });
}

void VM::setGlobals(const std::vector<std::string>& names) {
    for (auto& g : globals) decref(g);
    globals.assign(names.size(), SynapseValue());
    globalsDefined.assign(names.size(), false);
    for (size_t i = 0; i < names.size(); ++i) {
        globalNameMap[names[i]] = static_cast<int>(i);
        if (builtins.count(names[i])) {
            globals[i] = builtins[names[i]];
            incref(globals[i]);
            globalsDefined[i] = true;
        }
    }
}

InterpretResult VM::interpret(ObjFunction* function) {
    frameCount = 0;
    stackTop = stack.data();
    CallFrame& frame = callStack[frameCount++];
    frame.chunk = function;
    frame.ip = function->code.data();
    frame.frameStart = 0;
    for (int i = 0; i < function->maxSlots; ++i) {
        stackTop->type = ValueType::VAL_NULL;
        stackTop->as.i = 0;
        stackTop++;
    }
    InterpretResult result = run();
#ifdef SYNAPSE_PROFILER
    if (profilingEnabled) dumpProfile();
#endif
    return result;
}

InterpretResult VM::run() {
    CallFrame* currentFrame = &callStack[frameCount - 1];
    SynapseValue a, b, v, retVal, fnVal, resVal, waitVal, askRes;
    uint16_t index = 0, count = 0, offset = 0;
    uint8_t slot = 0;
    int argCount = 0;
    SynapseValue* stackBase = stack.data();

#ifdef SYNAPSE_PROFILER
    #define SYNAPSE_PROF_INC() if (profilingEnabled) opcodeCounts[*currentFrame->ip]++
#else
    #define SYNAPSE_PROF_INC() 
#endif

    #define RUNTIME_ERROR(...) \
        do { \
            std::cerr << "[RuntimeError] " << currentFrame->chunk->name << " at line " << currentFrame->chunk->lines[currentFrame->ip - currentFrame->chunk->code.data() - 1] << ": "; \
            fprintf(stderr, __VA_ARGS__); \
            fprintf(stderr, "\n"); \
            return InterpretResult::RUNTIME_ERROR; \
        } while (0)

#ifdef __GNUC__
    #define INTERPRETER_LOOP() \
        SYNAPSE_PROF_INC(); \
        goto *dispatch_table[*currentFrame->ip++]
    #define DISPATCH() \
        SYNAPSE_PROF_INC(); \
        goto *dispatch_table[*currentFrame->ip++]
    #define CASE(op) L_##op:
    static void* dispatch_table[] = {
        &&L_OP_CONSTANT, &&L_OP_CONSTANT_8, &&L_OP_CONSTANT_16, &&L_OP_NULL, &&L_OP_TRUE, &&L_OP_FALSE,
        &&L_OP_POP, &&L_OP_DUP, &&L_OP_GET_LOCAL, &&L_OP_SET_LOCAL, &&L_OP_GET_GLOBAL, &&L_OP_SET_GLOBAL,
        &&L_OP_DEFINE_GLOBAL, &&L_OP_EQUAL, &&L_OP_STRICT_EQUAL, &&L_OP_GREATER, &&L_OP_LESS,
        &&L_OP_ADD, &&L_OP_SUBTRACT, &&L_OP_MULTIPLY, &&L_OP_DIVIDE, &&L_OP_MODULO,
        &&L_OP_INT_DIVIDE, &&L_OP_EXPONENT, &&L_OP_AND, &&L_OP_OR, &&L_OP_NOT, &&L_OP_NEGATE,
        &&L_OP_PRINT, &&L_OP_PRINTLN, &&L_OP_JUMP, &&L_OP_JUMP_IF_FALSE, &&L_OP_LOOP,
        &&L_OP_CALL, &&L_OP_RETURN, &&L_OP_TUPLE, &&L_OP_LIST, &&L_OP_MAP, &&L_OP_INDEX_GET, &&L_OP_INDEX_SET,
        &&L_OP_MOUSE_MOVE, &&L_OP_MOUSE_CLICK, &&L_OP_KEY_PRESS, &&L_OP_KEY_TYPE,
        &&L_OP_WAIT, &&L_OP_ASK,
        &&L_OP_STRING_ADD,
        &&L_OP_STRING_EQUAL
    };
    INTERPRETER_LOOP();
#endif

            CASE(OP_CONSTANT) {
                index = (uint16_t)(*currentFrame->ip++ << 8);
                index |= *currentFrame->ip++;
                pushV(currentFrame->chunk->constants[index]);
                DISPATCH();
            }
            CASE(OP_CONSTANT_8) {
                index = *currentFrame->ip++;
                pushV(currentFrame->chunk->constants[index]);
                DISPATCH();
            }
            CASE(OP_CONSTANT_16) {
                goto L_OP_CONSTANT;
            }
            CASE(OP_NULL)      pushV(SynapseValue()); DISPATCH();
            CASE(OP_TRUE)      pushV(true); DISPATCH();
            CASE(OP_FALSE)     pushV(false); DISPATCH();
            CASE(OP_POP)       decref(popV()); DISPATCH();
            CASE(OP_DUP)       pushV(peekV()); DISPATCH();
            CASE(OP_GET_LOCAL) {
                slot = *currentFrame->ip++;
                pushV(stackBase[currentFrame->frameStart + slot]);
                DISPATCH();
            }
            CASE(OP_SET_LOCAL) {
                slot = *currentFrame->ip++;
                v = peekV();
                SynapseValue& slotRef = stackBase[currentFrame->frameStart + slot];
                if (v.type == ValueType::VAL_OBJ || slotRef.type == ValueType::VAL_OBJ) {
                    incref(v);
                    decref(slotRef);
                }
                slotRef = v;
                DISPATCH();
            }
            CASE(OP_GET_GLOBAL) {
                index = (uint16_t)(*currentFrame->ip++ << 8);
                index |= *currentFrame->ip++;
                if (!globalsDefined[index]) {
                    RUNTIME_ERROR("Undefined variable");
                }
                pushV(globals[index]);
                DISPATCH();
            }
            CASE(OP_DEFINE_GLOBAL) {
                index = (uint16_t)(*currentFrame->ip++ << 8);
                index |= *currentFrame->ip++;
                v = peekV();
                SynapseValue& gRef = globals[index];
                if (v.type == ValueType::VAL_OBJ || gRef.type == ValueType::VAL_OBJ) {
                    incref(v);
                    decref(gRef);
                }
                gRef = v;
                globalsDefined[index] = true;
                DISPATCH();
            }
            CASE(OP_SET_GLOBAL) {
                index = (uint16_t)(*currentFrame->ip++ << 8);
                index |= *currentFrame->ip++;
                v = peekV();
                SynapseValue& gRef = globals[index];
                if (v.type == ValueType::VAL_OBJ || gRef.type == ValueType::VAL_OBJ) {
                    incref(v);
                    decref(gRef);
                }
                gRef = v;
                globalsDefined[index] = true;
                DISPATCH();
            }

            CASE(OP_ADD) {
                SynapseValue valB = stackTop[-1];
                SynapseValue valA = stackTop[-2];

                if (valA.type == ValueType::VAL_INT && valB.type == ValueType::VAL_INT) {
                    stackTop[-2] = SynapseValue(valA.as.i + valB.as.i);
                    stackTop--;
                } else if (valA.type == ValueType::VAL_OBJ && valA.as.obj->type == ObjType::STR &&
                           valB.type == ValueType::VAL_OBJ && valB.as.obj->type == ObjType::STR) {
                    auto* sA = static_cast<ObjString*>(valA.as.obj);
                    auto* sB = static_cast<ObjString*>(valB.as.obj);

                    size_t h = (reinterpret_cast<size_t>(sA)) ^ (reinterpret_cast<size_t>(sB) << 1);
                    int idx = static_cast<int>(h % CONCAT_CACHE_SIZE);
                    auto& entry = concatCache[idx];

                    if (entry.a == sA && entry.b == sB) {
                        ObjString* resObj = entry.res;
                        incref(resObj);
                        stackTop[-2] = SynapseValue(resObj);
                        stackTop--;
                        decref(valA); decref(valB);
                        DISPATCH();
                    }

                    if (sA->refCount == 1 && !sA->isInterned) {
                        sA->append(sB->c_str(), sB->length);
                        stackTop[-2] = valA;
                        stackTop--;
                        decref(valB);
                        DISPATCH();
                    }
                    
                    size_t newLen = sA->length + sB->length;
                    char* buf = new char[newLen + 1];
                    std::memcpy(buf, sA->c_str(), sA->length);
                    std::memcpy(buf + sA->length, sB->c_str(), sB->length);
                    buf[newLen] = '\0';
                    SynapseValue res = takeString(buf, newLen);
                    ObjString* resStr = static_cast<ObjString*>(res.as.obj);

                    if (entry.a) decref(entry.a);
                    if (entry.b) decref(entry.b);
                    if (entry.res) decref(entry.res);

                    entry.a = sA; incref(sA);
                    entry.b = sB; incref(sB);
                    entry.res = resStr; incref(resStr);

                    stackTop[-2] = res;
                    stackTop--;
                    decref(valA); decref(valB);
                } else if (valA.type == ValueType::VAL_OBJ && valA.as.obj->type == ObjType::STR) {
                    auto* sA = static_cast<ObjString*>(valA.as.obj);
                    std::string rhs = valueToString(valB);
                    size_t newLen = sA->length + rhs.size();
                    char* buf = new char[newLen + 1];
                    std::memcpy(buf, sA->c_str(), sA->length);
                    std::memcpy(buf + sA->length, rhs.data(), rhs.size());
                    buf[newLen] = '\0';
                    SynapseValue res = takeString(buf, newLen);
                    stackTop[-2] = res;
                    stackTop--;
                    decref(valA); decref(valB);
                } else if (valB.type == ValueType::VAL_OBJ && valB.as.obj->type == ObjType::STR) {
                    auto* sB = static_cast<ObjString*>(valB.as.obj);
                    std::string lhs = valueToString(valA);
                    size_t newLen = lhs.size() + sB->length;
                    char* buf = new char[newLen + 1];
                    std::memcpy(buf, lhs.data(), lhs.size());
                    std::memcpy(buf + lhs.size(), sB->c_str(), sB->length);
                    buf[newLen] = '\0';
                    SynapseValue res = takeString(buf, newLen);
                    stackTop[-2] = res;
                    stackTop--;
                    decref(valA); decref(valB);
                } else {
                    double da = valueToDouble(valA);
                    double db = valueToDouble(valB);
                    stackTop[-2] = SynapseValue(da + db);
                    stackTop--;
                    decref(valA); decref(valB);
                }
                DISPATCH();
            }

            CASE(OP_SUBTRACT) {
                SynapseValue valB = popV();
                SynapseValue valA = popV();
                if (valA.type == ValueType::VAL_INT && valB.type == ValueType::VAL_INT) {
                    pushV(SynapseValue(valA.as.i - valB.as.i));
                } else {
                    pushV(SynapseValue(valueToDouble(valA) - valueToDouble(valB)));
                }
                decref(valA); decref(valB);
                DISPATCH();
            }
            CASE(OP_MULTIPLY) {
                SynapseValue valB = popV();
                SynapseValue valA = popV();
                if (valA.type == ValueType::VAL_INT && valB.type == ValueType::VAL_INT) {
                    pushV(SynapseValue(valA.as.i * valB.as.i));
                } else {
                    pushV(SynapseValue(valueToDouble(valA) * valueToDouble(valB)));
                }
                decref(valA); decref(valB);
                DISPATCH();
            }

            CASE(OP_EQUAL) {
                SynapseValue valB = popV();
                SynapseValue valA = popV();
                bool result;
                if (valA.type != valB.type) {
                    if (valA.type == ValueType::VAL_INT && valB.type == ValueType::VAL_FLOAT) result = ((double)valA.as.i == valB.as.f);
                    else if (valA.type == ValueType::VAL_FLOAT && valB.type == ValueType::VAL_INT) result = (valA.as.f == (double)valB.as.i);
                    else result = false;
                } else if (valA.type == ValueType::VAL_OBJ) {
                    Obj* ao = valA.as.obj;
                    Obj* bo = valB.as.obj;
                    if (ao == bo) result = true;
                    else if (!ao || !bo || ao->type != bo->type) result = false;
                    else if (ao->type == ObjType::STR) result = static_cast<ObjString*>(ao)->stringEquals(static_cast<ObjString*>(bo));
                    else result = valuesAreEqual(valA, valB);
                } else {
                    result = (valA == valB);
                }
                pushV(SynapseValue(result));
                decref(valA); decref(valB);
                DISPATCH();
            }

            CASE(OP_STRING_ADD) {
                SynapseValue vB = popV();
                SynapseValue vA = popV();
                auto* sA = static_cast<ObjString*>(vA.as.obj);
                auto* sB = static_cast<ObjString*>(vB.as.obj);
                size_t newLen = sA->length + sB->length;
                char* buf = new char[newLen + 1];
                std::memcpy(buf, sA->c_str(), sA->length);
                std::memcpy(buf + sA->length, sB->c_str(), sB->length);
                buf[newLen] = '\0';
                SynapseValue res = takeString(buf, newLen);
                pushV(res);
                decref(res);
                decref(vA); decref(vB);
                DISPATCH();
            }

            CASE(OP_STRING_EQUAL) {
                SynapseValue vB = popV();
                SynapseValue vA = popV();
                bool res = static_cast<ObjString*>(vA.as.obj)->stringEquals(static_cast<ObjString*>(vB.as.obj));
                pushV(SynapseValue(res));
                decref(vA); decref(vB);
                DISPATCH();
            }

            CASE(OP_DIVIDE) {
                SynapseValue valB = popV();
                SynapseValue valA = popV();
                if (valueToDouble(valB) == 0.0) {
                    RUNTIME_ERROR("Division by zero");
                }
                pushV(valueToDouble(valA) / valueToDouble(valB));
                decref(valA); decref(valB);
                DISPATCH();
            }
            CASE(OP_INT_DIVIDE) {
                SynapseValue valB = popV();
                SynapseValue valA = popV();
                long long den = valueToInt(valB);
                if (den == 0) {
                    RUNTIME_ERROR("Division by zero");
                }
                pushV(valueToInt(valA) / den);
                decref(valA); decref(valB);
                DISPATCH();
            }
            CASE(OP_MODULO) {
                SynapseValue valB = popV();
                SynapseValue valA = popV();
                long long den = valueToInt(valB);
                if (den == 0) {
                    RUNTIME_ERROR("Division by zero");
                }
                pushV(valueToInt(valA) % den);
                decref(valA); decref(valB);
                DISPATCH();
            }
            CASE(OP_EXPONENT) {
                SynapseValue valB = popV();
                SynapseValue valA = popV();
                pushV(std::pow(valueToDouble(valA), valueToDouble(valB)));
                decref(valA); decref(valB);
                DISPATCH();
            }
            CASE(OP_AND) {
                SynapseValue valB = popV();
                SynapseValue valA = popV();
                pushV(valueToBool(valA) && valueToBool(valB));
                decref(valA); decref(valB);
                DISPATCH();
            }
            CASE(OP_OR) {
                SynapseValue valB = popV();
                SynapseValue valA = popV();
                pushV(valueToBool(valA) || valueToBool(valB));
                decref(valA); decref(valB);
                DISPATCH();
            }

            CASE(OP_STRICT_EQUAL) {
                SynapseValue valB = popV();
                SynapseValue valA = popV();
                if (valA.type != valB.type) pushV(false);
                else pushV(valuesAreEqual(valA, valB));
                decref(valA); decref(valB);
                DISPATCH();
            }
            CASE(OP_GREATER) {
                SynapseValue valB = popV();
                SynapseValue valA = popV();
                bool res;
                if (valA.type == ValueType::VAL_INT && valB.type == ValueType::VAL_INT) res = valA.as.i > valB.as.i;
                else res = valueToDouble(valA) > valueToDouble(valB);
                pushV(SynapseValue(res));
                decref(valA); decref(valB);
                DISPATCH();
            }
            CASE(OP_LESS) {
                SynapseValue valB = popV();
                SynapseValue valA = popV();
                bool res;
                if (valA.type == ValueType::VAL_INT && valB.type == ValueType::VAL_INT) res = valA.as.i < valB.as.i;
                else res = valueToDouble(valA) < valueToDouble(valB);
                pushV(SynapseValue(res));
                decref(valA); decref(valB);
                DISPATCH();
            }
            CASE(OP_NOT) { v = popV(); pushV(!valueToBool(v)); decref(v); DISPATCH(); }
            CASE(OP_NEGATE) {
                v = popV();
                if (v.type == ValueType::VAL_INT) pushV(-v.as.i);
                else pushV(-valueToDouble(v));
                decref(v);
                DISPATCH();
            }

            CASE(OP_PRINT) { v = popV(); std::cout << valueToString(v); decref(v); DISPATCH(); }
            CASE(OP_PRINTLN) { v = popV(); std::cout << valueToString(v) << std::endl; decref(v); DISPATCH(); }

            CASE(OP_TUPLE) {
                count = (uint16_t)(*currentFrame->ip++ << 8);
                count |= *currentFrame->ip++;
                {
                    std::vector<SynapseValue> elements;
                    elements.reserve(count);
                    for (int i = 0; i < count; ++i) elements.push_back(peekV(count - 1 - i));
                    resVal = makeTuple(elements);
                    for (int i = 0; i < count; ++i) decref(popV());
                    pushV(resVal);
                    decref(resVal);
                }
                DISPATCH();
            }
            CASE(OP_LIST) {
                count = (uint16_t)(*currentFrame->ip++ << 8);
                count |= *currentFrame->ip++;
                {
                    std::vector<SynapseValue> elements;
                    elements.reserve(count);
                    for (int i = 0; i < count; ++i) elements.push_back(peekV(count - 1 - i));
                    resVal = makeList(elements);
                    for (int i = 0; i < count; ++i) decref(popV());
                    pushV(resVal);
                    decref(resVal);
                }
                DISPATCH();
            }
            CASE(OP_MAP) {
                count = (uint16_t)(*currentFrame->ip++ << 8);
                count |= *currentFrame->ip++;
                {
                    resVal = makeMap();
                    auto mapObj = static_cast<ObjMap*>(resVal.as.obj);
                    for (int i = 0; i < count; ++i) {
                        SynapseValue val = popV();
                        SynapseValue key = popV();
                        mapObj->items[valueToString(key)] = val;
                        decref(key);
                    }
                    pushV(resVal);
                    decref(resVal);
                }
                DISPATCH();
            }
            CASE(OP_INDEX_GET) {
                SynapseValue idxVal = popV();
                SynapseValue coll = popV();
                if (coll.type == ValueType::VAL_OBJ) {
                    if (coll.as.obj->type == ObjType::STR) {
                        int i = (int)valueToInt(idxVal);
                        auto* strObj = static_cast<ObjString*>(coll.as.obj);
                        if (i >= 0 && i < (int)strObj->length) {
                            char ch[] = { strObj->c_str()[i], '\0' };
                            SynapseValue res = makeString(std::string_view(ch, 1));
                            pushV(res);
                            decref(res);
                        } else {
                            std::cerr << "[RuntimeError] String index out of bounds" << std::endl;
                            return InterpretResult::RUNTIME_ERROR;
                        }
                    } else if (coll.as.obj->type == ObjType::TUPLE) {
                        int i = (int)valueToInt(idxVal);
                        auto* t = static_cast<ObjTuple*>(coll.as.obj);
                        if (i >= 0 && i < (int)t->elements.size()) pushV(t->elements[i]);
                        else {
                            std::cerr << "[RuntimeError] Tuple index out of bounds" << std::endl;
                            return InterpretResult::RUNTIME_ERROR;
                        }
                    } else if (coll.as.obj->type == ObjType::LIST) {
                        int i = (int)valueToInt(idxVal);
                        auto* l = static_cast<ObjList*>(coll.as.obj);
                        if (i >= 0 && i < (int)l->elements.size()) pushV(l->elements[i]);
                        else {
                            std::cerr << "[RuntimeError] List index out of bounds" << std::endl;
                            return InterpretResult::RUNTIME_ERROR;
                        }
                    } else if (coll.as.obj->type == ObjType::MAP) {
                        auto* m = static_cast<ObjMap*>(coll.as.obj);
                        std::string key = valueToString(idxVal);
                        if (m->items.count(key)) pushV(m->items[key]);
                        else pushV(SynapseValue()); // Return null for missing map keys
                    } else { RUNTIME_ERROR("Indexing only supported on tuples, lists, and maps."); }
                } else { RUNTIME_ERROR("Indexing requires an object."); }
                
                decref(coll);
                decref(idxVal);
                DISPATCH();
            }
            CASE(OP_INDEX_SET) {
                SynapseValue val = popV();
                SynapseValue idxVal = popV();
                SynapseValue coll = popV();
                if (coll.type == ValueType::VAL_OBJ) {
                    if (coll.as.obj->type == ObjType::LIST) {
                        int i = (int)valueToInt(idxVal);
                        auto* l = static_cast<ObjList*>(coll.as.obj);
                        if (i >= 0 && i < (int)l->elements.size()) {
                            decref(l->elements[i]);
                            incref(val);
                            l->elements[i] = val;
                        } else {
                            std::cerr << "[RuntimeError] List index out of bounds" << std::endl;
                            return InterpretResult::RUNTIME_ERROR;
                        }
                    } else if (coll.as.obj->type == ObjType::MAP) {
                        auto* m = static_cast<ObjMap*>(coll.as.obj);
                        std::string key = valueToString(idxVal);
                        if (m->items.count(key)) decref(m->items[key]);
                        incref(val);
                        m->items[key] = val;
                        } else {
                            RUNTIME_ERROR("Invalid property access");
                        }
                    } else {
                        RUNTIME_ERROR("Properties only supported on objects");
                    }
                
                pushV(val); // Assignment expression value
                decref(coll);
                decref(idxVal);
                decref(val);
                DISPATCH();
            }

            CASE(OP_JUMP) {
                offset = (uint16_t)(*currentFrame->ip++ << 8);
                offset |= *currentFrame->ip++;
                currentFrame->ip += offset;
                DISPATCH();
            }
            CASE(OP_JUMP_IF_FALSE) {
                offset = (uint16_t)(*currentFrame->ip++ << 8);
                offset |= *currentFrame->ip++;
                if (!valueToBool(peekV())) currentFrame->ip += offset;
                DISPATCH();
            }
            CASE(OP_LOOP) {
                offset = (uint16_t)(*currentFrame->ip++ << 8);
                offset |= *currentFrame->ip++;
                currentFrame->ip -= offset;
                DISPATCH();
            }

            CASE(OP_WAIT) {
                waitVal = popV();
                if (waitVal.type == ValueType::VAL_TIME) std::this_thread::sleep_for(std::chrono::milliseconds(waitVal.as.ms));
                decref(waitVal);
                DISPATCH();
            }

            CASE(OP_ASK) {
                index = (uint16_t)(*currentFrame->ip++ << 8);
                index |= *currentFrame->ip++;
                offset = (uint16_t)(*currentFrame->ip++ << 8);
                offset |= *currentFrame->ip++;
                {
                    std::string promptStr = valueToString(currentFrame->chunk->constants[index]);
                    std::string typeCastStr = valueToString(currentFrame->chunk->constants[offset]);
                    std::cout << promptStr << " " << std::flush;
                    std::string inputStr;
                    std::getline(std::cin, inputStr);
                    if (typeCastStr == "INT") try { askRes = std::stoll(inputStr); } catch(...) { askRes = 0LL; }
                    else if (typeCastStr == "FLOAT") try { askRes = std::stod(inputStr); } catch(...) { askRes = 0.0; }
                    else if (typeCastStr == "BOOL") askRes = (inputStr == "true" || inputStr == "1");
                    else askRes = makeString(inputStr);
                    pushV(askRes);
                    decref(askRes);
                }
                DISPATCH();
            }

            CASE(OP_MOUSE_MOVE) {
                v = popV();
                if (platform) { auto coord = extractCoord(v); platform->mouseMove(coord.first, coord.second); }
                decref(v);
                DISPATCH();
            }
            CASE(OP_MOUSE_CLICK) {
                argCount = *currentFrame->ip++;
                v = popV();
                if (platform) {
                    if (v.type != ValueType::VAL_NULL) { auto coord = extractCoord(v); platform->mouseMove(coord.first, coord.second); }
                    platform->mouseClick(static_cast<MouseButton>(argCount));
                }
                decref(v);
                DISPATCH();
            }
            CASE(OP_KEY_PRESS) { v = popV(); if (platform) platform->keyPress(valueToString(v)); decref(v); DISPATCH(); }
            CASE(OP_KEY_TYPE) { v = popV(); if (platform) platform->keyType(valueToString(v)); decref(v); DISPATCH(); }

            CASE(OP_CALL) {
                argCount = *currentFrame->ip++;
                fnVal = peekV(argCount);
                if (fnVal.type == ValueType::VAL_OBJ) {
                    if (fnVal.as.obj->type == ObjType::FUNC) {
                        auto function = static_cast<ObjFunction*>(fnVal.as.obj);
                        if (argCount != function->arity) {
                            std::cerr << "[RuntimeError] Argument count mismatch" << std::endl;
                            return InterpretResult::RUNTIME_ERROR;
                        }
                        if (frameCount >= MAX_FRAMES) {
                            std::cerr << "[VM RuntimeError] Stack overflow" << std::endl;
                            return InterpretResult::RUNTIME_ERROR;
                        }
                        CallFrame& newFrame = callStack[frameCount++];
                        newFrame.chunk = function;
                        newFrame.ip = function->code.data();
                        newFrame.frameStart = (int)(stackTop - stackBase - argCount);
                        int resCount = function->maxSlots - argCount;
                        for (int i = 0; i < resCount; ++i) {
                            stackTop->type = ValueType::VAL_NULL;
                            stackTop->as.i = 0;
                            stackTop++;
                        }
                        currentFrame = &callStack[frameCount - 1];
                    } else if (fnVal.as.obj->type == ObjType::NATIVE) {
                        auto native = static_cast<ObjNative*>(fnVal.as.obj);
                        std::vector<SynapseValue> args;
                        args.reserve(argCount);
                        for (int i = 0; i < argCount; ++i) {
                            SynapseValue arg = peekV(argCount - 1 - i);
                            incref(arg);
                            args.push_back(arg);
                        }
                        SynapseValue result = native->func(args);
                        for (int i = 0; i < argCount + 1; ++i) decref(popV());
                        pushV(result);
                        for (auto& arg : args) decref(arg);
                        decref(result);
                    } else {
                        RUNTIME_ERROR("Can only call functions or natives.");
                    }
                } else {
                    RUNTIME_ERROR("Can only call objects.");
                }
                DISPATCH();
            }

            CASE(OP_RETURN) {
                retVal = popV();
                incref(retVal);
                int frameStartIdx = currentFrame->frameStart;
                frameCount--;
                int popLimit = frameStartIdx > 0 ? frameStartIdx - 1 : 0;
                while (stackTop > stackBase + popLimit) {
                    decref(popV());
                }
                pushV(retVal);
                decref(retVal);
                if (frameCount == 0) return InterpretResult::OK;
                currentFrame = &callStack[frameCount - 1];
                DISPATCH();
            }

#ifndef __GNUC__
            default: return InterpretResult::RUNTIME_ERROR;
        }
    }
#endif
    return InterpretResult::RUNTIME_ERROR;
}

#ifdef SYNAPSE_PROFILER
void VM::dumpProfile() {
    std::cout << "\n=== VM Runtime Telemetry ===\n";
    std::cout << "Total Allocations: " << Obj::totalAllocations.load() << "\n";
    std::cout << "Max Stack Depth:   " << maxStackDepth << "\n";
    std::cout << "----------------------------\n";
    std::cout << "Opcode Frequencies:\n";
    for (int i = 0; i < 256; ++i) {
        if (opcodeCounts[i] > 0) {
            std::cout << "  OpCode " << i << ": " << opcodeCounts[i] << "\n";
        }
    }
    std::cout << "============================\n";
}
#endif

} // namespace Synapse
