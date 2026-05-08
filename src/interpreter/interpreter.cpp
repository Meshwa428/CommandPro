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
        if constexpr (std::is_same_v<T, SynapsePoint>)
            return "(" + std::to_string(a.x) + ", " + std::to_string(a.y) + ")";
        if constexpr (std::is_same_v<T, SynapseNull>)  return "null";
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

// ──────────────────────────────────────────────────────────────────────────
//  Environment
// ──────────────────────────────────────────────────────────────────────────
SynapseValue Environment::get(const std::string& name) const {
    auto it = vars.find(name);
    if (it != vars.end()) return it->second;
    if (parent) return parent->get(name);
    throw RuntimeError("Undefined variable: '" + name + "'");
}

void Environment::assign(const std::string& name, SynapseValue val) {
    auto it = vars.find(name);
    if (it != vars.end()) { it->second = std::move(val); return; }
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

void Interpreter::visit(PointLiteralNode& n) {
    double x = valueToDouble(eval(*n.x));
    double y = valueToDouble(eval(*n.y));
    lastValue = SynapsePoint{x, y};
}

// ──────────────────────────────────────────────────────────────────────────
//  Identifier lookup
// ──────────────────────────────────────────────────────────────────────────
void Interpreter::visit(IdentifierNode& n) {
    lastValue = currentEnv->get(n.name);
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
    if (op == "==")  return SynapseValue(l == r);
    if (op == "!=")  return SynapseValue(l != r);
    if (op == "===") return SynapseValue(l == r && l.index() == r.index());
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
//  Phase 2: Mouse & Keyboard Automation
// ──────────────────────────────────────────────────────────────────────────
void Interpreter::visit(MouseMoveNode& n) {
    if (!platform)
        throw RuntimeError("No platform available for MOUSE MOVE", n.line, n.column);
    SynapseValue v = eval(*n.pointExpr);
    if (!std::holds_alternative<SynapsePoint>(v))
        throw RuntimeError("MOUSE MOVE requires a point (x, y)", n.line, n.column);
    auto& pt = std::get<SynapsePoint>(v);
    int tx = static_cast<int>(pt.x);
    int ty = static_cast<int>(pt.y);
    platform->mouseMove(tx, ty);
    waitForMouse(tx, ty);
}

void Interpreter::visit(MouseClickNode& n) {
    if (!platform)
        throw RuntimeError("No platform available for MOUSE CLICK", n.line, n.column);
    if (n.pointExpr) {
        SynapseValue v = eval(*n.pointExpr);
        if (!std::holds_alternative<SynapsePoint>(v))
            throw RuntimeError("MOUSE CLICK AT requires a point (x, y)", n.line, n.column);
        auto& pt = std::get<SynapsePoint>(v);
        int tx = static_cast<int>(pt.x);
        int ty = static_cast<int>(pt.y);
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

void Interpreter::registerBuiltins() {
    // get_mouse_pos() -> POINT
    builtins["get_mouse_pos"] = [this](const std::vector<SynapseValue>& args) -> SynapseValue {
        if (!platform) return SynapsePoint{0, 0};
        auto p = platform->getMousePosition();
        return SynapsePoint{static_cast<double>(p.x), static_cast<double>(p.y)};
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
}

} // namespace Synapse
