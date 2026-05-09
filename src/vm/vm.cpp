#include "vm/vm.h"
#include "vm/opcode.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <cstdio>

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
            if (v.as.obj->type == ObjType::STR) return (long long)static_cast<ObjString*>(v.as.obj)->chars.size();
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
}

void VM::setGlobals(const std::vector<std::string>& names) {
    for (auto& g : globals) decref(g);
    globals.assign(names.size(), SynapseValue());
    for (size_t i = 0; i < names.size(); ++i) {
        globalNameMap[names[i]] = static_cast<int>(i);
        if (builtins.count(names[i])) {
            globals[i] = builtins[names[i]];
            incref(globals[i]);
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
    InterpretResult res = run();
#ifdef SYNAPSE_PROFILER
    if (profilingEnabled) dumpProfile();
#endif
    return res;
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

#ifdef __GNUC__
    #define INTERPRETER_LOOP() \
        SYNAPSE_PROF_INC(); \
        goto *dispatch_table[*currentFrame->ip++]
    #define DISPATCH() \
        SYNAPSE_PROF_INC(); \
        goto *dispatch_table[*currentFrame->ip++]
    #define CASE(op) L_##op:
    static void* dispatch_table[] = {
        &&L_OP_CONSTANT, &&L_OP_CONSTANT_16, &&L_OP_NULL, &&L_OP_TRUE, &&L_OP_FALSE,
        &&L_OP_POP, &&L_OP_GET_LOCAL, &&L_OP_SET_LOCAL, &&L_OP_GET_GLOBAL, &&L_OP_SET_GLOBAL,
        &&L_OP_DEFINE_GLOBAL, &&L_OP_EQUAL, &&L_OP_STRICT_EQUAL, &&L_OP_GREATER, &&L_OP_LESS,
        &&L_OP_ADD, &&L_OP_SUBTRACT, &&L_OP_MULTIPLY, &&L_OP_DIVIDE, &&L_OP_MODULO,
        &&L_OP_INT_DIVIDE, &&L_OP_EXPONENT, &&L_OP_AND, &&L_OP_OR, &&L_OP_NOT, &&L_OP_NEGATE,
        &&L_OP_PRINT, &&L_OP_PRINTLN, &&L_OP_JUMP, &&L_OP_JUMP_IF_FALSE, &&L_OP_LOOP,
        &&L_OP_CALL, &&L_OP_RETURN, &&L_OP_TUPLE, &&L_OP_LIST, &&L_OP_MAP, &&L_OP_INDEX_GET,
        &&L_OP_MOUSE_MOVE, &&L_OP_MOUSE_CLICK, &&L_OP_KEY_PRESS, &&L_OP_KEY_TYPE,
        &&L_OP_WAIT, &&L_OP_ASK,
        &&L_OP_ADD_INT, &&L_OP_SUB_INT, &&L_OP_MUL_INT, &&L_OP_DIV_INT,
        &&L_OP_GET_LOCAL_0, &&L_OP_GET_LOCAL_1, &&L_OP_GET_LOCAL_2, &&L_OP_GET_LOCAL_3,
        &&L_OP_GET_LOCAL_4, &&L_OP_GET_LOCAL_5, &&L_OP_GET_LOCAL_6, &&L_OP_GET_LOCAL_7,
        &&L_OP_GET_LOCAL_8
    };
    INTERPRETER_LOOP();
#else
    #define CASE(op) case op:
    #define DISPATCH() break
    for (;;) {
        uint8_t instruction = *currentFrame->ip++;
        switch (instruction) {
#endif

            CASE(OP_CONSTANT) {
                index = (uint16_t)(*currentFrame->ip++ << 8);
                index |= *currentFrame->ip++;
                pushV(currentFrame->chunk->constants[index]);
                DISPATCH();
            }
            CASE(OP_CONSTANT_16) {
                index = (uint16_t)(*currentFrame->ip++ << 8);
                index |= *currentFrame->ip++;
                pushV(currentFrame->chunk->constants[index]);
                DISPATCH();
            }
            CASE(OP_NULL)      pushV(SynapseValue()); DISPATCH();
            CASE(OP_TRUE)      pushV(true); DISPATCH();
            CASE(OP_FALSE)     pushV(false); DISPATCH();
            CASE(OP_POP)       decref(popV()); DISPATCH();
            
            CASE(OP_GET_LOCAL) {
                slot = *currentFrame->ip++;
                // Specialize!
                if (slot <= 8) {
                    currentFrame->ip[-2] = (uint8_t)(OP_GET_LOCAL_0 + slot);
                    currentFrame->ip[-1] = 0x00; // NOP or padding? 
                    // Actually, OP_GET_LOCAL takes 1 byte operand.
                    // If we replace it with OP_GET_LOCAL_0, we have an extra byte.
                    // We can't easily remove it without shifting everything.
                    // Let's just use it as it is for now, maybe don't specialize GET_LOCAL yet if it's tricky.
                    // Actually, we can keep the operand byte but ignore it in OP_GET_LOCAL_N.
                }
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
                DISPATCH();
            }

            CASE(OP_ADD) {
                SynapseValue& vB = stackTop[-1];
                SynapseValue& vA = stackTop[-2];
                if (vA.type == ValueType::VAL_INT && vB.type == ValueType::VAL_INT) {
                    // Quicken!
                    currentFrame->ip[-1] = OP_ADD_INT;
                    vA.as.i += vB.as.i;
                    stackTop--;
                } else if (vA.type == ValueType::VAL_OBJ && vA.as.obj->type == ObjType::STR) {
                    resVal = makeString(valueToString(vA) + valueToString(vB));
                    decref(vA); decref(vB);
                    stackTop[-2] = resVal;
                    stackTop--;
                } else if (vB.type == ValueType::VAL_OBJ && vB.as.obj->type == ObjType::STR) {
                    resVal = makeString(valueToString(vA) + valueToString(vB));
                    decref(vA); decref(vB);
                    stackTop[-2] = resVal;
                    stackTop--;
                } else {
                    double da = valueToDouble(vA);
                    double db = valueToDouble(vB);
                    decref(vA); decref(vB);
                    stackTop[-2] = SynapseValue(da + db);
                    stackTop--;
                }
                DISPATCH();
            }
            CASE(OP_SUBTRACT) {
                SynapseValue& vB = stackTop[-1];
                SynapseValue& vA = stackTop[-2];
                if (vA.type == ValueType::VAL_INT && vB.type == ValueType::VAL_INT) {
                    currentFrame->ip[-1] = OP_SUB_INT;
                    vA.as.i -= vB.as.i;
                    stackTop--;
                } else {
                    double da = valueToDouble(vA);
                    double db = valueToDouble(vB);
                    decref(vA); decref(vB);
                    stackTop[-2] = SynapseValue(da - db);
                    stackTop--;
                }
                DISPATCH();
            }
            CASE(OP_MULTIPLY) {
                SynapseValue& vB = stackTop[-1];
                SynapseValue& vA = stackTop[-2];
                if (vA.type == ValueType::VAL_INT && vB.type == ValueType::VAL_INT) {
                    currentFrame->ip[-1] = OP_MUL_INT;
                    vA.as.i *= vB.as.i;
                    stackTop--;
                } else {
                    double da = valueToDouble(vA);
                    double db = valueToDouble(vB);
                    decref(vA); decref(vB);
                    stackTop[-2] = SynapseValue(da * db);
                    stackTop--;
                }
                DISPATCH();
            }
            CASE(OP_DIVIDE) {
                b = popV(); a = popV();
                pushV(valueToDouble(a) / valueToDouble(b));
                decref(a); decref(b);
                DISPATCH();
            }
            CASE(OP_INT_DIVIDE) {
                b = popV(); a = popV();
                long long den = valueToInt(b);
                if (den == 0) return InterpretResult::RUNTIME_ERROR;
                pushV(valueToInt(a) / den);
                decref(a); decref(b);
                DISPATCH();
            }
            CASE(OP_MODULO) {
                b = popV(); a = popV();
                long long den = valueToInt(b);
                if (den == 0) return InterpretResult::RUNTIME_ERROR;
                pushV(valueToInt(a) % den);
                decref(a); decref(b);
                DISPATCH();
            }
            CASE(OP_EXPONENT) {
                b = popV(); a = popV();
                pushV(std::pow(valueToDouble(a), valueToDouble(b)));
                decref(a); decref(b);
                DISPATCH();
            }
            CASE(OP_AND) {
                b = popV(); a = popV();
                pushV(valueToBool(a) && valueToBool(b));
                decref(a); decref(b);
                DISPATCH();
            }
            CASE(OP_OR) {
                b = popV(); a = popV();
                pushV(valueToBool(a) || valueToBool(b));
                decref(a); decref(b);
                DISPATCH();
            }
            CASE(OP_EQUAL) {
                b = popV(); a = popV();
                pushV(valuesAreEqual(a, b));
                decref(a); decref(b);
                DISPATCH();
            }
            CASE(OP_STRICT_EQUAL) {
                b = popV(); a = popV();
                if (a.type != b.type) pushV(false);
                else pushV(valuesAreEqual(a, b));
                decref(a); decref(b);
                DISPATCH();
            }
            CASE(OP_GREATER) {
                SynapseValue& vB = stackTop[-1];
                SynapseValue& vA = stackTop[-2];
                bool res;
                if (vA.type == ValueType::VAL_INT && vB.type == ValueType::VAL_INT) res = vA.as.i > vB.as.i;
                else res = valueToDouble(vA) > valueToDouble(vB);
                decref(vA); decref(vB);
                stackTop[-2] = SynapseValue(res);
                stackTop--;
                DISPATCH();
            }
            CASE(OP_LESS) {
                SynapseValue& vB = stackTop[-1];
                SynapseValue& vA = stackTop[-2];
                bool res;
                if (vA.type == ValueType::VAL_INT && vB.type == ValueType::VAL_INT) res = vA.as.i < vB.as.i;
                else res = valueToDouble(vA) < valueToDouble(vB);
                decref(vA); decref(vB);
                stackTop[-2] = SynapseValue(res);
                stackTop--;
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
                resVal = popV(); a = popV();
                if (a.type == ValueType::VAL_OBJ) {
                    if (a.as.obj->type == ObjType::TUPLE) pushV(static_cast<ObjTuple*>(a.as.obj)->elements[valueToInt(resVal)]);
                    else if (a.as.obj->type == ObjType::LIST) pushV(static_cast<ObjList*>(a.as.obj)->elements[valueToInt(resVal)]);
                    else if (a.as.obj->type == ObjType::MAP) pushV(static_cast<ObjMap*>(a.as.obj)->items[valueToString(resVal)]);
                    else if (a.as.obj->type == ObjType::STR) {
                        resVal = makeString(std::string(1, static_cast<ObjString*>(a.as.obj)->chars[valueToInt(resVal)]));
                        pushV(resVal); decref(resVal);
                    }
                } else return InterpretResult::RUNTIME_ERROR;
                decref(a); decref(resVal);
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
                        if (argCount != function->arity) return InterpretResult::RUNTIME_ERROR;
                        if (frameCount >= MAX_FRAMES) return InterpretResult::RUNTIME_ERROR;
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
                    } else return InterpretResult::RUNTIME_ERROR;
                } else return InterpretResult::RUNTIME_ERROR;
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

            CASE(OP_ADD_INT) {
                stackTop[-2].as.i += stackTop[-1].as.i;
                stackTop--;
                DISPATCH();
            }
            CASE(OP_SUB_INT) {
                stackTop[-2].as.i -= stackTop[-1].as.i;
                stackTop--;
                DISPATCH();
            }
            CASE(OP_MUL_INT) {
                stackTop[-2].as.i *= stackTop[-1].as.i;
                stackTop--;
                DISPATCH();
            }
            CASE(OP_DIV_INT) {
                if (stackTop[-1].as.i == 0) return InterpretResult::RUNTIME_ERROR;
                stackTop[-2].as.i /= stackTop[-1].as.i;
                stackTop--;
                DISPATCH();
            }
            CASE(OP_GET_LOCAL_0) pushV(stackBase[currentFrame->frameStart + 0]); currentFrame->ip++; DISPATCH();
            CASE(OP_GET_LOCAL_1) pushV(stackBase[currentFrame->frameStart + 1]); currentFrame->ip++; DISPATCH();
            CASE(OP_GET_LOCAL_2) pushV(stackBase[currentFrame->frameStart + 2]); currentFrame->ip++; DISPATCH();
            CASE(OP_GET_LOCAL_3) pushV(stackBase[currentFrame->frameStart + 3]); currentFrame->ip++; DISPATCH();
            CASE(OP_GET_LOCAL_4) pushV(stackBase[currentFrame->frameStart + 4]); currentFrame->ip++; DISPATCH();
            CASE(OP_GET_LOCAL_5) pushV(stackBase[currentFrame->frameStart + 5]); currentFrame->ip++; DISPATCH();
            CASE(OP_GET_LOCAL_6) pushV(stackBase[currentFrame->frameStart + 6]); currentFrame->ip++; DISPATCH();
            CASE(OP_GET_LOCAL_7) pushV(stackBase[currentFrame->frameStart + 7]); currentFrame->ip++; DISPATCH();
            CASE(OP_GET_LOCAL_8) pushV(stackBase[currentFrame->frameStart + 8]); currentFrame->ip++; DISPATCH();
#ifndef __GNUC__
            default: return InterpretResult::RUNTIME_ERROR;
        }
    }
#endif
    return InterpretResult::RUNTIME_ERROR;
}

#ifdef SYNAPSE_PROFILER
void VM::dumpProfile() {
    std::cout << "\n=== Opcode Profile ===\n";
    for (int i = 0; i < 256; ++i) {
        if (opcodeCounts[i] > 0) {
            std::cout << "OpCode " << i << ": " << opcodeCounts[i] << " calls\n";
        }
    }
    std::cout << "======================\n";
}
#endif

} // namespace Synapse
