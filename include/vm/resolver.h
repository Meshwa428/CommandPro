#pragma once
#include "parser/ast.h"
#include <vector>
#include <unordered_map>

namespace Synapse {

class Resolver : public ASTVisitor {
public:
    void resolve(ProgramNode& program);

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

    // Automation
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
    struct Scope {
        std::unordered_map<std::string, int> vars;
    };
    std::vector<Scope> scopes;
    int nextSlot = 0;

    void beginScope();
    void endScope();
    void declare(const std::string& name, ASTNode& node);
    void resolveLocal(const std::string& name, ASTNode& node);
};

} // namespace Synapse
