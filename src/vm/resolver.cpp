#include "vm/resolver.h"

namespace Synapse {

void Resolver::resolve(ProgramNode& program) {
    nextSlot = 0;
    program.accept(*this);
    program.maxSlots = nextSlot;
}

void Resolver::beginScope() {
    scopes.push_back(Scope());
}

void Resolver::endScope() {
    scopes.pop_back();
}

void Resolver::declare(const std::string& name, ASTNode& node) {
    if (scopes.empty()) {
        node.isGlobal = true;
        return;
    }
    Scope& scope = scopes.back();
    int slot = nextSlot++;
    scope.vars[name] = slot;
    node.slotIndex = slot;
}

void Resolver::resolveLocal(const std::string& name, ASTNode& node) {
    for (int i = (int)scopes.size() - 1; i >= 0; --i) {
        if (scopes[i].vars.count(name)) {
            node.slotIndex = scopes[i].vars[name];
            node.isGlobal = false;
            return;
        }
    }
    node.isGlobal = true;
}

void Resolver::visit(IntLiteralNode& n) {}
void Resolver::visit(FloatLiteralNode& n) {}
void Resolver::visit(StringLiteralNode& n) {}
void Resolver::visit(BoolLiteralNode& n) {}
void Resolver::visit(NullLiteralNode& n) {}
void Resolver::visit(TimeLiteralNode& n) {}
void Resolver::visit(TupleLiteralNode& n) {
    for (auto& el : n.elements) el->accept(*this);
}

void Resolver::visit(IdentifierNode& n) {
    resolveLocal(n.name, n);
}

void Resolver::visit(BinaryExprNode& n) {
    n.left->accept(*this);
    n.right->accept(*this);
}

void Resolver::visit(UnaryExprNode& n) {
    n.operand->accept(*this);
}

void Resolver::visit(BlockNode& n) {
    beginScope();
    for (auto& stmt : n.statements) stmt->accept(*this);
    endScope();
}

void Resolver::visit(ProgramNode& n) {
    for (auto& stmt : n.statements) stmt->accept(*this);
}

void Resolver::visit(VarDeclNode& n) {
    if (n.value) n.value->accept(*this);
    declare(n.name, n);
}

void Resolver::visit(TypedVarDeclNode& n) {
    if (n.value) n.value->accept(*this);
    declare(n.name, n);
}

void Resolver::visit(AssignNode& n) {
    n.value->accept(*this);
    resolveLocal(n.name, n);
}

void Resolver::visit(CompoundAssignNode& n) {
    n.value->accept(*this);
    resolveLocal(n.name, n);
}

void Resolver::visit(PrintNode& n) {
    n.value->accept(*this);
}

void Resolver::visit(AskNode& n) {
    declare(n.varName, n);
}

void Resolver::visit(WaitNode& n) {
    n.duration->accept(*this);
}

void Resolver::visit(ReturnNode& n) {
    if (n.value) n.value->accept(*this);
}

void Resolver::visit(IfNode& n) {
    n.condition->accept(*this);
    n.thenBlock->accept(*this);
    if (n.elseBlock) n.elseBlock->accept(*this);
}

void Resolver::visit(RepeatNode& n) {
    n.count->accept(*this);
    n.body->accept(*this);
}

void Resolver::visit(WhileNode& n) {
    n.condition->accept(*this);
    n.body->accept(*this);
}

void Resolver::visit(FuncDeclNode& n) {
    declare(n.name, n);
    int savedNext = nextSlot;
    nextSlot = 0;
    beginScope();
    for (const auto& p : n.params) {
        int slot = nextSlot++;
        scopes.back().vars[p] = slot;
    }
    n.body->accept(*this);
    n.maxSlots = nextSlot;
    endScope();
    nextSlot = savedNext;
}

void Resolver::visit(FuncCallNode& n) {
    for (auto& arg : n.args) arg->accept(*this);
}

void Resolver::visit(TryCatchNode& n) {
    n.tryBlock->accept(*this);
    beginScope();
    int slot = nextSlot++;
    scopes.back().vars[n.errorVar] = slot;
    n.catchBlock->accept(*this);
    endScope();
}

void Resolver::visit(MouseMoveNode& n) {
    n.pointExpr->accept(*this);
}

void Resolver::visit(MouseClickNode& n) {
    if (n.pointExpr) n.pointExpr->accept(*this);
}

void Resolver::visit(KeyPressNode& n) {}

void Resolver::visit(KeyTypeNode& n) {
    n.textExpr->accept(*this);
}

void Resolver::visit(AppOpenNode& n) {
    n.nameExpr->accept(*this);
}

void Resolver::visit(AppListNode& n) {}

void Resolver::visit(ListLiteralNode& n) {
    for (auto& el : n.elements) el->accept(*this);
}

void Resolver::visit(MapLiteralNode& n) {
    for (auto& pair : n.items) {
        pair.first->accept(*this);
        pair.second->accept(*this);
    }
}

void Resolver::visit(IndexAccessNode& n) {
    n.object->accept(*this);
    n.index->accept(*this);
}

void Resolver::visit(ExpressionStmtNode& n) {
    n.expression->accept(*this);
}

} // namespace Synapse
