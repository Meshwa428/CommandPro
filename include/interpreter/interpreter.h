#pragma once
#include "parser/ast.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <functional>
#include <stdexcept>

namespace Synapse {

// ── Runtime value type ─────────────────────────────────────────────────────
struct SynapsePoint {
    double x, y;
    bool operator==(const SynapsePoint& o) const { return x == o.x && y == o.y; }
    bool operator!=(const SynapsePoint& o) const { return !(*this == o); }
};

struct SynapseTime {
    long long ms = 0;
    bool operator==(const SynapseTime& o) const { return ms == o.ms; }
    bool operator!=(const SynapseTime& o) const { return ms != o.ms; }
};

struct SynapseNull {
    bool operator==(const SynapseNull&) const { return true; }
    bool operator!=(const SynapseNull&) const { return false; }
};

using SynapseValue = std::variant<
    long long,      // INT
    double,         // FLOAT
    std::string,    // STR
    bool,           // BOOL
    SynapseTime,    // TIME
    SynapsePoint,   // POINT
    SynapseNull     // NULL
>;

// ── Helper converters ──────────────────────────────────────────────────────
std::string valueToString(const SynapseValue& v);
bool        valueToBool(const SynapseValue& v);
double      valueToDouble(const SynapseValue& v);
long long   valueToInt(const SynapseValue& v);

// ── Exception for early returns ────────────────────────────────────────────
struct ReturnSignal {
    SynapseValue value;
};

// ── Scope-chained variable environment ────────────────────────────────────
class Environment {
public:
    explicit Environment(std::shared_ptr<Environment> parent = nullptr)
        : parent(std::move(parent)) {}

    void set(const std::string& name, SynapseValue val) {
        vars[name] = std::move(val);
    }
    void assign(const std::string& name, SynapseValue val);
    SynapseValue get(const std::string& name) const;
    bool         has(const std::string& name) const;

private:
    std::unordered_map<std::string, SynapseValue> vars;
    std::shared_ptr<Environment>                  parent;
};

// ── Runtime error ─────────────────────────────────────────────────────────
class RuntimeError : public std::runtime_error {
public:
    int line = 0, column = 0;
    RuntimeError(const std::string& msg, int ln = 0, int col = 0)
        : std::runtime_error(msg), line(ln), column(col) {}
};

// ── Function record ────────────────────────────────────────────────────────
struct SynapseFunction {
    std::vector<std::string>        params;
    ASTNode*                        body;   // non-owning; owned by AST
    std::shared_ptr<Environment>    closure;
};

// ── Built-in function signature ───────────────────────────────────────────
using BuiltinFunc = std::function<SynapseValue(const std::vector<SynapseValue>&)>;

// ── Interpreter ───────────────────────────────────────────────────────────
class Interpreter : public ASTVisitor {
public:
    explicit Interpreter(std::shared_ptr<IPlatform> platform = nullptr);
    void interpret(ProgramNode& program);

    // ASTVisitor overrides
    void visit(IntLiteralNode&)      override;
    void visit(FloatLiteralNode&)    override;
    void visit(StringLiteralNode&)   override;
    void visit(BoolLiteralNode&)     override;
    void visit(NullLiteralNode&)     override;
    void visit(TimeLiteralNode&)     override;
    void visit(PointLiteralNode&)    override;
    void visit(IdentifierNode&)      override;
    void visit(BinaryExprNode&)      override;
    void visit(UnaryExprNode&)       override;
    void visit(BlockNode&)           override;
    void visit(ProgramNode&)         override;
    void visit(VarDeclNode&)         override;
    void visit(AssignNode&)          override;
    void visit(CompoundAssignNode&)  override;
    void visit(PrintNode&)           override;
    void visit(AskNode&)             override;
    void visit(WaitNode&)            override;
    void visit(ReturnNode&)          override;
    void visit(IfNode&)              override;
    void visit(RepeatNode&)          override;
    void visit(WhileNode&)           override;
    void visit(FuncDeclNode&)        override;
    void visit(FuncCallNode&)        override;
    void visit(TryCatchNode&)        override;

    // Phase 2: Automation
    void visit(MouseMoveNode&)       override;
    void visit(MouseClickNode&)      override;
    void visit(KeyPressNode&)        override;
    void visit(KeyTypeNode&)         override;

private:
    SynapseValue eval(ASTNode& node);
    void         exec(ASTNode& node);
    void         execBlock(BlockNode& block, std::shared_ptr<Environment> env);
    SynapseValue applyBinaryOp(const std::string& op,
                               const SynapseValue& l,
                               const SynapseValue& r);
    SynapseValue applyCompound(const std::string& op,
                               const SynapseValue& l,
                               const SynapseValue& r);
    void         waitForMouse(int x, int y);

    std::shared_ptr<Environment>                          globalEnv;
    std::shared_ptr<Environment>                          currentEnv;
    SynapseValue                                          lastValue;
    std::unordered_map<std::string, SynapseFunction>      functions;
    std::shared_ptr<IPlatform>                            platform;
    std::unordered_map<std::string, BuiltinFunc>          builtins;

    void registerBuiltins();
};

} // namespace Synapse
