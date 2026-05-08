#include "parser/ast.h"

namespace Synapse {

// ── accept() wiring ────────────────────────────────────────────────────────
void IntLiteralNode    ::accept(ASTVisitor& v) { v.visit(*this); }
void FloatLiteralNode  ::accept(ASTVisitor& v) { v.visit(*this); }
void StringLiteralNode ::accept(ASTVisitor& v) { v.visit(*this); }
void BoolLiteralNode   ::accept(ASTVisitor& v) { v.visit(*this); }
void NullLiteralNode   ::accept(ASTVisitor& v) { v.visit(*this); }
void TimeLiteralNode   ::accept(ASTVisitor& v) { v.visit(*this); }
void PointLiteralNode  ::accept(ASTVisitor& v) { v.visit(*this); }
void IdentifierNode    ::accept(ASTVisitor& v) { v.visit(*this); }
void BinaryExprNode    ::accept(ASTVisitor& v) { v.visit(*this); }
void UnaryExprNode     ::accept(ASTVisitor& v) { v.visit(*this); }
void BlockNode         ::accept(ASTVisitor& v) { v.visit(*this); }
void ProgramNode       ::accept(ASTVisitor& v) { v.visit(*this); }
void VarDeclNode       ::accept(ASTVisitor& v) { v.visit(*this); }
void AssignNode        ::accept(ASTVisitor& v) { v.visit(*this); }
void CompoundAssignNode::accept(ASTVisitor& v) { v.visit(*this); }
void PrintNode         ::accept(ASTVisitor& v) { v.visit(*this); }
void AskNode           ::accept(ASTVisitor& v) { v.visit(*this); }
void WaitNode          ::accept(ASTVisitor& v) { v.visit(*this); }
void ReturnNode        ::accept(ASTVisitor& v) { v.visit(*this); }
void IfNode            ::accept(ASTVisitor& v) { v.visit(*this); }
void RepeatNode        ::accept(ASTVisitor& v) { v.visit(*this); }
void WhileNode         ::accept(ASTVisitor& v) { v.visit(*this); }
void FuncDeclNode      ::accept(ASTVisitor& v) { v.visit(*this); }
void FuncCallNode      ::accept(ASTVisitor& v) { v.visit(*this); }
void TryCatchNode      ::accept(ASTVisitor& v) { v.visit(*this); }

// ── Automation nodes ───────────────────────────────────────────────────────
void MouseMoveNode ::accept(ASTVisitor& v) { v.visit(*this); }
void MouseClickNode::accept(ASTVisitor& v) { v.visit(*this); }
void KeyPressNode  ::accept(ASTVisitor& v) { v.visit(*this); }
void KeyTypeNode   ::accept(ASTVisitor& v) { v.visit(*this); }

// ── TimeLiteralNode::toMs() ───────────────────────────────────────────────
long long TimeLiteralNode::toMs() const {
    switch (unit) {
        case TimeUnit::MS: return static_cast<long long>(amount);
        case TimeUnit::S:  return static_cast<long long>(amount * 1000);
        case TimeUnit::M:  return static_cast<long long>(amount * 60000);
        case TimeUnit::H:  return static_cast<long long>(amount * 3600000);
    }
    return 0;
}

} // namespace Synapse
