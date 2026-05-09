#pragma once
#include <string>
#include <vector>
#include <memory>
#include "platform/platform.h"

namespace Synapse {

class ASTVisitor;

enum class NodeType {
    PROGRAM, BLOCK, VAR_DECL, TYPED_VAR_DECL, ASSIGN, COMPOUND_ASSIGN,
    BINARY_EXPR, UNARY_EXPR, IDENTIFIER, INT_LIT, FLOAT_LIT, STR_LIT, BOOL_LIT, NULL_LIT, TIME_LIT,
    IF_STMT, WHILE_LOOP, REPEAT_LOOP, FUNC_DECL, FUNC_CALL, RETURN_STMT,
    PRINT_STMT, ASK_STMT, WAIT_STMT, TRY_CATCH, 
    MOUSE_MOVE, MOUSE_CLICK, KEY_PRESS, KEY_TYPE, APP_OPEN, APP_LIST,
    TUPLE_LIT, LIST_LIT, MAP_LIT, INDEX_ACCESS,
    EXPR_STMT
};

class ASTNode {
public:
    NodeType type;
    int line = 0, column = 0;
    bool isGlobal = false;
    int  slotIndex = -1;

    explicit ASTNode(NodeType t) : type(t) {}
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& v) = 0;
};

using NodePtr = std::unique_ptr<ASTNode>;

// ── Visitors ──────────────────────────────────────────────────────────────

class IntLiteralNode; class FloatLiteralNode; class StringLiteralNode;
class BoolLiteralNode; class NullLiteralNode; class TimeLiteralNode;
class TupleLiteralNode; class IdentifierNode; class BinaryExprNode;
class UnaryExprNode; class BlockNode; class ProgramNode;
class VarDeclNode; class TypedVarDeclNode; class AssignNode;
class CompoundAssignNode; class PrintNode; class AskNode;
class WaitNode; class ReturnNode; class IfNode;
class RepeatNode; class WhileNode; class FuncDeclNode;
class FuncCallNode; class TryCatchNode; class MouseMoveNode;
class MouseClickNode; class KeyPressNode; class KeyTypeNode;
class AppOpenNode; class AppListNode; class ListLiteralNode;
class MapLiteralNode; class IndexAccessNode; class ExpressionStmtNode;

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    virtual void visit(IntLiteralNode&) = 0;
    virtual void visit(FloatLiteralNode&) = 0;
    virtual void visit(StringLiteralNode&) = 0;
    virtual void visit(BoolLiteralNode&) = 0;
    virtual void visit(NullLiteralNode&) = 0;
    virtual void visit(TimeLiteralNode&) = 0;
    virtual void visit(TupleLiteralNode&) = 0;
    virtual void visit(IdentifierNode&) = 0;
    virtual void visit(BinaryExprNode&) = 0;
    virtual void visit(UnaryExprNode&) = 0;
    virtual void visit(BlockNode&) = 0;
    virtual void visit(ProgramNode&) = 0;
    virtual void visit(VarDeclNode&) = 0;
    virtual void visit(TypedVarDeclNode&) = 0;
    virtual void visit(AssignNode&) = 0;
    virtual void visit(CompoundAssignNode&) = 0;
    virtual void visit(PrintNode&) = 0;
    virtual void visit(AskNode&) = 0;
    virtual void visit(WaitNode&) = 0;
    virtual void visit(ReturnNode&) = 0;
    virtual void visit(IfNode&) = 0;
    virtual void visit(RepeatNode&) = 0;
    virtual void visit(WhileNode&) = 0;
    virtual void visit(FuncDeclNode&) = 0;
    virtual void visit(FuncCallNode&) = 0;
    virtual void visit(TryCatchNode&) = 0;
    virtual void visit(MouseMoveNode&) = 0;
    virtual void visit(MouseClickNode&) = 0;
    virtual void visit(KeyPressNode&) = 0;
    virtual void visit(KeyTypeNode&) = 0;
    virtual void visit(AppOpenNode&) = 0;
    virtual void visit(AppListNode&) = 0;
    virtual void visit(ListLiteralNode&) = 0;
    virtual void visit(MapLiteralNode&) = 0;
    virtual void visit(IndexAccessNode&) = 0;
    virtual void visit(ExpressionStmtNode&) = 0;
};

// ── Nodes ─────────────────────────────────────────────────────────────────

class IntLiteralNode : public ASTNode {
public:
    long long value;
    explicit IntLiteralNode(long long v) : ASTNode(NodeType::INT_LIT), value(v) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class FloatLiteralNode : public ASTNode {
public:
    double value;
    explicit FloatLiteralNode(double v) : ASTNode(NodeType::FLOAT_LIT), value(v) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class StringLiteralNode : public ASTNode {
public:
    std::string value;
    explicit StringLiteralNode(std::string v) : ASTNode(NodeType::STR_LIT), value(std::move(v)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class BoolLiteralNode : public ASTNode {
public:
    bool value;
    explicit BoolLiteralNode(bool v) : ASTNode(NodeType::BOOL_LIT), value(v) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class NullLiteralNode : public ASTNode {
public:
    NullLiteralNode() : ASTNode(NodeType::NULL_LIT) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class TimeLiteralNode : public ASTNode {
public:
    double amount;
    std::string unit;
    TimeLiteralNode(double a, std::string u) : ASTNode(NodeType::TIME_LIT), amount(a), unit(std::move(u)) {}
    long long toMs() const {
        if (unit == "s") return (long long)(amount * 1000);
        return (long long)amount;
    }
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class TupleLiteralNode : public ASTNode {
public:
    std::vector<NodePtr> elements;
    TupleLiteralNode(std::vector<NodePtr> e) : ASTNode(NodeType::TUPLE_LIT), elements(std::move(e)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class IdentifierNode : public ASTNode {
public:
    std::string name;
    explicit IdentifierNode(std::string n) : ASTNode(NodeType::IDENTIFIER), name(std::move(n)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class BinaryExprNode : public ASTNode {
public:
    std::string op;
    NodePtr left, right;
    BinaryExprNode(std::string o, NodePtr l, NodePtr r)
        : ASTNode(NodeType::BINARY_EXPR), op(std::move(o)), left(std::move(l)), right(std::move(r)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class UnaryExprNode : public ASTNode {
public:
    std::string op;
    NodePtr operand;
    UnaryExprNode(std::string o, NodePtr p)
        : ASTNode(NodeType::UNARY_EXPR), op(std::move(o)), operand(std::move(p)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class BlockNode : public ASTNode {
public:
    std::vector<NodePtr> statements;
    bool needsScope = true;
    BlockNode() : ASTNode(NodeType::BLOCK) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class ProgramNode : public ASTNode {
public:
    std::vector<NodePtr> statements;
    int maxSlots = 0;
    ProgramNode() : ASTNode(NodeType::PROGRAM) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class VarDeclNode : public ASTNode {
public:
    std::string name;
    NodePtr value;
    VarDeclNode(std::string n, NodePtr v) : ASTNode(NodeType::VAR_DECL), name(std::move(n)), value(std::move(v)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class TypedVarDeclNode : public ASTNode {
public:
    std::string name, typeName;
    NodePtr value;
    TypedVarDeclNode(std::string n, std::string t, NodePtr v)
        : ASTNode(NodeType::TYPED_VAR_DECL), name(std::move(n)), typeName(std::move(t)), value(std::move(v)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class AssignNode : public ASTNode {
public:
    std::string name;
    NodePtr value;
    AssignNode(std::string n, NodePtr v) : ASTNode(NodeType::ASSIGN), name(std::move(n)), value(std::move(v)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class CompoundAssignNode : public ASTNode {
public:
    std::string name, op;
    NodePtr value;
    CompoundAssignNode(std::string n, std::string o, NodePtr v)
        : ASTNode(NodeType::COMPOUND_ASSIGN), name(std::move(n)), op(std::move(o)), value(std::move(v)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class PrintNode : public ASTNode {
public:
    NodePtr value;
    bool newline;
    PrintNode(NodePtr v, bool nl) : ASTNode(NodeType::PRINT_STMT), value(std::move(v)), newline(nl) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class AskNode : public ASTNode {
public:
    std::string prompt, varName, typeCast;
    AskNode(std::string p, std::string v, std::string t)
        : ASTNode(NodeType::ASK_STMT), prompt(std::move(p)), varName(std::move(v)), typeCast(std::move(t)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class WaitNode : public ASTNode {
public:
    NodePtr duration;
    explicit WaitNode(NodePtr d) : ASTNode(NodeType::WAIT_STMT), duration(std::move(d)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class ReturnNode : public ASTNode {
public:
    NodePtr value;
    explicit ReturnNode(NodePtr v) : ASTNode(NodeType::RETURN_STMT), value(std::move(v)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class IfNode : public ASTNode {
public:
    NodePtr condition, thenBlock, elseBlock;
    IfNode(NodePtr c, NodePtr t, NodePtr e)
        : ASTNode(NodeType::IF_STMT), condition(std::move(c)), thenBlock(std::move(t)), elseBlock(std::move(e)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class RepeatNode : public ASTNode {
public:
    NodePtr count, body;
    RepeatNode(NodePtr c, NodePtr b) : ASTNode(NodeType::REPEAT_LOOP), count(std::move(c)), body(std::move(b)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class WhileNode : public ASTNode {
public:
    NodePtr condition, body;
    bool isSimpleNumericLoop = false;
    std::string counterVar;
    long long limit = 0;
    std::string op;
    WhileNode(NodePtr c, NodePtr b) : ASTNode(NodeType::WHILE_LOOP), condition(std::move(c)), body(std::move(b)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class FuncDeclNode : public ASTNode {
public:
    std::string name;
    std::vector<std::string> params;
    NodePtr body;
    int maxSlots = 0;
    FuncDeclNode(std::string n, std::vector<std::string> p, NodePtr b)
        : ASTNode(NodeType::FUNC_DECL), name(std::move(n)), params(std::move(p)), body(std::move(b)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class FuncCallNode : public ASTNode {
public:
    std::string name;
    std::vector<NodePtr> args;
    FuncCallNode(std::string n, std::vector<NodePtr> a)
        : ASTNode(NodeType::FUNC_CALL), name(std::move(n)), args(std::move(a)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class TryCatchNode : public ASTNode {
public:
    NodePtr tryBlock, catchBlock;
    std::string errorVar;
    TryCatchNode(NodePtr t, std::string e, NodePtr c)
        : ASTNode(NodeType::TRY_CATCH), tryBlock(std::move(t)), errorVar(std::move(e)), catchBlock(std::move(c)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class MouseMoveNode : public ASTNode {
public:
    NodePtr pointExpr;
    explicit MouseMoveNode(NodePtr p) : ASTNode(NodeType::MOUSE_MOVE), pointExpr(std::move(p)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class MouseClickNode : public ASTNode {
public:
    MouseButton button;
    NodePtr pointExpr;
    MouseClickNode(MouseButton b, NodePtr p) : ASTNode(NodeType::MOUSE_CLICK), button(b), pointExpr(std::move(p)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class KeyPressNode : public ASTNode {
public:
    std::string key;
    explicit KeyPressNode(std::string k) : ASTNode(NodeType::KEY_PRESS), key(std::move(k)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class KeyTypeNode : public ASTNode {
public:
    NodePtr textExpr;
    explicit KeyTypeNode(NodePtr t) : ASTNode(NodeType::KEY_TYPE), textExpr(std::move(t)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class AppOpenNode : public ASTNode {
public:
    NodePtr nameExpr;
    explicit AppOpenNode(NodePtr n) : ASTNode(NodeType::APP_OPEN), nameExpr(std::move(n)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class AppListNode : public ASTNode {
public:
    AppListNode() : ASTNode(NodeType::APP_LIST) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class ListLiteralNode : public ASTNode {
public:
    std::vector<NodePtr> elements;
    ListLiteralNode(std::vector<NodePtr> e) : ASTNode(NodeType::LIST_LIT), elements(std::move(e)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class MapLiteralNode : public ASTNode {
public:
    std::vector<std::pair<NodePtr, NodePtr>> items;
    MapLiteralNode(std::vector<std::pair<NodePtr, NodePtr>> i) : ASTNode(NodeType::MAP_LIT), items(std::move(i)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class IndexAccessNode : public ASTNode {
public:
    NodePtr object, index;
    IndexAccessNode(NodePtr o, NodePtr i) : ASTNode(NodeType::INDEX_ACCESS), object(std::move(o)), index(std::move(i)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

class ExpressionStmtNode : public ASTNode {
public:
    NodePtr expression;
    explicit ExpressionStmtNode(NodePtr e) : ASTNode(NodeType::EXPR_STMT), expression(std::move(e)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

} // namespace Synapse
