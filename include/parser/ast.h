#pragma once
#include <string>
#include <vector>
#include <memory>
#include "platform/platform.h"

namespace Synapse {

// ── Forward declarations ───────────────────────────────────────────────────
class ASTVisitor;

enum class NodeType {
    INT_LIT, FLOAT_LIT, STR_LIT, BOOL_LIT, NULL_LIT, TIME_LIT,
    TUPLE_LIT, LIST_LIT, MAP_LIT, IDENTIFIER,
    BINARY_EXPR, UNARY_EXPR, BLOCK, PROGRAM,
    VAR_DECL, TYPED_VAR_DECL, ASSIGN, COMPOUND_ASSIGN,
    PRINT, ASK, WAIT, RETURN, IF, REPEAT, WHILE,
    FUNC_DECL, FUNC_CALL, TRY_CATCH,
    MOUSE_MOVE, MOUSE_CLICK, KEY_PRESS, KEY_TYPE, APP_OPEN, APP_LIST,
    INDEX_ACCESS
};

// ── Base node ─────────────────────────────────────────────────────────────
class ASTNode {
public:
    NodeType type;
    int line = 0, column = 0;
    explicit ASTNode(NodeType t) : type(t) {}
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& v) = 0;
};

using NodePtr = std::unique_ptr<ASTNode>;
using NodeList = std::vector<NodePtr>;

// ── Literals ──────────────────────────────────────────────────────────────
class IntLiteralNode : public ASTNode {
public: long long value;
    explicit IntLiteralNode(long long v) : ASTNode(NodeType::INT_LIT), value(v) {}
    void accept(ASTVisitor& v) override;
};

class FloatLiteralNode : public ASTNode {
public: double value;
    explicit FloatLiteralNode(double v) : ASTNode(NodeType::FLOAT_LIT), value(v) {}
    void accept(ASTVisitor& v) override;
};

class StringLiteralNode : public ASTNode {
public: std::string value;
    explicit StringLiteralNode(std::string v) : ASTNode(NodeType::STR_LIT), value(std::move(v)) {}
    void accept(ASTVisitor& v) override;
};

class BoolLiteralNode : public ASTNode {
public: bool value;
    explicit BoolLiteralNode(bool v) : ASTNode(NodeType::BOOL_LIT), value(v) {}
    void accept(ASTVisitor& v) override;
};

class NullLiteralNode : public ASTNode {
public:
    NullLiteralNode() : ASTNode(NodeType::NULL_LIT) {}
    void accept(ASTVisitor& v) override;
};

// Time unit enum
enum class TimeUnit { MS, S, M, H };

class TimeLiteralNode : public ASTNode {
public:
    double   amount;
    TimeUnit unit;
    TimeLiteralNode(double a, TimeUnit u) : ASTNode(NodeType::TIME_LIT), amount(a), unit(u) {}
    // Returns duration in milliseconds
    long long toMs() const;
    void accept(ASTVisitor& v) override;
};

class TupleLiteralNode : public ASTNode {
public:
    NodeList elements;
    explicit TupleLiteralNode(NodeList e) : ASTNode(NodeType::TUPLE_LIT), elements(std::move(e)) {}
    void accept(ASTVisitor& v) override;
};

class ListLiteralNode : public ASTNode {
public:
    NodeList elements;
    explicit ListLiteralNode(NodeList e) : ASTNode(NodeType::LIST_LIT), elements(std::move(e)) {}
    void accept(ASTVisitor& v) override;
};

class MapLiteralNode : public ASTNode {
public:
    std::vector<std::pair<NodePtr, NodePtr>> items;
    explicit MapLiteralNode(std::vector<std::pair<NodePtr, NodePtr>> i) : ASTNode(NodeType::MAP_LIT), items(std::move(i)) {}
    void accept(ASTVisitor& v) override;
};

class IndexAccessNode : public ASTNode {
public:
    NodePtr object;
    NodePtr index;
    IndexAccessNode(NodePtr obj, NodePtr idx) : ASTNode(NodeType::INDEX_ACCESS), object(std::move(obj)), index(std::move(idx)) {}
    void accept(ASTVisitor& v) override;
};

// ── Identifier ────────────────────────────────────────────────────────────
class IdentifierNode : public ASTNode {
public: std::string name;
    explicit IdentifierNode(std::string n) : ASTNode(NodeType::IDENTIFIER), name(std::move(n)) {}
    void accept(ASTVisitor& v) override;
};

// ── Expressions ───────────────────────────────────────────────────────────
class BinaryExprNode : public ASTNode {
public:
    std::string op;
    NodePtr left, right;
    BinaryExprNode(std::string o, NodePtr l, NodePtr r)
        : ASTNode(NodeType::BINARY_EXPR), op(std::move(o)), left(std::move(l)), right(std::move(r)) {}
    void accept(ASTVisitor& v) override;
};

class UnaryExprNode : public ASTNode {
public:
    std::string op;
    NodePtr     operand;
    UnaryExprNode(std::string o, NodePtr n)
        : ASTNode(NodeType::UNARY_EXPR), op(std::move(o)), operand(std::move(n)) {}
    void accept(ASTVisitor& v) override;
};

// ── Statements ────────────────────────────────────────────────────────────
class BlockNode : public ASTNode {
public:
    NodeList statements;
    bool     needsScope = true;
    BlockNode() : ASTNode(NodeType::BLOCK) {}
    void accept(ASTVisitor& v) override;
};

class ProgramNode : public ASTNode {
public:
    NodeList statements;
    ProgramNode() : ASTNode(NodeType::PROGRAM) {}
    void accept(ASTVisitor& v) override;
};

class VarDeclNode : public ASTNode {
public:
    std::string name;
    NodePtr     value;
    VarDeclNode(std::string n, NodePtr v)
        : ASTNode(NodeType::VAR_DECL), name(std::move(n)), value(std::move(v)) {}
    void accept(ASTVisitor& v) override;
};

// Typed variable declaration: int x = expr; float y = 3.14;
// The declared type is validated/coerced at runtime.
class TypedVarDeclNode : public ASTNode {
public:
    std::string typeName; // "int", "float", "str", "bool", "tuple", "list", "map", "time"
    std::string name;
    NodePtr     value;
    TypedVarDeclNode(std::string type, std::string n, NodePtr v)
        : ASTNode(NodeType::TYPED_VAR_DECL), typeName(std::move(type)), name(std::move(n)), value(std::move(v)) {}
    void accept(ASTVisitor& v) override;
};

class AssignNode : public ASTNode {
public:
    std::string name;
    NodePtr     value;
    AssignNode(std::string n, NodePtr v)
        : ASTNode(NodeType::ASSIGN), name(std::move(n)), value(std::move(v)) {}
    void accept(ASTVisitor& v) override;
};

class CompoundAssignNode : public ASTNode {
public:
    std::string name;
    std::string op;   // e.g. "+", "-", "*"
    NodePtr     value;
    CompoundAssignNode(std::string n, std::string o, NodePtr v)
        : ASTNode(NodeType::COMPOUND_ASSIGN), name(std::move(n)), op(std::move(o)), value(std::move(v)) {}
    void accept(ASTVisitor& v) override;
};

class PrintNode : public ASTNode {
public:
    NodePtr value;
    bool    newline;
    PrintNode(NodePtr v, bool nl) : ASTNode(NodeType::PRINT), value(std::move(v)), newline(nl) {}
    void accept(ASTVisitor& v) override;
};

class AskNode : public ASTNode {
public:
    std::string prompt;
    std::string varName;
    std::string typeCast; // "INT", "FLOAT", "STR", "" = raw string
    AskNode(std::string p, std::string var, std::string tc)
        : ASTNode(NodeType::ASK), prompt(std::move(p)), varName(std::move(var)), typeCast(std::move(tc)) {}
    void accept(ASTVisitor& v) override;
};

class WaitNode : public ASTNode {
public:
    NodePtr duration; // expected to be TimeLiteralNode or IdentifierNode
    explicit WaitNode(NodePtr d) : ASTNode(NodeType::WAIT), duration(std::move(d)) {}
    void accept(ASTVisitor& v) override;
};

class ReturnNode : public ASTNode {
public:
    NodePtr value; // may be null for bare return
    explicit ReturnNode(NodePtr v) : ASTNode(NodeType::RETURN), value(std::move(v)) {}
    void accept(ASTVisitor& v) override;
};

// ── Control Flow ─────────────────────────────────────────────────────────
class IfNode : public ASTNode {
public:
    NodePtr condition;
    NodePtr thenBlock;
    NodePtr elseBlock; // may be null
    IfNode(NodePtr c, NodePtr t, NodePtr e)
        : ASTNode(NodeType::IF), condition(std::move(c)), thenBlock(std::move(t)), elseBlock(std::move(e)) {}
    void accept(ASTVisitor& v) override;
};

class RepeatNode : public ASTNode {
public:
    NodePtr count;
    NodePtr body;
    RepeatNode(NodePtr c, NodePtr b)
        : ASTNode(NodeType::REPEAT), count(std::move(c)), body(std::move(b)) {}
    void accept(ASTVisitor& v) override;
};

class WhileNode : public ASTNode {
public:
    NodePtr condition;
    NodePtr body;
    // Fast-path metadata
    bool isSimpleNumericLoop = false;
    std::string counterVar;
    long long limit = 0;
    std::string op;

    WhileNode(NodePtr c, NodePtr b)
        : ASTNode(NodeType::WHILE), condition(std::move(c)), body(std::move(b)) {}
    void accept(ASTVisitor& v) override;
};

// ── Functions ─────────────────────────────────────────────────────────────
class FuncDeclNode : public ASTNode {
public:
    std::string              name;
    std::vector<std::string> params;
    NodePtr                  body;
    FuncDeclNode(std::string n, std::vector<std::string> p, NodePtr b)
        : ASTNode(NodeType::FUNC_DECL), name(std::move(n)), params(std::move(p)), body(std::move(b)) {}
    void accept(ASTVisitor& v) override;
};

class FuncCallNode : public ASTNode {
public:
    std::string name;
    NodeList    args;
    FuncCallNode(std::string n, NodeList a)
        : ASTNode(NodeType::FUNC_CALL), name(std::move(n)), args(std::move(a)) {}
    void accept(ASTVisitor& v) override;
};

// ── Error Handling ────────────────────────────────────────────────────────
class TryCatchNode : public ASTNode {
public:
    NodePtr     tryBlock;
    std::string errorVar;
    NodePtr     catchBlock;
    TryCatchNode(NodePtr t, std::string e, NodePtr c)
        : ASTNode(NodeType::TRY_CATCH), tryBlock(std::move(t)), errorVar(std::move(e)), catchBlock(std::move(c)) {}
    void accept(ASTVisitor& v) override;
};

// ── Visitor Interface ─────────────────────────────────────────────────────
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    virtual void visit(IntLiteralNode&)      = 0;
    virtual void visit(FloatLiteralNode&)    = 0;
    virtual void visit(StringLiteralNode&)   = 0;
    virtual void visit(BoolLiteralNode&)     = 0;
    virtual void visit(NullLiteralNode&)     = 0;
    virtual void visit(TimeLiteralNode&)     = 0;
    virtual void visit(TupleLiteralNode&)    = 0;
    virtual void visit(IdentifierNode&)      = 0;
    virtual void visit(BinaryExprNode&)      = 0;
    virtual void visit(UnaryExprNode&)       = 0;
    virtual void visit(BlockNode&)           = 0;
    virtual void visit(ProgramNode&)         = 0;
    virtual void visit(VarDeclNode&)         = 0;
    virtual void visit(TypedVarDeclNode&)    = 0;
    virtual void visit(AssignNode&)          = 0;
    virtual void visit(CompoundAssignNode&)  = 0;
    virtual void visit(PrintNode&)           = 0;
    virtual void visit(AskNode&)             = 0;
    virtual void visit(WaitNode&)            = 0;
    virtual void visit(ReturnNode&)          = 0;
    virtual void visit(IfNode&)              = 0;
    virtual void visit(RepeatNode&)          = 0;
    virtual void visit(WhileNode&)           = 0;
    virtual void visit(FuncDeclNode&)        = 0;
    virtual void visit(FuncCallNode&)        = 0;
    virtual void visit(TryCatchNode&)        = 0;

    // Phase 2: Automation Nodes
    virtual void visit(class MouseMoveNode&) = 0;
    virtual void visit(class MouseClickNode&) = 0;
    virtual void visit(class KeyPressNode&) = 0;
    virtual void visit(class KeyTypeNode&) = 0;
    virtual void visit(class AppOpenNode&) = 0;
    virtual void visit(class AppListNode&) = 0;
    virtual void visit(ListLiteralNode&)   = 0;
    virtual void visit(MapLiteralNode&)    = 0;
    virtual void visit(IndexAccessNode&)   = 0;
};

// ── Automation Nodes (Phase 2) ─────────────────────────────────────────────

class MouseMoveNode : public ASTNode {
public:
    NodePtr pointExpr; // Evaluates to any 2-element iterable (Tuple or List)

    explicit MouseMoveNode(NodePtr pt) : ASTNode(NodeType::MOUSE_MOVE), pointExpr(std::move(pt)) {}
    void accept(ASTVisitor& v) override;
};

class MouseClickNode : public ASTNode {
public:
    MouseButton button;
    NodePtr pointExpr; // Optional

    explicit MouseClickNode(MouseButton btn, NodePtr pt = nullptr)
        : ASTNode(NodeType::MOUSE_CLICK), button(btn), pointExpr(std::move(pt)) {}
    void accept(ASTVisitor& v) override;
};

class KeyPressNode : public ASTNode {
public:
    std::string key;
    explicit KeyPressNode(std::string k) : ASTNode(NodeType::KEY_PRESS), key(std::move(k)) {}
    void accept(ASTVisitor& v) override;
};

class KeyTypeNode : public ASTNode {
public:
    NodePtr textExpr; // Evaluates to string
    explicit KeyTypeNode(NodePtr t) : ASTNode(NodeType::KEY_TYPE), textExpr(std::move(t)) {}
    void accept(ASTVisitor& v) override;
};

class AppOpenNode : public ASTNode {
public:
    NodePtr nameExpr;
    explicit AppOpenNode(NodePtr n) : ASTNode(NodeType::APP_OPEN), nameExpr(std::move(n)) {}
    void accept(ASTVisitor& v) override;
};

class AppListNode : public ASTNode {
public:
    AppListNode() : ASTNode(NodeType::APP_LIST) {}
    void accept(ASTVisitor& v) override;
};

} // namespace Synapse
