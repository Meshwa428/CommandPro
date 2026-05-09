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

// ── Primitive helpers (no SynapseValue dependency) ────────────────────────
struct SynapseTime {
    long long ms = 0;
    bool operator==(const SynapseTime& o) const { return ms == o.ms; }
    bool operator!=(const SynapseTime& o) const { return ms != o.ms; }
};

struct SynapseNull {
    bool operator==(const SynapseNull&) const { return true; }
    bool operator!=(const SynapseNull&) const { return false; }
};

// Forward-declare all recursive collection types before SynapseValue
struct SynapseList;
struct SynapseMap;
class  SynapseTuple;  // fully defined below

// ── Core runtime value variant ─────────────────────────────────────────────
using SynapseValue = std::variant<
    long long,                      // INT
    double,                         // FLOAT
    std::string,                    // STR
    bool,                           // BOOL
    SynapseTime,                    // TIME
    SynapseNull,                    // NULL
    std::shared_ptr<SynapseTuple>,  // TUPLE (immutable)
    std::shared_ptr<SynapseList>,
    std::shared_ptr<SynapseMap>
>;

// ── SynapseTuple — Python-style immutable sequence ─────────────────────────
// Defined after SynapseValue to avoid circular dependency.
// Future: add Small Object Optimization (inline buffer for size≤2) once the
// recursive variant stabilises.
class SynapseTuple {
public:
    explicit SynapseTuple(std::vector<SynapseValue> elems)
        : _data(std::move(elems)) {}

    size_t size() const { return _data.size(); }

    const SynapseValue& at(size_t i) const {
        if (i >= _data.size()) throw std::out_of_range("Tuple index out of range");
        return _data[i];
    }

    bool operator==(const SynapseTuple& o) const { return _data == o._data; }
    bool operator!=(const SynapseTuple& o) const { return !(*this == o); }

private:
    std::vector<SynapseValue> _data;
};

struct SynapseList {
    std::vector<SynapseValue> elements;
    bool operator==(const SynapseList& o) const { return elements == o.elements; }
};

struct SynapseMap {
    std::unordered_map<std::string, SynapseValue> items;
    bool operator==(const SynapseMap& o) const { return items == o.items; }
};

// ── Helper converters ──────────────────────────────────────────────────────
enum class DataType {
    NONE,
    INT,
    FLOAT,
    STR,
    BOOL,
    TUPLE,
    LIST,
    MAP,
    TIME
};

std::string valueToString(const SynapseValue& v);
bool        valueToBool(const SynapseValue& v);
double      valueToDouble(const SynapseValue& v);
long long   valueToInt(const SynapseValue& v);

// Extracts (x, y) as integers from any 2-element collection (Tuple or List).
// Throws RuntimeError if the value is not a compatible collection.
std::pair<int, int> extractCoord(const SynapseValue& v, int line = 0, int col = 0);

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

    struct VarRecord {
        SynapseValue value;
        DataType     type = DataType::NONE;
    };

    void clear() {
        vars.clear();
        parent = nullptr;
    }

    void reset(std::shared_ptr<Environment> newParent, size_t sizeHint = 0) {
        vars.clear();
        parent = std::move(newParent);
        if (sizeHint > 0) vars.reserve(sizeHint);
    }

    void set(const std::string& name, SynapseValue val) {
        vars[name] = {std::move(val), DataType::NONE};
    }
    
    void setTyped(const std::string& name, SynapseValue val, DataType dt) {
        vars[name] = {std::move(val), dt};
    }

    void assign(const std::string& name, SynapseValue val) {
        auto it = vars.find(name);
        if (it != vars.end()) {
            // Optimization: if both are INT, just update the value
            if (it->second.type == DataType::INT && val.index() == 0) {
                it->second.value = std::move(val);
                return;
            }
            // For now, overwrite since we check types at declaration
            it->second.value = std::move(val);
            return;
        }
        if (parent) parent->assign(name, std::move(val));
    }

    SynapseValue get(const std::string& name, int ln = 0, int col = 0) const {
        auto it = vars.find(name);
        if (it != vars.end()) return it->second.value;
        if (parent) return parent->get(name, ln, col);
        throw RuntimeError("Undefined variable: '" + name + "'", ln, col);
    }

    bool has(const std::string& name) const {
        if (vars.count(name)) return true;
        return parent ? parent->has(name) : false;
    }

    std::string getTypeConstraint(const std::string& name) const {
        auto it = vars.find(name);
        if (it != vars.end()) {
            switch (it->second.type) {
                case DataType::INT:   return "int";
                case DataType::FLOAT: return "float";
                case DataType::STR:   return "str";
                case DataType::BOOL:  return "bool";
                case DataType::TUPLE: return "tuple";
                case DataType::LIST:  return "list";
                case DataType::MAP:   return "map";
                case DataType::TIME:  return "time";
                default:              return "";
            }
        }
        if (parent) return parent->getTypeConstraint(name);
        return "";
    }

    VarRecord* getRecord(const std::string& name) {
        auto it = vars.find(name);
        if (it != vars.end()) return &it->second;
        if (parent) return parent->getRecord(name);
        return nullptr;
    }

private:
    std::unordered_map<std::string, VarRecord> vars;
    std::shared_ptr<Environment>               parent;
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

private:
    SynapseValue eval(ASTNode& node) {
        node.accept(*this);
        return lastValue;
    }
    void exec(ASTNode& node) {
        if (isReturning) return;
        node.accept(*this);
    }
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

    // Return control
    bool                                                  isReturning = false;
    SynapseValue                                          returnValue = SynapseNull{};

    // Environment pooling
    std::vector<std::shared_ptr<Environment>>             envPool;
    std::shared_ptr<Environment>                          acquireEnv(std::shared_ptr<Environment> parent, size_t sizeHint = 0);
    void                                                  releaseEnv(std::shared_ptr<Environment> env);

    void registerBuiltins();
};

} // namespace Synapse
