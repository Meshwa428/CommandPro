#pragma once
#include "parser/ast.h"
#include "vm/chunk.h"
#include "vm/opcode.h"
#include <memory>
#include <vector>
#include <unordered_map>

namespace Synapse {

class Compiler : public ASTVisitor {
public:
    ObjFunction* compile(ProgramNode& program);

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
    void visit(IndexSetNode&)        override;
    void visit(ExpressionStmtNode&)  override;

    const std::vector<std::string>& getGlobalNames() const { return globalNames; }

private:
    std::vector<ObjFunction*>                functionStack;
    std::vector<std::string>                 globalNames;
    std::unordered_map<std::string, int>     globalLookup;

    int getGlobalIndex(const std::string& name);
    
    ObjFunction* current() { return functionStack.back(); }

    void emitByte(uint8_t byte, int line);
    void emitBytes(uint8_t b1, uint8_t b2, int line);
    void emitConstant(SynapseValue value, int line);
    int  makeConstant(SynapseValue value);
    int  emitJump(uint8_t instruction, int line);
    void patchJump(int offset);
    void emitLoop(int loopStart, int line);
};

} // namespace Synapse
