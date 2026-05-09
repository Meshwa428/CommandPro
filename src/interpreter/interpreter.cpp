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
    size_t idx = v.index();
    if (idx == 0) return std::get<0>(v) != 0;
    if (idx == 1) return std::get<1>(v) != 0.0;
    if (idx == 2) return !std::get<2>(v).empty();
    if (idx == 3) return std::get<3>(v);
    if (idx == 5) return false; // SynapseNull
    return true;
}

double valueToDouble(const SynapseValue& v) {
    size_t idx = v.index();
    if (idx == 1) return std::get<1>(v);
    if (idx == 0) return static_cast<double>(std::get<0>(v));
    if (idx == 3) return std::get<3>(v) ? 1.0 : 0.0;
    if (idx == 2) {
        try { return std::stod(std::get<2>(v)); } catch (...) { return 0.0; }
    }
    return 0.0;
}

long long valueToInt(const SynapseValue& v) {
    size_t idx = v.index();
    if (idx == 0) return std::get<0>(v);
    if (idx == 1) return static_cast<long long>(std::get<1>(v));
    if (idx == 3) return std::get<3>(v) ? 1LL : 0LL;
    if (idx == 2) {
        try { return std::stoll(std::get<2>(v)); } catch (...) { return 0LL; }
    }
    return 0LL;
}

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
        case DataType::INT:
            if (val.index() == 0) return val;
            if (val.index() == 1) return static_cast<long long>(std::get<1>(val));
            throw RuntimeError("Type mismatch: Cannot convert to int", ln, col);
        case DataType::FLOAT:
            if (val.index() == 1) return val;
            if (val.index() == 0) return static_cast<double>(std::get<0>(val));
            throw RuntimeError("Type mismatch: Cannot convert to float", ln, col);
        case DataType::STR:
            if (val.index() == 2) return val;
            throw RuntimeError("Type mismatch: Expected str", ln, col);
        case DataType::BOOL:
            if (val.index() == 3) return val;
            throw RuntimeError("Type mismatch: Expected bool", ln, col);
        case DataType::TUPLE:
            if (val.index() == 4) return val;
            throw RuntimeError("Type mismatch: Expected tuple", ln, col);
        case DataType::LIST:
            if (val.index() == 6) return val;
            throw RuntimeError("Type mismatch: Expected list", ln, col);
        case DataType::MAP:
            if (val.index() == 7) return val;
            throw RuntimeError("Type mismatch: Expected map", ln, col);
        case DataType::TIME:
            if (val.index() == 8) return val;
            throw RuntimeError("Type mismatch: Expected time", ln, col);
        default: return val;
    }
}

// ──────────────────────────────────────────────────────────────────────────
//  Environment

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

std::shared_ptr<Environment> Interpreter::acquireEnv(std::shared_ptr<Environment> parent, size_t sizeHint) {
    if (envPool.empty()) {
        auto env = std::make_shared<Environment>(parent);
        if (sizeHint > 0) env->reset(parent, sizeHint);
        return env;
    }
    auto env = std::move(envPool.back());
    envPool.pop_back();
    env->reset(std::move(parent), sizeHint);
    return env;
}

void Interpreter::releaseEnv(std::shared_ptr<Environment> env) {
    if (envPool.size() < 128) { // Keep pool size reasonable
        envPool.push_back(std::move(env));
    }
}

// ──────────────────────────────────────────────────────────────────────────
//  Internal helpers
// ──────────────────────────────────────────────────────────────────────────

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
    lastValue = currentEnv->get(n.name, n.line, n.column);
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
    size_t lIdx = l.index();
    size_t rIdx = r.index();
    bool bothInt = (lIdx == 0 && rIdx == 0);

    if (op == "+")  return bothInt ? SynapseValue(std::get<0>(l) + std::get<0>(r))
                                   : SynapseValue(valueToDouble(l) + valueToDouble(r));
    if (op == "-")  return bothInt ? SynapseValue(std::get<0>(l) - std::get<0>(r))
                                   : SynapseValue(valueToDouble(l) - valueToDouble(r));
    if (op == "*")  return bothInt ? SynapseValue(std::get<0>(l) * std::get<0>(r))
                                   : SynapseValue(valueToDouble(l) * valueToDouble(r));
    if (op == "/")  return SynapseValue(valueToDouble(l) / valueToDouble(r));
    if (op == "//") return SynapseValue(static_cast<long long>(
                        std::floor(valueToDouble(l) / valueToDouble(r))));
    if (op == "%")  return bothInt ? SynapseValue(std::get<0>(l) % std::get<0>(r))
                                   : SynapseValue(std::fmod(valueToDouble(l), valueToDouble(r)));
    if (op == "**") return SynapseValue(std::pow(valueToDouble(l), valueToDouble(r)));

    // Comparison
    // Comparison
    if (op == "==")  return SynapseValue(isEqual(l, r));
    if (op == "!=")  return SynapseValue(!isEqual(l, r));
    if (op == "===") return SynapseValue(lIdx == rIdx && isEqual(l, r));
    if (op == "<")   return bothInt ? SynapseValue(std::get<0>(l) < std::get<0>(r))
                                    : SynapseValue(valueToDouble(l) < valueToDouble(r));
    if (op == ">")   return bothInt ? SynapseValue(std::get<0>(l) > std::get<0>(r))
                                    : SynapseValue(valueToDouble(l) > valueToDouble(r));
    if (op == "<=")  return bothInt ? SynapseValue(std::get<0>(l) <= std::get<0>(r))
                                    : SynapseValue(valueToDouble(l) <= valueToDouble(r));
    if (op == ">=")  return bothInt ? SynapseValue(std::get<0>(l) >= std::get<0>(r))
                                    : SynapseValue(valueToDouble(l) >= valueToDouble(r));

    // Logical
    // Logical (Short-circuiting is handled at the AST level usually, but this is for expressions)
    if (op == "AND" || op == "and") {
        bool bl = (lIdx == 3) ? std::get<3>(l) : valueToBool(l);
        if (!bl) return SynapseValue(false);
        return SynapseValue((rIdx == 3) ? std::get<3>(r) : valueToBool(r));
    }
    if (op == "OR"  || op == "or") {
        bool bl = (lIdx == 3) ? std::get<3>(l) : valueToBool(l);
        if (bl) return SynapseValue(true);
        return SynapseValue((rIdx == 3) ? std::get<3>(r) : valueToBool(r));
    }

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
    SynapseValue l;
    if (n.left->type == NodeType::IDENTIFIER) {
        auto* ident = static_cast<IdentifierNode*>(n.left.get());
        l = currentEnv->get(ident->name, ident->line, ident->column);
    } else {
        l = eval(*n.left);
    }

    SynapseValue r;
    if (n.right->type == NodeType::IDENTIFIER) {
        auto* ident = static_cast<IdentifierNode*>(n.right.get());
        r = currentEnv->get(ident->name, ident->line, ident->column);
    } else if (n.right->type == NodeType::INT_LIT) {
        r = static_cast<IntLiteralNode*>(n.right.get())->value;
    } else {
        r = eval(*n.right);
    }

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
    if (isReturning) return;
    if (n.needsScope) {
        auto childEnv = acquireEnv(currentEnv);
        execBlock(n, childEnv);
        releaseEnv(childEnv);
    } else {
        for (auto& stmt : n.statements) {
            exec(*stmt);
            if (isReturning) break;
        }
    }
}

void Interpreter::visit(ProgramNode& n) {
    for (auto& stmt : n.statements) {
        exec(*stmt);
        if (isReturning) break;
    }
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
    DataType dt = stringToDataType(n.typeName);
    val = coerceToType(dt, val, n.line, n.column);
    currentEnv->setTyped(n.name, val, dt);
}

void Interpreter::visit(AssignNode& n) {
    // Fast-path for common i = i + 1 or sum = sum + i
    if (n.value->type == NodeType::BINARY_EXPR) {
        auto* bExpr = static_cast<BinaryExprNode*>(n.value.get());
        if (bExpr->left->type == NodeType::IDENTIFIER) {
            auto* ident = static_cast<IdentifierNode*>(bExpr->left.get());
            if (ident->name == n.name) { // Self-assignment: var = var op expr
                SynapseValue cur = currentEnv->get(n.name, n.line, n.column);
                if (cur.index() == 0) { // int
                    if (bExpr->right->type == NodeType::INT_LIT) {
                        auto* lit = static_cast<IntLiteralNode*>(bExpr->right.get());
                        if (bExpr->op == "+") {
                            currentEnv->assign(n.name, std::get<0>(cur) + lit->value);
                            return;
                        }
                    } else if (bExpr->right->type == NodeType::IDENTIFIER) {
                        auto* rIdent = static_cast<IdentifierNode*>(bExpr->right.get());
                        SynapseValue rVal = currentEnv->get(rIdent->name, rIdent->line, rIdent->column);
                        if (rVal.index() == 0) {
                            if (bExpr->op == "+") {
                                currentEnv->assign(n.name, std::get<0>(cur) + std::get<0>(rVal));
                                return;
                            }
                        }
                    }
                }
            }
        }
    }

    SynapseValue val = eval(*n.value);
    currentEnv->assign(n.name, val);
}

void Interpreter::visit(CompoundAssignNode& n) {
    SynapseValue cur = currentEnv->get(n.name, n.line, n.column);
    
    // Fast-path for numeric compound assignments
    if (cur.index() == 0) { // long long
        if (n.value->type == NodeType::INT_LIT) {
            auto* lit = static_cast<IntLiteralNode*>(n.value.get());
            if (n.op == "+") {
                currentEnv->assign(n.name, std::get<0>(cur) + lit->value);
                return;
            }
        }
    }

    SynapseValue rhs = eval(*n.value);
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
    if (isReturning) return;
    if (valueToBool(eval(*n.condition))) {
        exec(*n.thenBlock);
    } else if (n.elseBlock) {
        exec(*n.elseBlock);
    }
}

void Interpreter::visit(RepeatNode& n) {
    if (isReturning) return;
    long long count = valueToInt(eval(*n.count));
    for (long long i = 0; i < count; ++i) {
        exec(*n.body);
        if (isReturning) break;
    }
}

void Interpreter::visit(WhileNode& n) {
    if (isReturning) return;
    if (n.isSimpleNumericLoop) {
        // High-performance fast-path for 'while (i < N)'
        auto* rec = currentEnv->getRecord(n.counterVar);
        if (rec && rec->value.index() == 0) {
            long long& i = std::get<0>(rec->value);
            long long limit = n.limit;
            while (i < limit) {
                exec(*n.body);
                if (isReturning) break;
            }
            return;
        }
    }

    while (valueToBool(eval(*n.condition))) {
        exec(*n.body);
        if (isReturning) break;
    }
}

// ──────────────────────────────────────────────────────────────────────────
//  Return
// ──────────────────────────────────────────────────────────────────────────
void Interpreter::visit(ReturnNode& n) {
    returnValue = n.value ? eval(*n.value) : SynapseValue{SynapseNull{}};
    isReturning = true;
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
    args.reserve(n.args.size());
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

    if (args.size() != fn.params.size())
        throw RuntimeError("Function '" + n.name + "' expects " +
                           std::to_string(fn.params.size()) + " arguments, got " +
                           std::to_string(args.size()), n.line, n.column);

    // Create local scope inheriting from function's closure
    auto localEnv = acquireEnv(fn.closure, fn.params.size());
    for (size_t i = 0; i < fn.params.size(); ++i)
        localEnv->set(fn.params[i], args[i]);

    auto savedEnv = currentEnv;
    currentEnv = localEnv;

    bool savedReturning = isReturning;
    isReturning = false;

    exec(*fn.body);
    
    lastValue = isReturning ? returnValue : SynapseValue{SynapseNull{}};
    
    isReturning = savedReturning;
    currentEnv = savedEnv;
    releaseEnv(localEnv);
}

// ──────────────────────────────────────────────────────────────────────────
//  Try / Catch
// ──────────────────────────────────────────────────────────────────────────
void Interpreter::visit(TryCatchNode& n) {
    if (isReturning) return;
    try {
        exec(*n.tryBlock);
    } catch (RuntimeError& e) {
        if (isReturning) throw; // Rethrow if it's not a real error? Wait, I removed the exception return.
        // If it's a real RuntimeError, handle it.
        auto errEnv = acquireEnv(currentEnv);
        errEnv->set(n.errorVar, std::string(e.what()));
        auto savedEnv = currentEnv;
        currentEnv = errEnv;
        exec(*n.catchBlock);
        currentEnv = savedEnv;
        releaseEnv(errEnv);
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

    // now() -> INT (current time in milliseconds)
    builtins["now"] = [](const std::vector<SynapseValue>& args) -> SynapseValue {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        return static_cast<long long>(duration.count());
    };
}

} // namespace Synapse
