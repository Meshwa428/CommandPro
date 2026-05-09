#include "vm/compiler.h"
#include "vm/constant_folder.h"
#include "vm/intern.h"
#include <iostream>

namespace Synapse {

ObjFunction* Compiler::compile(ProgramNode& program) {
    ConstantFolder folder;
    folder.fold(program);

    auto mainFn = new ObjFunction();
    mainFn->name = "<main>";
    mainFn->maxSlots = program.maxSlots;
    incref(mainFn);
    functionStack.push_back(mainFn);
    
    program.accept(*this);
    
    emitByte(OP_RETURN, 0);
    return functionStack.back();
}

void Compiler::emitByte(uint8_t byte, int line) {
    current()->code.push_back(byte);
}

void Compiler::emitBytes(uint8_t b1, uint8_t b2, int line) {
    emitByte(b1, line);
    emitByte(b2, line);
}

int Compiler::makeConstant(SynapseValue value) {
    return VMFunctionHelper::addConstant(current(), value);
}

void Compiler::emitConstant(SynapseValue value, int line) {
    int constant = makeConstant(value);
    emitByte(OP_CONSTANT, line);
    emitByte((constant >> 8) & 0xff, line);
    emitByte(constant & 0xff, line);
}

int Compiler::emitJump(uint8_t instruction, int line) {
    emitByte(instruction, line);
    emitByte(0xff, line);
    emitByte(0xff, line);
    return static_cast<int>(current()->code.size() - 2);
}

void Compiler::patchJump(int offset) {
    int jump = static_cast<int>(current()->code.size() - offset - 2);
    if (jump > 65535) std::cerr << "Too much code to jump over." << std::endl;
    current()->code[offset] = (jump >> 8) & 0xff;
    current()->code[offset + 1] = jump & 0xff;
}

void Compiler::emitLoop(int loopStart, int line) {
    emitByte(OP_LOOP, line);
    int offset = static_cast<int>(current()->code.size() - loopStart + 2);
    if (offset > 65535) std::cerr << "Loop body too large." << std::endl;
    emitByte((offset >> 8) & 0xff, line);
    emitByte(offset & 0xff, line);
}

// ── Visitors ──────────────────────────────────────────────────────────────

void Compiler::visit(IntLiteralNode& n) {
    emitConstant(n.value, n.line);
}

void Compiler::visit(FloatLiteralNode& n) {
    emitConstant(n.value, n.line);
}

void Compiler::visit(StringLiteralNode& n) {
    emitConstant(SynapseValue(StringInterner::instance().intern(n.value)), n.line);
}

void Compiler::visit(BoolLiteralNode& n) {
    emitByte(n.value ? OP_TRUE : OP_FALSE, n.line);
}

void Compiler::visit(NullLiteralNode& n) {
    emitByte(OP_NULL, n.line);
}

void Compiler::visit(TimeLiteralNode& n) {
    SynapseValue v;
    v.type = ValueType::VAL_TIME;
    v.as.ms = static_cast<long long>(n.amount);
    emitConstant(v, n.line);
}

int Compiler::getGlobalIndex(const std::string& name) {
    auto it = globalLookup.find(name);
    if (it != globalLookup.end()) return it->second;
    int index = static_cast<int>(globalNames.size());
    globalNames.push_back(name);
    globalLookup[name] = index;
    return index;
}

void Compiler::visit(IdentifierNode& n) {
    if (n.isGlobal) {
        int index = getGlobalIndex(n.name);
        emitByte(OP_GET_GLOBAL, n.line);
        emitByte((index >> 8) & 0xff, n.line);
        emitByte(index & 0xff, n.line);
    } else {
        emitBytes(OP_GET_LOCAL, static_cast<uint8_t>(n.slotIndex), n.line);
    }
}

void Compiler::visit(VarDeclNode& n) {
    if (n.value) n.value->accept(*this);
    else emitByte(OP_NULL, n.line);
    
    if (n.isGlobal) {
        int index = getGlobalIndex(n.name);
        emitByte(OP_DEFINE_GLOBAL, n.line);
        emitByte((index >> 8) & 0xff, n.line);
        emitByte(index & 0xff, n.line);
    } else {
        emitBytes(OP_SET_LOCAL, static_cast<uint8_t>(n.slotIndex), n.line);
        emitByte(OP_POP, n.line); // Pop the initial value since it's stored in slot
    }
}

void Compiler::visit(TypedVarDeclNode& n) {
    if (n.value) n.value->accept(*this);
    else emitByte(OP_NULL, n.line);
    
    if (n.isGlobal) {
        int index = getGlobalIndex(n.name);
        emitByte(OP_DEFINE_GLOBAL, n.line);
        emitByte((index >> 8) & 0xff, n.line);
        emitByte(index & 0xff, n.line);
    } else {
        emitBytes(OP_SET_LOCAL, static_cast<uint8_t>(n.slotIndex), n.line);
        emitByte(OP_POP, n.line);
    }
}

void Compiler::visit(AssignNode& n) {
    n.value->accept(*this);
    if (n.isGlobal) {
        int index = getGlobalIndex(n.name);
        emitByte(OP_SET_GLOBAL, n.line);
        emitByte((index >> 8) & 0xff, n.line);
        emitByte(index & 0xff, n.line);
    } else {
        emitBytes(OP_SET_LOCAL, static_cast<uint8_t>(n.slotIndex), n.line);
    }
}

void Compiler::visit(CompoundAssignNode& n) {
    if (n.isGlobal) {
        int index = getGlobalIndex(n.name);
        emitByte(OP_GET_GLOBAL, n.line);
        emitByte((index >> 8) & 0xff, n.line);
        emitByte(index & 0xff, n.line);
        n.value->accept(*this);
        if (n.op == "+")      emitByte(OP_ADD, n.line);
        else if (n.op == "-")  emitByte(OP_SUBTRACT, n.line);
        else if (n.op == "*")  emitByte(OP_MULTIPLY, n.line);
        else if (n.op == "/")  emitByte(OP_DIVIDE, n.line);
        else if (n.op == "//") emitByte(OP_INT_DIVIDE, n.line);
        else if (n.op == "%")  emitByte(OP_MODULO, n.line);
        else if (n.op == "**") emitByte(OP_EXPONENT, n.line);
        emitByte(OP_SET_GLOBAL, n.line);
        emitByte((index >> 8) & 0xff, n.line);
        emitByte(index & 0xff, n.line);
    } else {
        emitBytes(OP_GET_LOCAL, static_cast<uint8_t>(n.slotIndex), n.line);
        n.value->accept(*this);
        if (n.op == "+")      emitByte(OP_ADD, n.line);
        else if (n.op == "-")  emitByte(OP_SUBTRACT, n.line);
        else if (n.op == "*")  emitByte(OP_MULTIPLY, n.line);
        else if (n.op == "/")  emitByte(OP_DIVIDE, n.line);
        else if (n.op == "//") emitByte(OP_INT_DIVIDE, n.line);
        else if (n.op == "%")  emitByte(OP_MODULO, n.line);
        else if (n.op == "**") emitByte(OP_EXPONENT, n.line);
        emitBytes(OP_SET_LOCAL, static_cast<uint8_t>(n.slotIndex), n.line);
    }
}

void Compiler::visit(BinaryExprNode& n) {
    n.left->accept(*this);
    n.right->accept(*this);
    if (n.op == "+")  emitByte(OP_ADD, n.line);
    else if (n.op == "-")  emitByte(OP_SUBTRACT, n.line);
    else if (n.op == "*")  emitByte(OP_MULTIPLY, n.line);
    else if (n.op == "/")  emitByte(OP_DIVIDE, n.line);
    else if (n.op == "//") emitByte(OP_INT_DIVIDE, n.line);
    else if (n.op == "%")  emitByte(OP_MODULO, n.line);
    else if (n.op == "**") emitByte(OP_EXPONENT, n.line);
    else if (n.op == "==") emitByte(OP_EQUAL, n.line);
    else if (n.op == "===") emitByte(OP_STRICT_EQUAL, n.line);
    else if (n.op == "!=") { emitByte(OP_EQUAL, n.line); emitByte(OP_NOT, n.line); }
    else if (n.op == ">")  emitByte(OP_GREATER, n.line);
    else if (n.op == "<")  emitByte(OP_LESS, n.line);
    else if (n.op == ">=") { emitByte(OP_LESS, n.line); emitByte(OP_NOT, n.line); }
    else if (n.op == "<=") { emitByte(OP_GREATER, n.line); emitByte(OP_NOT, n.line); }
    else if (n.op == "AND") emitByte(OP_AND, n.line);
    else if (n.op == "OR")  emitByte(OP_OR, n.line);
}

void Compiler::visit(UnaryExprNode& n) {
    n.operand->accept(*this);
    if (n.op == "NOT") emitByte(OP_NOT, n.line);
    else if (n.op == "-") emitByte(OP_NEGATE, n.line);
}

void Compiler::visit(PrintNode& n) {
    n.value->accept(*this);
    emitByte(n.newline ? OP_PRINTLN : OP_PRINT, n.line);
}

void Compiler::visit(ProgramNode& n) {
    for (auto& stmt : n.statements) stmt->accept(*this);
}

void Compiler::visit(BlockNode& n) {
    for (auto& stmt : n.statements) stmt->accept(*this);
}

void Compiler::visit(IfNode& n) {
    n.condition->accept(*this);
    int thenJump = emitJump(OP_JUMP_IF_FALSE, n.line);
    emitByte(OP_POP, n.line);
    n.thenBlock->accept(*this);
    int elseJump = emitJump(OP_JUMP, n.line);
    patchJump(thenJump);
    emitByte(OP_POP, n.line);
    if (n.elseBlock) n.elseBlock->accept(*this);
    patchJump(elseJump);
}

void Compiler::visit(WhileNode& n) {
    int loopStart = static_cast<int>(current()->code.size());
    n.condition->accept(*this);
    int exitJump = emitJump(OP_JUMP_IF_FALSE, n.line);
    emitByte(OP_POP, n.line);
    n.body->accept(*this);
    emitLoop(loopStart, n.line);
    patchJump(exitJump);
    emitByte(OP_POP, n.line);
}

void Compiler::visit(ReturnNode& n) {
    if (n.value) n.value->accept(*this);
    else emitByte(OP_NULL, n.line);
    emitByte(OP_RETURN, n.line);
}

void Compiler::visit(FuncDeclNode& n) {
    auto fn = new ObjFunction();
    fn->name = n.name;
    fn->arity = static_cast<int>(n.params.size());
    fn->maxSlots = n.maxSlots;
    incref(fn);
    functionStack.push_back(fn);
    n.body->accept(*this);
    emitByte(OP_RETURN, n.line);
    auto compiledFn = functionStack.back();
    functionStack.pop_back();
    emitConstant(compiledFn, n.line);
    int index = getGlobalIndex(n.name);
    emitByte(OP_DEFINE_GLOBAL, n.line);
    emitByte((index >> 8) & 0xff, n.line);
    emitByte(index & 0xff, n.line);
    decref(compiledFn);
}

void Compiler::visit(FuncCallNode& n) {
    int index = getGlobalIndex(n.name);
    emitByte(OP_GET_GLOBAL, n.line);
    emitByte((index >> 8) & 0xff, n.line);
    emitByte(index & 0xff, n.line);
    for (auto& arg : n.args) arg->accept(*this);
    emitBytes(OP_CALL, static_cast<uint8_t>(n.args.size()), n.line);
}

void Compiler::visit(TupleLiteralNode& n) {
    for (auto& el : n.elements) el->accept(*this);
    emitByte(OP_TUPLE, n.line);
    emitByte((n.elements.size() >> 8) & 0xff, n.line);
    emitByte(n.elements.size() & 0xff, n.line);
}

void Compiler::visit(ListLiteralNode& n) {
    for (auto& el : n.elements) el->accept(*this);
    emitByte(OP_LIST, n.line);
    emitByte((n.elements.size() >> 8) & 0xff, n.line);
    emitByte(n.elements.size() & 0xff, n.line);
}

void Compiler::visit(MapLiteralNode& n) {
    for (auto& pair : n.items) {
        pair.first->accept(*this);
        pair.second->accept(*this);
    }
    emitByte(OP_MAP, n.line);
    emitByte((n.items.size() >> 8) & 0xff, n.line);
    emitByte(n.items.size() & 0xff, n.line);
}

void Compiler::visit(IndexAccessNode& n) {
    n.object->accept(*this);
    n.index->accept(*this);
    emitByte(OP_INDEX_GET, n.line);
}

void Compiler::visit(ExpressionStmtNode& n) {
    n.expression->accept(*this);
    emitByte(OP_POP, n.line);
}

void Compiler::visit(WaitNode& n) {
    n.duration->accept(*this);
    emitByte(OP_WAIT, n.line);
}

void Compiler::visit(MouseMoveNode& n) {
    n.pointExpr->accept(*this);
    emitByte(OP_MOUSE_MOVE, n.line);
}

void Compiler::visit(MouseClickNode& n) {
    if (n.pointExpr) n.pointExpr->accept(*this);
    else emitByte(OP_NULL, n.line);
    emitBytes(OP_MOUSE_CLICK, static_cast<uint8_t>(n.button), n.line);
}

void Compiler::visit(KeyPressNode& n) {
    emitConstant(SynapseValue(StringInterner::instance().intern(n.key)), n.line);
    emitByte(OP_KEY_PRESS, n.line);
}

void Compiler::visit(KeyTypeNode& n) {
    n.textExpr->accept(*this);
    emitByte(OP_KEY_TYPE, n.line);
}

void Compiler::visit(AppOpenNode& n) {
    int index = getGlobalIndex("app_open");
    emitByte(OP_GET_GLOBAL, n.line);
    emitByte((index >> 8) & 0xff, n.line);
    emitByte(index & 0xff, n.line);
    n.nameExpr->accept(*this);
    emitBytes(OP_CALL, 1, n.line);
}

void Compiler::visit(AppListNode& n) {
    int index = getGlobalIndex("app_list");
    emitByte(OP_GET_GLOBAL, n.line);
    emitByte((index >> 8) & 0xff, n.line);
    emitByte(index & 0xff, n.line);
    emitBytes(OP_CALL, 0, n.line);
}

void Compiler::visit(RepeatNode& n) {
    n.count->accept(*this);
    int loopStart = static_cast<int>(current()->code.size());
    emitByte(OP_CONSTANT, n.line);
    int zero = makeConstant(0LL);
    emitByte((zero >> 8) & 0xff, n.line);
    emitByte(zero & 0xff, n.line);
    emitByte(OP_GREATER, n.line);
    int exitJump = emitJump(OP_JUMP_IF_FALSE, n.line);
    emitByte(OP_POP, n.line);
    n.body->accept(*this);
    emitByte(OP_CONSTANT, n.line);
    int one = makeConstant(1LL);
    emitByte((one >> 8) & 0xff, n.line);
    emitByte(one & 0xff, n.line);
    emitByte(OP_SUBTRACT, n.line);
    emitLoop(loopStart, n.line);
    patchJump(exitJump);
    emitByte(OP_POP, n.line);
}

void Compiler::visit(AskNode& n) {
    int promptIdx = makeConstant(SynapseValue(StringInterner::instance().intern(n.prompt)));
    int typeIdx = makeConstant(SynapseValue(StringInterner::instance().intern(n.typeCast)));
    emitByte(OP_ASK, n.line);
    emitByte((promptIdx >> 8) & 0xff, n.line);
    emitByte(promptIdx & 0xff, n.line);
    emitByte((typeIdx >> 8) & 0xff, n.line);
    emitByte(typeIdx & 0xff, n.line);
    if (n.isGlobal) {
        int index = getGlobalIndex(n.varName);
        emitByte(OP_DEFINE_GLOBAL, n.line);
        emitByte((index >> 8) & 0xff, n.line);
        emitByte(index & 0xff, n.line);
    } else {
        emitBytes(OP_SET_LOCAL, static_cast<uint8_t>(n.slotIndex), n.line);
        emitByte(OP_POP, n.line);
    }
}

void Compiler::visit(TryCatchNode& n) {}

} // namespace Synapse
