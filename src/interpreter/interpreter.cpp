#include "interpreter/interpreter.h"
#include <iostream>
#include <sstream>
#include <cmath>
#include <thread>
#include <chrono>

namespace Synapse {

// ──────────────────────────────────────────────────────────────────────────
//  Value helpers
// ──────────────────────────────────────────────────────────────────────────
std::string valueToString(const SynapseValue& v) {
    return std::visit([](auto&& a) -> std::string {
        using T = std::decay_t<decltype(a)>;
        if constexpr (std::is_same_v<T, long long>)    return std::to_string(a);
        if constexpr (std::is_same_v<T, double>)       {
            std::ostringstream ss;
            ss << a;
            return ss.str();
        }
        if constexpr (std::is_same_v<T, std::string>)  return a;
        if constexpr (std::is_same_v<T, bool>)         return a ? "true" : "false";
        if constexpr (std::is_same_v<T, SynapseTime>)  return std::to_string(a.ms) + "ms";
        if constexpr (std::is_same_v<T, SynapseNull>)  return "null";
        if constexpr (std::is_same_v<T, std::shared_ptr<SynapseTuple>>) {
            std::string res = "(";
            for (size_t i = 0; i < a->size(); ++i) {
                res += valueToString(a->at(i));
                if (i < a->size() - 1) res += ", ";
            }
            return res + ")";
        }
        if constexpr (std::is_same_v<T, std::shared_ptr<SynapseList>>) {
            std::string res = "[";
            for (size_t i = 0; i < a->elements.size(); ++i) {
                res += valueToString(a->elements[i]);
                if (i < a->elements.size() - 1) res += ", ";
            }
            return res + "]";
        }
        if constexpr (std::is_same_v<T, std::shared_ptr<SynapseMap>>) {
            std::string res = "{";
            size_t count = 0;
            for (auto const& [key, val] : a->items) {
                res += "\"" + key + "\": " + valueToString(val);
                if (++count < a->items.size()) res += ", ";
            }
            return res + "}";
        }
        return "?";
    }, v);
}

bool valueToBool(const SynapseValue& v) {
    return std::visit([](auto&& a) -> bool {
        using T = std::decay_t<decltype(a)>;
        if constexpr (std::is_same_v<T, long long>)   return a != 0;
        if constexpr (std::is_same_v<T, double>)      return a != 0.0;
        if constexpr (std::is_same_v<T, std::string>) return !a.empty();
        if constexpr (std::is_same_v<T, bool>)        return a;
        if constexpr (std::is_same_v<T, SynapseNull>) return false;
        return true;
    }, v);
}

double valueToDouble(const SynapseValue& v) {
    return std::visit([](auto&& a) -> double {
        using T = std::decay_t<decltype(a)>;
        if constexpr (std::is_same_v<T, long long>)   return static_cast<double>(a);
        if constexpr (std::is_same_v<T, double>)      return a;
        if constexpr (std::is_same_v<T, bool>)        return a ? 1.0 : 0.0;
        if constexpr (std::is_same_v<T, std::string>) {
            try { return std::stod(a); } catch (...) { return 0.0; }
        }
        return 0.0;
    }, v);
}

long long valueToInt(const SynapseValue& v) {
    return std::visit([](auto&& a) -> long long {
        using T = std::decay_t<decltype(a)>;
        if constexpr (std::is_same_v<T, long long>)   return a;
        if constexpr (std::is_same_v<T, double>)      return static_cast<long long>(a);
        if constexpr (std::is_same_v<T, bool>)        return a ? 1LL : 0LL;
        if constexpr (std::is_same_v<T, std::string>) {
            try { return std::stoll(a); } catch (...) { return 0LL; }
        }
        return 0LL;
    }, v);
}

SynapseValue coerceToType(const std::string& typeName, const SynapseValue& val, int ln, int col) {
    if (typeName.empty()) return val; // No constraint

    if (typeName == "int") {
        if (std::holds_alternative<long long>(val)) return val;
        if (std::holds_alternative<double>(val)) return static_cast<long long>(std::get<double>(val));
        throw RuntimeError("Type mismatch: Cannot convert to int", ln, col);
    }
    if (typeName == "float") {
        if (std::holds_alternative<double>(val)) return val;
        if (std::holds_alternative<long long>(val)) return static_cast<double>(std::get<long long>(val));
        throw RuntimeError("Type mismatch: Cannot convert to float", ln, col);
    }
    if (typeName == "str") {
        if (std::holds_alternative<std::string>(val)) return val;
        throw RuntimeError("Type mismatch: Expected str", ln, col);
    }
    if (typeName == "bool") {
        if (std::holds_alternative<bool>(val)) return val;
        throw RuntimeError("Type mismatch: Expected bool", ln, col);
    }
    if (typeName == "tuple") {
        if (std::holds_alternative<std::shared_ptr<SynapseTuple>>(val)) return val;
        throw RuntimeError("Type mismatch: Expected tuple", ln, col);
    }
    if (typeName == "list") {
        if (std::holds_alternative<std::shared_ptr<SynapseList>>(val)) return val;
        throw RuntimeError("Type mismatch: Expected list", ln, col);
    }
    if (typeName == "map") {
        if (std::holds_alternative<std::shared_ptr<SynapseMap>>(val)) return val;
        throw RuntimeError("Type mismatch: Expected map", ln, col);
    }
    if (typeName == "time") {
        if (std::holds_alternative<SynapseTime>(val)) return val;
        throw RuntimeError("Type mismatch: Expected time", ln, col);
    }
    return val;
}

// ──────────────────────────────────────────────────────────────────────────
//  Environment
// ──────────────────────────────────────────────────────────────────────────
SynapseValue Environment::get(const std::string& name) const {
    auto it = vars.find(name);
    if (it != vars.end()) return it->second;
    if (parent) return parent->get(name);
    throw RuntimeError("Undefined variable: '" + name + "'");
}

std::string Environment::getTypeConstraint(const std::string& name) const {
    auto it = types.find(name);
    if (it != types.end()) return it->second;
    if (parent) return parent->getTypeConstraint(name);
    return "";
}

void Environment::assign(const std::string& name, SynapseValue val) {
    auto it = vars.find(name);
    if (it != vars.end()) { 
        // Coerce if there's a constraint (line/col info lost here, will throw generic RuntimeError)
        auto typeIt = types.find(name);
        if (typeIt != types.end()) {
            val = coerceToType(typeIt->second, val, 0, 0);
        }
        it->second = std::move(val); 
        return; 
    }
    if (parent) { parent->assign(name, std::move(val)); return; }
    throw RuntimeError("Assignment to undeclared variable: '" + name + "'");
}

bool Environment::has(const std::string& name) const {
    if (vars.count(name)) return true;
    return parent ? parent->has(name) : false;
}

// ──────────────────────────────────────────────────────────────────────────
//  Interpreter construction
// ──────────────────────────────────────────────────────────────────────────
Interpreter::Interpreter(std::shared_ptr<IPlatform> plt)
    : platform(std::move(plt)) {
    globalEnv  = std::make_shared<Environment>();
    currentEnv = globalEnv;
    registerBuiltins();
}

void Interpreter::interpret(ProgramNode& program) {
    program.accept(*this);
}

// ──────────────────────────────────────────────────────────────────────────
//  Internal helpers
// ──────────────────────────────────────────────────────────────────────────
SynapseValue Interpreter::eval(ASTNode& node) {
    node.accept(*this);
    return lastValue;
}

void Interpreter::exec(ASTNode& node) {
    node.accept(*this);
}

void Interpreter::execBlock(BlockNode& block, std::shared_ptr<Environment> env) {
    auto saved = currentEnv;
    currentEnv = std::move(env);
    for (auto& stmt : block.statements)
        exec(*stmt);
    currentEnv = saved;
}

// ──────────────────────────────────────────────────────────────────────────
//  Literals
// ──────────────────────────────────────────────────────────────────────────
void Interpreter::visit(IntLiteralNode& n)    { lastValue = n.value; }
void Interpreter::visit(FloatLiteralNode& n)  { lastValue = n.value; }
void Interpreter::visit(StringLiteralNode& n) { lastValue = n.value; }
void Interpreter::visit(BoolLiteralNode& n)   { lastValue = n.value; }
void Interpreter::visit(NullLiteralNode&)     { lastValue = SynapseNull{}; }

void Interpreter::visit(TimeLiteralNode& n) {
    lastValue = SynapseTime{n.toMs()};
}

void Interpreter::visit(TupleLiteralNode& n) {
    std::vector<SynapseValue> elems;
    elems.reserve(n.elements.size());
    for (auto& expr : n.elements)
        elems.push_back(eval(*expr));
    lastValue = std::make_shared<SynapseTuple>(std::move(elems));
}

// ──────────────────────────────────────────────────────────────────────────
//  Identifier lookup
// ──────────────────────────────────────────────────────────────────────────
void Interpreter::visit(IdentifierNode& n) {
    lastValue = currentEnv->get(n.name);
}

static bool isEqual(const SynapseValue& l, const SynapseValue& r) {
    if (l.index() != r.index()) return false;
    if (std::holds_alternative<std::shared_ptr<SynapseTuple>>(l)) {
        auto p1 = std::get<std::shared_ptr<SynapseTuple>>(l);
        auto p2 = std::get<std::shared_ptr<SynapseTuple>>(r);
        if (p1 == p2) return true;
        if (p1 && p2) return *p1 == *p2;
        return false;
    }
    // Future: add List and Map deep equality here
    return l == r;
}

// ──────────────────────────────────────────────────────────────────────────
//  Binary operators
// ──────────────────────────────────────────────────────────────────────────
SynapseValue Interpreter::applyBinaryOp(const std::string& op,
                                         const SynapseValue& l,
                                         const SynapseValue& r) {
    // String concatenation with +
    if (op == "+" &&
        (std::holds_alternative<std::string>(l) ||
         std::holds_alternative<std::string>(r)))
        return valueToString(l) + valueToString(r);

    // Numeric arithmetic
    bool lIsInt = std::holds_alternative<long long>(l);
    bool rIsInt = std::holds_alternative<long long>(r);
    bool bothInt = lIsInt && rIsInt;

    if (op == "+")  return bothInt ? SynapseValue(valueToInt(l) + valueToInt(r))
                                   : SynapseValue(valueToDouble(l) + valueToDouble(r));
    if (op == "-")  return bothInt ? SynapseValue(valueToInt(l) - valueToInt(r))
                                   : SynapseValue(valueToDouble(l) - valueToDouble(r));
    if (op == "*")  return bothInt ? SynapseValue(valueToInt(l) * valueToInt(r))
                                   : SynapseValue(valueToDouble(l) * valueToDouble(r));
    if (op == "/")  return SynapseValue(valueToDouble(l) / valueToDouble(r));
    if (op == "//") return SynapseValue(static_cast<long long>(
                        std::floor(valueToDouble(l) / valueToDouble(r))));
    if (op == "%")  return bothInt ? SynapseValue(valueToInt(l) % valueToInt(r))
                                   : SynapseValue(std::fmod(valueToDouble(l), valueToDouble(r)));
    if (op == "**") return SynapseValue(std::pow(valueToDouble(l), valueToDouble(r)));

    // Comparison
    if (op == "==")  return SynapseValue(isEqual(l, r));
    if (op == "!=")  return SynapseValue(!isEqual(l, r));
    if (op == "===") return SynapseValue(isEqual(l, r) && l.index() == r.index());
    if (op == "<")   return SynapseValue(valueToDouble(l) < valueToDouble(r));
    if (op == ">")   return SynapseValue(valueToDouble(l) > valueToDouble(r));
    if (op == "<=")  return SynapseValue(valueToDouble(l) <= valueToDouble(r));
    if (op == ">=")  return SynapseValue(valueToDouble(l) >= valueToDouble(r));

    // Logical
    if (op == "AND" || op == "and") return SynapseValue(valueToBool(l) && valueToBool(r));
    if (op == "OR"  || op == "or")  return SynapseValue(valueToBool(l) || valueToBool(r));

    // Bitwise (integer only)
    if (op == "&")  return SynapseValue(valueToInt(l) & valueToInt(r));
    if (op == "|")  return SynapseValue(valueToInt(l) | valueToInt(r));
    if (op == "^")  return SynapseValue(valueToInt(l) ^ valueToInt(r));
    if (op == "<<") return SynapseValue(valueToInt(l) << valueToInt(r));
    if (op == ">>") return SynapseValue(valueToInt(l) >> valueToInt(r));

    throw RuntimeError("Unknown binary operator: " + op);
}

SynapseValue Interpreter::applyCompound(const std::string& op,
                                         const SynapseValue& l,
                                         const SynapseValue& r) {
    return applyBinaryOp(op, l, r);
}

void Interpreter::visit(BinaryExprNode& n) {
    SynapseValue l = eval(*n.left);
    SynapseValue r = eval(*n.right);
    lastValue = applyBinaryOp(n.op, l, r);
}

void Interpreter::visit(UnaryExprNode& n) {
    SynapseValue val = eval(*n.operand);
    if (n.op == "-")   lastValue = std::visit([](auto&& a) -> SynapseValue {
        using T = std::decay_t<decltype(a)>;
        if constexpr (std::is_same_v<T, long long>) return -a;
        if constexpr (std::is_same_v<T, double>)   return -a;
        return SynapseNull{};
    }, val);
    else if (n.op == "NOT" || n.op == "not")
        lastValue = SynapseValue(!valueToBool(val));
    else if (n.op == "~")
        lastValue = SynapseValue(~valueToInt(val));
    else
        throw RuntimeError("Unknown unary operator: " + n.op);
}

// ──────────────────────────────────────────────────────────────────────────
//  Block & Program
// ──────────────────────────────────────────────────────────────────────────
void Interpreter::visit(BlockNode& n) {
    auto childEnv = std::make_shared<Environment>(currentEnv);
    execBlock(n, childEnv);
}

void Interpreter::visit(ProgramNode& n) {
    for (auto& stmt : n.statements)
        exec(*stmt);
}

// ──────────────────────────────────────────────────────────────────────────
//  Variable declaration & assignment
// ──────────────────────────────────────────────────────────────────────────
void Interpreter::visit(VarDeclNode& n) {
    SynapseValue val = eval(*n.value);
    currentEnv->set(n.name, val);
}

void Interpreter::visit(TypedVarDeclNode& n) {
    SynapseValue val = eval(*n.value);
    val = coerceToType(n.typeName, val, n.line, n.column);
    currentEnv->setTyped(n.name, val, n.typeName);
}

void Interpreter::visit(AssignNode& n) {
    SynapseValue val = eval(*n.value);
    currentEnv->assign(n.name, val);
}

void Interpreter::visit(CompoundAssignNode& n) {
    SynapseValue cur  = currentEnv->get(n.name);
    SynapseValue rhs  = eval(*n.value);
    currentEnv->assign(n.name, applyCompound(n.op, cur, rhs));
}

// ──────────────────────────────────────────────────────────────────────────
//  I/O
// ──────────────────────────────────────────────────────────────────────────
void Interpreter::visit(PrintNode& n) {
    SynapseValue val = eval(*n.value);
    std::cout << valueToString(val);
    if (n.newline) std::cout << '\n';
}

void Interpreter::visit(AskNode& n) {
    std::cout << n.prompt << " " << std::flush;
    std::string input;
    std::getline(std::cin, input);

    SynapseValue result;
    if (n.typeCast == "INT" || n.typeCast == "int")
        result = static_cast<long long>(std::stoll(input));
    else if (n.typeCast == "FLOAT" || n.typeCast == "float")
        result = std::stod(input);
    else if (n.typeCast == "BOOL" || n.typeCast == "bool")
        result = (input == "true" || input == "1");
    else
        result = input;

    if (currentEnv->has(n.varName))
        currentEnv->assign(n.varName, result);
    else
        currentEnv->set(n.varName, result);
}

// ──────────────────────────────────────────────────────────────────────────
//  Wait
// ──────────────────────────────────────────────────────────────────────────
void Interpreter::visit(WaitNode& n) {
    SynapseValue dur = eval(*n.duration);
    long long ms = 0;
    if (std::holds_alternative<SynapseTime>(dur))
        ms = std::get<SynapseTime>(dur).ms;
    else
        ms = valueToInt(dur);
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// ──────────────────────────────────────────────────────────────────────────
//  Control flow
// ──────────────────────────────────────────────────────────────────────────
void Interpreter::visit(IfNode& n) {
    if (valueToBool(eval(*n.condition)))
        exec(*n.thenBlock);
    else if (n.elseBlock)
        exec(*n.elseBlock);
}

void Interpreter::visit(RepeatNode& n) {
    long long count = valueToInt(eval(*n.count));
    for (long long i = 0; i < count; ++i)
        exec(*n.body);
}

void Interpreter::visit(WhileNode& n) {
    while (valueToBool(eval(*n.condition)))
        exec(*n.body);
}

// ──────────────────────────────────────────────────────────────────────────
//  Return
// ──────────────────────────────────────────────────────────────────────────
void Interpreter::visit(ReturnNode& n) {
    SynapseValue val = n.value ? eval(*n.value) : SynapseValue{SynapseNull{}};
    throw ReturnSignal{val};
}

// ──────────────────────────────────────────────────────────────────────────
//  Functions
// ──────────────────────────────────────────────────────────────────────────
void Interpreter::visit(FuncDeclNode& n) {
    functions[n.name] = SynapseFunction{
        n.params,
        n.body.get(),
        currentEnv
    };
}

void Interpreter::visit(FuncCallNode& n) {
    std::vector<SynapseValue> args;
    for (auto& arg : n.args) args.push_back(eval(*arg));

    // Built-ins have priority
    auto itB = builtins.find(n.name);
    if (itB != builtins.end()) {
        lastValue = itB->second(args);
        return;
    }

    auto it = functions.find(n.name);
    if (it == functions.end())
        throw RuntimeError("Undefined function: '" + n.name + "'", n.line, n.column);

    SynapseFunction& fn = it->second;

    if (n.args.size() != fn.params.size())
        throw RuntimeError("Function '" + n.name + "' expects " +
                           std::to_string(fn.params.size()) + " arguments, got " +
                           std::to_string(n.args.size()), n.line, n.column);

    // Create local scope inheriting from function's closure
    auto localEnv = std::make_shared<Environment>(fn.closure);
    for (size_t i = 0; i < fn.params.size(); ++i)
        localEnv->set(fn.params[i], args[i]);

    auto savedEnv = currentEnv;
    currentEnv = localEnv;

    try {
        exec(*fn.body);
        lastValue = SynapseNull{};
    } catch (ReturnSignal& ret) {
        lastValue = ret.value;
    }

    currentEnv = savedEnv;
}

// ──────────────────────────────────────────────────────────────────────────
//  Try / Catch
// ──────────────────────────────────────────────────────────────────────────
void Interpreter::visit(TryCatchNode& n) {
    try {
        exec(*n.tryBlock);
    } catch (RuntimeError& e) {
        auto errEnv = std::make_shared<Environment>(currentEnv);
        errEnv->set(n.errorVar, std::string(e.what()));
        auto savedEnv = currentEnv;
        currentEnv = errEnv;
        exec(*n.catchBlock);
        currentEnv = savedEnv;
    }
}

// ──────────────────────────────────────────────────────────────────────────
//  Coordinate extraction helper
//  Accepts any 2-element Tuple or List; extracts (x, y) as ints.
// ──────────────────────────────────────────────────────────────────────────
std::pair<int, int> extractCoord(const SynapseValue& v, int line, int col) {
    auto extract2 = [&](auto getSize, auto getAt) -> std::pair<int, int> {
        if (getSize() < 2)
            throw RuntimeError("Coordinate requires at least 2 elements", line, col);
        int x = static_cast<int>(valueToDouble(getAt(0)));
        int y = static_cast<int>(valueToDouble(getAt(1)));
        return {x, y};
    };

    if (std::holds_alternative<std::shared_ptr<SynapseTuple>>(v)) {
        auto& t = std::get<std::shared_ptr<SynapseTuple>>(v);
        return extract2([&]{ return t->size(); }, [&](size_t i) -> const SynapseValue& { return t->at(i); });
    }
    if (std::holds_alternative<std::shared_ptr<SynapseList>>(v)) {
        auto& l = std::get<std::shared_ptr<SynapseList>>(v);
        return extract2([&]{ return l->elements.size(); }, [&](size_t i) -> const SynapseValue& { return l->elements[i]; });
    }
    throw RuntimeError("MOUSE command requires a 2-element Tuple (x, y) or List [x, y]", line, col);
}

// ──────────────────────────────────────────────────────────────────────────
//  Phase 2: Mouse & Keyboard Automation
// ──────────────────────────────────────────────────────────────────────────
void Interpreter::visit(MouseMoveNode& n) {
    if (!platform)
        throw RuntimeError("No platform available for MOUSE MOVE", n.line, n.column);
    SynapseValue v = eval(*n.pointExpr);
    auto [tx, ty] = extractCoord(v, n.line, n.column);
    platform->mouseMove(tx, ty);
    waitForMouse(tx, ty);
}

void Interpreter::visit(MouseClickNode& n) {
    if (!platform)
        throw RuntimeError("No platform available for MOUSE CLICK", n.line, n.column);
    if (n.pointExpr) {
        SynapseValue v = eval(*n.pointExpr);
        auto [tx, ty] = extractCoord(v, n.line, n.column);
        platform->mouseMove(tx, ty);
        waitForMouse(tx, ty);
    }
    platform->mouseClick(n.button);
}

void Interpreter::waitForMouse(int targetX, int targetY) {
    if (!platform) return;
    
    const int MAX_ATTEMPTS = 50; // ~500ms total
    for (int i = 0; i < MAX_ATTEMPTS; ++i) {
        auto pos = platform->getMousePosition();
        if (pos.x == targetX && pos.y == targetY) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    // If it never arrives, we proceed anyway but perhaps we should log a warning
}

void Interpreter::visit(KeyPressNode& n) {
    if (!platform)
        throw RuntimeError("No platform available for KEY PRESS", n.line, n.column);
    platform->keyPress(n.key);
}

void Interpreter::visit(KeyTypeNode& n) {
    if (!platform)
        throw RuntimeError("No platform available for KEY TYPE", n.line, n.column);
    SynapseValue v = eval(*n.textExpr);
    platform->keyType(valueToString(v));
}

void Interpreter::visit(ListLiteralNode& n) {
    auto list = std::make_shared<SynapseList>();
    for (auto& expr : n.elements) {
        list->elements.push_back(eval(*expr));
    }
    lastValue = list;
}

void Interpreter::visit(MapLiteralNode& n) {
    auto map = std::make_shared<SynapseMap>();
    for (auto& [keyExpr, valExpr] : n.items) {
        std::string key = valueToString(eval(*keyExpr));
        map->items[key] = eval(*valExpr);
    }
    lastValue = map;
}

void Interpreter::visit(IndexAccessNode& n) {
    SynapseValue obj = eval(*n.object);
    SynapseValue idx = eval(*n.index);

    if (std::holds_alternative<std::shared_ptr<SynapseTuple>>(obj)) {
        auto t = std::get<std::shared_ptr<SynapseTuple>>(obj);
        long long i = valueToInt(idx);
        if (i < 0 || i >= static_cast<long long>(t->size()))
            throw RuntimeError("Tuple index out of range", n.line, n.column);
        lastValue = t->at(static_cast<size_t>(i));
    } else if (std::holds_alternative<std::shared_ptr<SynapseList>>(obj)) {
        auto list = std::get<std::shared_ptr<SynapseList>>(obj);
        long long i = valueToInt(idx);
        if (i < 0 || i >= static_cast<long long>(list->elements.size()))
            throw RuntimeError("List index out of range", n.line, n.column);
        lastValue = list->elements[i];
    } else if (std::holds_alternative<std::shared_ptr<SynapseMap>>(obj)) {
        auto map = std::get<std::shared_ptr<SynapseMap>>(obj);
        std::string key = valueToString(idx);
        if (map->items.find(key) == map->items.end())
            throw RuntimeError("Map key not found: " + key, n.line, n.column);
        lastValue = map->items[key];
    } else if (std::holds_alternative<std::string>(obj)) {
        std::string str = std::get<std::string>(obj);
        long long i = valueToInt(idx);
        if (i < 0 || i >= static_cast<long long>(str.size()))
            throw RuntimeError("String index out of range", n.line, n.column);
        lastValue = std::string(1, str[i]);
    } else {
        throw RuntimeError("Indexing not supported for this type", n.line, n.column);
    }
}

void Interpreter::visit(AppOpenNode& n) {
    if (!platform)
        throw RuntimeError("No platform available for APP OPEN", n.line, n.column);
    SynapseValue name = eval(*n.nameExpr);
    platform->openApp(valueToString(name));
}

void Interpreter::visit(AppListNode& n) {
    if (!platform)
        throw RuntimeError("No platform available for APP LIST", n.line, n.column);
    
    auto list = std::make_shared<SynapseList>();
    auto apps = platform->getAvailableApps();
    for (const auto& app : apps) {
        list->elements.push_back(app);
    }
    lastValue = list;
}

void Interpreter::registerBuiltins() {
    // get_mouse_pos() -> POINT
    builtins["get_mouse_pos"] = [this](const std::vector<SynapseValue>& args) -> SynapseValue {
        if (!platform) {
            std::vector<SynapseValue> zero = {0LL, 0LL};
            return std::make_shared<SynapseTuple>(std::move(zero));
        }
        auto p = platform->getMousePosition();
        std::vector<SynapseValue> elems = {
            static_cast<long long>(p.x),
            static_cast<long long>(p.y)
        };
        return std::make_shared<SynapseTuple>(std::move(elems));
    };

    // assert(condition, message)
    builtins["assert"] = [](const std::vector<SynapseValue>& args) -> SynapseValue {
        if (args.empty()) return SynapseNull{};
        bool condition = valueToBool(args[0]);
        if (!condition) {
            std::string msg = args.size() > 1 ? valueToString(args[1]) : "Assertion failed";
            throw RuntimeError(msg);
        }
        return SynapseNull{};
    };

    // size(tuple_or_list_or_map_or_str)
    builtins["size"] = [](const std::vector<SynapseValue>& args) -> SynapseValue {
        if (args.empty()) return 0LL;
        const SynapseValue& v = args[0];
        if (std::holds_alternative<std::shared_ptr<SynapseTuple>>(v))
            return static_cast<long long>(std::get<std::shared_ptr<SynapseTuple>>(v)->size());
        if (std::holds_alternative<std::shared_ptr<SynapseList>>(v))
            return static_cast<long long>(std::get<std::shared_ptr<SynapseList>>(v)->elements.size());
        if (std::holds_alternative<std::shared_ptr<SynapseMap>>(v))
            return static_cast<long long>(std::get<std::shared_ptr<SynapseMap>>(v)->items.size());
        if (std::holds_alternative<std::string>(v))
            return static_cast<long long>(std::get<std::string>(v).size());
        return 0LL;
    };

    // keys(map) -> LIST of strings
    builtins["keys"] = [](const std::vector<SynapseValue>& args) -> SynapseValue {
        if (args.empty()) return std::make_shared<SynapseList>();
        const SynapseValue& v = args[0];
        auto res = std::make_shared<SynapseList>();
        if (std::holds_alternative<std::shared_ptr<SynapseMap>>(v)) {
            auto m = std::get<std::shared_ptr<SynapseMap>>(v);
            for (auto const& [key, val] : m->items) {
                res->elements.push_back(key);
            }
        }
        return res;
    };
}

} // namespace Synapse
