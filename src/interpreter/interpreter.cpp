#include "interpreter/interpreter.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>

namespace Synapse {

static DataType stringToDataType(const std::string& typeName) {
    if (typeName == "int")   return DataType::INT;
    if (typeName == "float") return DataType::FLOAT;
    if (typeName == "str")   return DataType::STR;
    if (typeName == "bool")  return DataType::BOOL;
    if (typeName == "tuple") return DataType::TUPLE;
    if (typeName == "list")  return DataType::LIST;
    if (typeName == "map")   return DataType::MAP;
    if (typeName == "time")  return DataType::TIME;
    return DataType::NONE;
}

SynapseValue coerceToType(DataType type, const SynapseValue& val, int ln, int col) {
    if (type == DataType::NONE) return val;
    switch (type) {
        case DataType::INT:   return valueToInt(val);
        case DataType::FLOAT: return valueToDouble(val);
        case DataType::STR:   return makeString(valueToString(val));
        case DataType::BOOL:  return valueToBool(val);
        default: return val;
    }
}

Interpreter::Interpreter(std::shared_ptr<IPlatform> plt) : platform(std::move(plt)) {
    currentEnv = std::make_shared<Environment>();
    registerBuiltins();
}

Interpreter::~Interpreter() {}

void Interpreter::interpret(ProgramNode& program) {
    program.accept(*this);
}

SynapseValue Interpreter::eval(ASTNode& node) {
    node.accept(*this);
    return lastValue;
}

void Interpreter::visit(IntLiteralNode& n)   { lastValue = n.value; }
void Interpreter::visit(FloatLiteralNode& n) { lastValue = n.value; }
void Interpreter::visit(StringLiteralNode& n){ lastValue = makeString(n.value); }
void Interpreter::visit(BoolLiteralNode& n)  { lastValue = n.value; }
void Interpreter::visit(NullLiteralNode& n)  { lastValue = SynapseValue(); }
void Interpreter::visit(TimeLiteralNode& n)  { 
    SynapseValue v; v.type = ValueType::VAL_TIME; v.as.ms = n.toMs(); lastValue = v; 
}

void Interpreter::visit(IdentifierNode& n) {
    try {
        lastValue = currentEnv->get(n.name, n.line, n.column);
    } catch (const std::exception& e) {
        std::cerr << "Failed to get variable: " << n.name << " at " << n.line << ":" << n.column << std::endl;
        throw;
    }
}

SynapseValue Interpreter::applyBinaryOp(const std::string& op, const SynapseValue& l, const SynapseValue& r) {
    if (op == "+") {
        if ((l.type == ValueType::VAL_OBJ && l.as.obj->type == ObjType::STR) || 
            (r.type == ValueType::VAL_OBJ && r.as.obj->type == ObjType::STR))
            return makeString(valueToString(l) + valueToString(r));
        if (l.type == ValueType::VAL_INT && r.type == ValueType::VAL_INT) return l.as.i + r.as.i;
        return valueToDouble(l) + valueToDouble(r);
    }
    if (op == "-") {
        if (l.type == ValueType::VAL_INT && r.type == ValueType::VAL_INT) return l.as.i - r.as.i;
        return valueToDouble(l) - valueToDouble(r);
    }
    if (op == "*") {
        if (l.type == ValueType::VAL_INT && r.type == ValueType::VAL_INT) return l.as.i * r.as.i;
        return valueToDouble(l) * valueToDouble(r);
    }
    if (op == "/") return valueToDouble(l) / valueToDouble(r);
    if (op == "//") {
        long long den = valueToInt(r);
        if (den == 0) throw RuntimeError("Division by zero");
        return valueToInt(l) / den;
    }
    if (op == "%") {
        long long den = valueToInt(r);
        if (den == 0) throw RuntimeError("Division by zero");
        return valueToInt(l) % den;
    }
    if (op == "**") return std::pow(valueToDouble(l), valueToDouble(r));
    if (op == "==") return valuesAreEqual(l, r);
    if (op == "!=") return !valuesAreEqual(l, r);
    if (op == "===") {
        if (l.type != r.type) return false;
        return valuesAreEqual(l, r);
    }
    if (op == ">")  return valueToDouble(l) > valueToDouble(r);
    if (op == "<")  return valueToDouble(l) < valueToDouble(r);
    if (op == ">=") return valueToDouble(l) >= valueToDouble(r);
    if (op == "<=") return valueToDouble(l) <= valueToDouble(r);
    if (op == "AND") return valueToBool(l) && valueToBool(r);
    if (op == "OR")  return valueToBool(l) || valueToBool(r);
    return SynapseValue();
}

SynapseValue Interpreter::applyCompound(const std::string& op, const SynapseValue& l, const SynapseValue& r) {
    return applyBinaryOp(op, l, r);
}

void Interpreter::visit(BinaryExprNode& n) {
    SynapseValue l = eval(*n.left);
    SynapseValue r = eval(*n.right);
    lastValue = applyBinaryOp(n.op, l, r);
}

void Interpreter::visit(UnaryExprNode& n) {
    SynapseValue val = eval(*n.operand);
    if (n.op == "-") {
        if (val.type == ValueType::VAL_INT) lastValue = -val.as.i;
        else lastValue = -valueToDouble(val);
    } else if (n.op == "NOT") {
        lastValue = !valueToBool(val);
    }
}

void Interpreter::visit(BlockNode& n) {
    auto savedEnv = currentEnv;
    if (n.needsScope) currentEnv = std::make_shared<Environment>(currentEnv);
    for (auto& stmt : n.statements) {
        stmt->accept(*this);
        if (isReturning) break;
    }
    currentEnv = savedEnv;
}

void Interpreter::visit(ProgramNode& n) {
    for (auto& stmt : n.statements) {
        stmt->accept(*this);
        if (isReturning) break;
    }
}

void Interpreter::visit(VarDeclNode& n) {
    SynapseValue val = n.value ? eval(*n.value) : SynapseValue();
    currentEnv->define(n.name, val);
}

void Interpreter::visit(TypedVarDeclNode& n) {
    SynapseValue val = n.value ? eval(*n.value) : SynapseValue();
    DataType dt = stringToDataType(n.typeName);
    val = coerceToType(dt, val, n.line, n.column);
    currentEnv->setTyped(n.name, val, dt);
}

void Interpreter::visit(AssignNode& n) {
    SynapseValue val = eval(*n.value);
    currentEnv->assign(n.name, val);
}

void Interpreter::visit(CompoundAssignNode& n) {
    SynapseValue cur = currentEnv->get(n.name, n.line, n.column);
    SynapseValue rhs = eval(*n.value);
    currentEnv->assign(n.name, applyCompound(n.op, cur, rhs));
}

void Interpreter::visit(PrintNode& n) {
    SynapseValue val = eval(*n.value);
    if (n.newline) std::cout << valueToString(val) << std::endl;
    else std::cout << valueToString(val);
}

void Interpreter::visit(AskNode& n) {
    std::cout << n.prompt << " " << std::flush;
    std::string input;
    std::getline(std::cin, input);
    SynapseValue res;
    if (n.typeCast == "int") try { res = std::stoll(input); } catch(...) { res = 0LL; }
    else if (n.typeCast == "float") try { res = std::stod(input); } catch(...) { res = 0.0; }
    else if (n.typeCast == "bool") res = (input == "true" || input == "1");
    else res = makeString(input);
    currentEnv->define(n.varName, res);
}



void Interpreter::visit(IfNode& n) {
    if (valueToBool(eval(*n.condition))) {
        n.thenBlock->accept(*this);
    } else if (n.elseBlock) {
        n.elseBlock->accept(*this);
    }
}

void Interpreter::visit(WhileNode& n) {
    while (valueToBool(eval(*n.condition))) {
        n.body->accept(*this);
        if (isReturning) break;
    }
}

void Interpreter::visit(RepeatNode& n) {
    long long count = valueToInt(eval(*n.count));
    for (long long i = 0; i < count; ++i) {
        n.body->accept(*this);
        if (isReturning) break;
    }
}

void Interpreter::visit(ReturnNode& n) {
    returnValue = n.value ? eval(*n.value) : SynapseValue();
    isReturning = true;
}

void Interpreter::visit(FuncDeclNode& n) {
    SynapseFunction fn;
    fn.name = n.name; fn.params = n.params; fn.body = n.body.get(); fn.closure = currentEnv;
    functions[n.name] = fn;
}

void Interpreter::visit(FuncCallNode& n) {
    std::vector<SynapseValue> args;
    for (auto& arg : n.args) args.push_back(eval(*arg));
    if (builtins.count(n.name)) {
        lastValue = (*builtins[n.name])(args);
        return;
    }
    if (!functions.count(n.name)) throw RuntimeError("Undefined function: '" + n.name + "'", n.line, n.column);
    SynapseFunction& fn = functions[n.name];
    if (args.size() != fn.params.size()) throw RuntimeError("Function '" + n.name + "' expects " + std::to_string(fn.params.size()) + " arguments, got " + std::to_string(args.size()), n.line, n.column);
    auto localEnv = std::make_shared<Environment>(fn.closure);
    for (size_t i = 0; i < fn.params.size(); ++i) localEnv->define(fn.params[i], args[i]);
    auto savedEnv = currentEnv; currentEnv = localEnv;
    bool savedReturning = isReturning; isReturning = false;
    SynapseValue savedReturnValue = returnValue; returnValue = SynapseValue();
    fn.body->accept(*this);
    lastValue = returnValue;
    currentEnv = savedEnv; isReturning = savedReturning; returnValue = savedReturnValue;
}

void Interpreter::visit(TryCatchNode& n) {
    try {
        n.tryBlock->accept(*this);
    } catch (const RuntimeError& e) {
        auto localEnv = std::make_shared<Environment>(currentEnv);
        localEnv->define(n.errorVar, makeString(e.what()));
        auto savedEnv = currentEnv; currentEnv = localEnv;
        n.catchBlock->accept(*this);
        currentEnv = savedEnv;
    }
}

void Interpreter::visit(MouseMoveNode& n) {
    SynapseValue pt = eval(*n.pointExpr);
    if (platform) { auto coord = extractCoord(pt); platform->mouseMove(coord.first, coord.second); }
}

void Interpreter::visit(MouseClickNode& n) {
    SynapseValue pt = n.pointExpr ? eval(*n.pointExpr) : SynapseValue();
    if (platform) {
        if (pt.type != ValueType::VAL_NULL) { auto coord = extractCoord(pt); platform->mouseMove(coord.first, coord.second); }
        platform->mouseClick(n.button);
    }
}

void Interpreter::visit(KeyPressNode& n) { if (platform) platform->keyPress(n.key); }
void Interpreter::visit(KeyTypeNode& n) {
    SynapseValue text = eval(*n.textExpr);
    if (platform) platform->keyType(valueToString(text));
}

void Interpreter::visit(AppOpenNode& n) {
    SynapseValue name = eval(*n.nameExpr);
    if (platform) platform->openApp(valueToString(name));
}

void Interpreter::visit(AppListNode& n) {
    auto apps = platform ? platform->getAvailableApps() : std::vector<std::string>{};
    lastValue = makeList();
    auto listObj = static_cast<ObjList*>(lastValue.as.obj);
    for (const auto& a : apps) listObj->elements.push_back(makeString(a));
}

void Interpreter::visit(TupleLiteralNode& n) {
    std::vector<SynapseValue> elements;
    for (auto& el : n.elements) elements.push_back(eval(*el));
    lastValue = makeTuple(std::move(elements));
}

void Interpreter::visit(ListLiteralNode& n) {
    lastValue = makeList();
    auto listObj = static_cast<ObjList*>(lastValue.as.obj);
    for (auto& el : n.elements) listObj->elements.push_back(eval(*el));
}

void Interpreter::visit(MapLiteralNode& n) {
    lastValue = makeMap();
    auto mapObj = static_cast<ObjMap*>(lastValue.as.obj);
    for (auto& [k, v] : n.items) {
        SynapseValue key = eval(*k);
        SynapseValue val = eval(*v);
        mapObj->items[valueToString(key)] = val;
    }
}

void Interpreter::visit(IndexAccessNode& n) {
    SynapseValue obj = eval(*n.object);
    SynapseValue idx = eval(*n.index);
    if (obj.type == ValueType::VAL_OBJ) {
        if (obj.as.obj->type == ObjType::TUPLE) lastValue = static_cast<ObjTuple*>(obj.as.obj)->elements[valueToInt(idx)];
        else if (obj.as.obj->type == ObjType::LIST) lastValue = static_cast<ObjList*>(obj.as.obj)->elements[valueToInt(idx)];
        else if (obj.as.obj->type == ObjType::MAP) lastValue = static_cast<ObjMap*>(obj.as.obj)->items[valueToString(idx)];
        else if (obj.as.obj->type == ObjType::STR) lastValue = makeString(std::string(1, static_cast<ObjString*>(obj.as.obj)->chars[valueToInt(idx)]));
    }
}

void Interpreter::visit(ExpressionStmtNode& n) {
    eval(*n.expression);
}

void Interpreter::visit(WaitNode& n) {
    SynapseValue duration = eval(*n.duration);
    if (duration.type == ValueType::VAL_TIME) std::this_thread::sleep_for(std::chrono::milliseconds(duration.as.ms));
}

void Interpreter::registerBuiltins() {
    builtins["now"] = std::make_shared<BuiltinFunc>([](const std::vector<SynapseValue>&) -> SynapseValue {
        auto now = std::chrono::steady_clock::now();
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        return static_cast<double>(micros) / 1000.0;
    });

    builtins["size"] = std::make_shared<BuiltinFunc>([](const std::vector<SynapseValue>& args) -> SynapseValue {
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

    builtins["app_open"] = std::make_shared<BuiltinFunc>([this](const std::vector<SynapseValue>& args) -> SynapseValue {
        if (args.empty()) return false;
        if (platform) platform->openApp(valueToString(args[0]));
        return true;
    });

    builtins["app_list"] = std::make_shared<BuiltinFunc>([this](const std::vector<SynapseValue>&) -> SynapseValue {
        auto apps = platform ? platform->getAvailableApps() : std::vector<std::string>{};
        SynapseValue list = makeList();
        auto listObj = static_cast<ObjList*>(list.as.obj);
        for (const auto& a : apps) listObj->elements.push_back(makeString(a));
        return list;
    });

    builtins["get_mouse_pos"] = std::make_shared<BuiltinFunc>([this](const std::vector<SynapseValue>&) -> SynapseValue {
        auto pos = platform ? platform->getMousePosition() : IPlatform::Point{0, 0};
        std::vector<SynapseValue> el;
        el.push_back((long long)pos.x);
        el.push_back((long long)pos.y);
        return makeTuple(std::move(el));
    });

    builtins["assert"] = std::make_shared<BuiltinFunc>([](const std::vector<SynapseValue>& args) -> SynapseValue {
        if (!args.empty() && !valueToBool(args[0])) {
            throw RuntimeError(args.size() > 1 ? valueToString(args[1]) : "Assertion failed");
        }
        return true;
    });
}

} // namespace Synapse
