#pragma once
#include "parser/ast.h"
#include "value.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>

namespace Synapse {

// ── Exception for early returns ────────────────────────────────────────────
struct ReturnSignal {
    SynapseValue value;
};

// ── Runtime error ─────────────────────────────────────────────────────────
class RuntimeError : public std::runtime_error {
public:
    int line = 0, column = 0;
    RuntimeError(const std::string& msg, int ln = 0, int col = 0)
        : std::runtime_error(msg), line(ln), column(col) {}
};

// ── Scope-chained variable environment ────────────────────────────────────
class Environment {
public:
    explicit Environment(std::shared_ptr<Environment> parent = nullptr)
        : parent(std::move(parent)) {}

    ~Environment() {
        for (auto& pair : vars) decref(pair.second.value);
    }

    struct VarRecord {
        SynapseValue value;
        DataType     type = DataType::NONE;
    };

    void define(const std::string& name, SynapseValue val) {
        incref(val);
        vars[name] = {val, DataType::NONE};
    }

    void setTyped(const std::string& name, SynapseValue val, DataType dt) {
        incref(val);
        vars[name] = {val, dt};
    }

    void assign(const std::string& name, SynapseValue val) {
        auto it = vars.find(name);
        if (it != vars.end()) {
            incref(val);
            decref(it->second.value);
            it->second.value = val;
            return;
        }
        if (parent) parent->assign(name, val);
        else throw RuntimeError("Undefined variable: '" + name + "'");
    }

    SynapseValue get(const std::string& name, int ln = 0, int col = 0) const {
        auto it = vars.find(name);
        if (it != vars.end()) return it->second.value;
        if (parent) return parent->get(name, ln, col);
        throw RuntimeError("Undefined variable: '" + name + "'", ln, col);
    }

private:
    std::unordered_map<std::string, VarRecord> vars;
    std::shared_ptr<Environment>               parent;
};

// ── Function record ────────────────────────────────────────────────────────
struct SynapseFunction {
    std::string                     name;
    std::vector<std::string>        params;
    ASTNode*                        body;   // non-owning; owned by AST
    std::shared_ptr<Environment>    closure;
};

// ── Interpreter ───────────────────────────────────────────────────────────
class Interpreter : public ASTVisitor {
public:
    explicit Interpreter(std::shared_ptr<IPlatform> platform = nullptr);
    ~Interpreter();
    void interpret(ProgramNode& program);

    // ASTVisitor overrides
    void visit(IntLiteralNode&)      override;
    void visit(FloatLiteralNode&)    override;
    void visit(StringLiteralNode&)   override;
    void visit(BoolLiteralNode&)     override;
    void visit(NullLiteralNode&)     override;
    void visit(TimeLiteralNode&)     override;
    void visit(TupleLiteralNode&)    override;
    void visit(IdentifierNode&)      override;
    void visit(BinaryExprNode&)      override;
    void visit(UnaryExprNode&)       override;
    void visit(BlockNode&)           override;
    void visit(ProgramNode&)         override;
    void visit(VarDeclNode&)         override;
    void visit(TypedVarDeclNode&)    override;
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
    void visit(AppOpenNode&)         override;
    void visit(AppListNode&)         override;
    void visit(ListLiteralNode&)     override;
    void visit(MapLiteralNode&)      override;
    void visit(IndexAccessNode&)     override;
    void visit(ExpressionStmtNode&)  override;

private:
    SynapseValue eval(ASTNode& node);
    SynapseValue applyBinaryOp(const std::string& op, const SynapseValue& l, const SynapseValue& r);
    SynapseValue applyCompound(const std::string& op, const SynapseValue& l, const SynapseValue& r);
    
    std::shared_ptr<Environment>                          currentEnv;
    SynapseValue                                          lastValue;
    SynapseValue                                          returnValue;
    std::unordered_map<std::string, SynapseFunction>      functions;
    std::shared_ptr<IPlatform>                            platform;
    std::unordered_map<std::string, std::shared_ptr<BuiltinFunc>> builtins;

    bool                                                  isReturning = false;

    void registerBuiltins();
};

SynapseValue coerceToType(DataType type, const SynapseValue& val, int ln, int col);

} // namespace Synapse
