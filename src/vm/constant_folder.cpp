#include "vm/constant_folder.h"
#include <cmath>

namespace Synapse {

NodePtr ConstantFolder::foldNode(NodePtr node) {
    if (!node) return nullptr;
    node->accept(*this);
    if (lastFolded) {
        NodePtr result = std::move(lastFolded);
        lastFolded = nullptr;
        return result;
    }
    return node;
}

void ConstantFolder::fold(ProgramNode& program) {
    for (auto& stmt : program.statements) {
        stmt = foldNode(std::move(stmt));
    }
}

// ── Helpers ──────────────────────────────────────────────────────────────

static bool isLiteral(const NodePtr& node) {
    if (!node) return false;
    switch (node->type) {
        case NodeType::INT_LIT:
        case NodeType::FLOAT_LIT:
        case NodeType::STR_LIT:
        case NodeType::BOOL_LIT:
        case NodeType::NULL_LIT:
            return true;
        default:
            return false;
    }
}

static SynapseValue getLiteralValue(const NodePtr& node) {
    switch (node->type) {
        case NodeType::INT_LIT:   return SynapseValue(static_cast<IntLiteralNode*>(node.get())->value);
        case NodeType::FLOAT_LIT: return SynapseValue(static_cast<FloatLiteralNode*>(node.get())->value);
        case NodeType::STR_LIT:   return makeString(static_cast<StringLiteralNode*>(node.get())->value);
        case NodeType::BOOL_LIT:  return SynapseValue(static_cast<BoolLiteralNode*>(node.get())->value);
        default:                  return SynapseValue();
    }
}

static NodePtr valueToLiteralNode(const SynapseValue& v, int line, int col) {
    NodePtr node;
    if (v.type == ValueType::VAL_INT) node = std::make_unique<IntLiteralNode>(v.as.i);
    else if (v.type == ValueType::VAL_FLOAT) node = std::make_unique<FloatLiteralNode>(v.as.f);
    else if (v.type == ValueType::VAL_OBJ && v.as.obj->type == ObjType::STR) node = std::make_unique<StringLiteralNode>(static_cast<ObjString*>(v.as.obj)->chars);
    else if (v.type == ValueType::VAL_BOOL) node = std::make_unique<BoolLiteralNode>(v.as.b);
    else if (v.type == ValueType::VAL_NULL) node = std::make_unique<NullLiteralNode>();
    else return nullptr;
    
    node->line = line;
    node->column = col;
    return node;
}

// ── Visitors ──────────────────────────────────────────────────────────────

void ConstantFolder::visit(IntLiteralNode&) {}
void ConstantFolder::visit(FloatLiteralNode&) {}
void ConstantFolder::visit(StringLiteralNode&) {}
void ConstantFolder::visit(BoolLiteralNode&) {}
void ConstantFolder::visit(NullLiteralNode&) {}
void ConstantFolder::visit(TimeLiteralNode&) {}
void ConstantFolder::visit(TupleLiteralNode& n) {
    for (auto& el : n.elements) el = foldNode(std::move(el));
}
void ConstantFolder::visit(IdentifierNode&) {}

void ConstantFolder::visit(BinaryExprNode& n) {
    n.left = foldNode(std::move(n.left));
    n.right = foldNode(std::move(n.right));

    if (isLiteral(n.left) && isLiteral(n.right)) {
        SynapseValue l = getLiteralValue(n.left);
        SynapseValue r = getLiteralValue(n.right);
        SynapseValue res;

        try {
            if (n.op == "+") {
                if ((l.type == ValueType::VAL_OBJ && l.as.obj->type == ObjType::STR) || 
                    (r.type == ValueType::VAL_OBJ && r.as.obj->type == ObjType::STR))
                    res = makeString(valueToString(l) + valueToString(r));
                else if (l.type == ValueType::VAL_INT && r.type == ValueType::VAL_INT) res = l.as.i + r.as.i;
                else res = valueToDouble(l) + valueToDouble(r);
            } else if (n.op == "-") {
                if (l.type == ValueType::VAL_INT && r.type == ValueType::VAL_INT) res = l.as.i - r.as.i;
                else res = valueToDouble(l) - valueToDouble(r);
            } else if (n.op == "*") {
                if (l.type == ValueType::VAL_INT && r.type == ValueType::VAL_INT) res = l.as.i * r.as.i;
                else res = valueToDouble(l) * valueToDouble(r);
            } else if (n.op == "/") {
                double den = valueToDouble(r);
                if (den == 0.0) return;
                res = valueToDouble(l) / den;
            } else if (n.op == "//") {
                long long den = valueToInt(r);
                if (den != 0) res = valueToInt(l) / den;
                else return;
            } else if (n.op == "%") {
                long long den = valueToInt(r);
                if (den != 0) res = valueToInt(l) % den;
                else return;
            } else if (n.op == "**") {
                res = std::pow(valueToDouble(l), valueToDouble(r));
            } else if (n.op == "==") {
                res = valuesAreEqual(l, r);
            } else if (n.op == "!=") {
                res = !valuesAreEqual(l, r);
            } else if (n.op == ">") {
                res = valueToDouble(l) > valueToDouble(r);
            } else if (n.op == "<") {
                res = valueToDouble(l) < valueToDouble(r);
            } else if (n.op == ">=") {
                res = valueToDouble(l) >= valueToDouble(r);
            } else if (n.op == "<=") {
                res = valueToDouble(l) <= valueToDouble(r);
            } else if (n.op == "AND") {
                res = valueToBool(l) && valueToBool(r);
            } else if (n.op == "OR") {
                res = valueToBool(l) || valueToBool(r);
            } else {
                return;
            }

            lastFolded = valueToLiteralNode(res, n.line, n.column);
            decref(l); decref(r); 
        } catch (...) {}
    }
}

void ConstantFolder::visit(UnaryExprNode& n) {
    n.operand = foldNode(std::move(n.operand));
    if (isLiteral(n.operand)) {
        SynapseValue v = getLiteralValue(n.operand);
        SynapseValue res;
        if (n.op == "-") {
            if (v.type == ValueType::VAL_INT) res = -v.as.i;
            else res = -valueToDouble(v);
        } else if (n.op == "NOT") {
            res = !valueToBool(v);
        } else return;
        
        lastFolded = valueToLiteralNode(res, n.line, n.column);
        decref(v);
    }
}

void ConstantFolder::visit(BlockNode& n) {
    for (auto& stmt : n.statements) stmt = foldNode(std::move(stmt));
}

void ConstantFolder::visit(ProgramNode& n) {
    for (auto& stmt : n.statements) stmt = foldNode(std::move(stmt));
}

void ConstantFolder::visit(VarDeclNode& n) {
    if (n.value) n.value = foldNode(std::move(n.value));
}

void ConstantFolder::visit(TypedVarDeclNode& n) {
    if (n.value) n.value = foldNode(std::move(n.value));
}

void ConstantFolder::visit(AssignNode& n) {
    n.value = foldNode(std::move(n.value));
}

void ConstantFolder::visit(CompoundAssignNode& n) {
    n.value = foldNode(std::move(n.value));
}

void ConstantFolder::visit(PrintNode& n) {
    n.value = foldNode(std::move(n.value));
}

void ConstantFolder::visit(WaitNode& n) {
    n.duration = foldNode(std::move(n.duration));
}

void ConstantFolder::visit(ReturnNode& n) {
    if (n.value) n.value = foldNode(std::move(n.value));
}

void ConstantFolder::visit(IfNode& n) {
    n.condition = foldNode(std::move(n.condition));
    n.thenBlock = foldNode(std::move(n.thenBlock));
    if (n.elseBlock) n.elseBlock = foldNode(std::move(n.elseBlock));
}

void ConstantFolder::visit(RepeatNode& n) {
    n.count = foldNode(std::move(n.count));
    n.body = foldNode(std::move(n.body));
}

void ConstantFolder::visit(WhileNode& n) {
    n.condition = foldNode(std::move(n.condition));
    n.body = foldNode(std::move(n.body));
}

void ConstantFolder::visit(FuncDeclNode& n) {
    n.body = foldNode(std::move(n.body));
}

void ConstantFolder::visit(FuncCallNode& n) {
    for (auto& arg : n.args) arg = foldNode(std::move(arg));
}

void ConstantFolder::visit(TryCatchNode& n) {
    n.tryBlock = foldNode(std::move(n.tryBlock));
    n.catchBlock = foldNode(std::move(n.catchBlock));
}

void ConstantFolder::visit(MouseMoveNode& n) {
    n.pointExpr = foldNode(std::move(n.pointExpr));
}

void ConstantFolder::visit(MouseClickNode& n) {
    if (n.pointExpr) n.pointExpr = foldNode(std::move(n.pointExpr));
}

void ConstantFolder::visit(KeyPressNode& n) {}

void ConstantFolder::visit(KeyTypeNode& n) {
    n.textExpr = foldNode(std::move(n.textExpr));
}

void ConstantFolder::visit(AppOpenNode& n) {
    n.nameExpr = foldNode(std::move(n.nameExpr));
}

void ConstantFolder::visit(AppListNode& n) {}

void ConstantFolder::visit(ListLiteralNode& n) {
    for (auto& el : n.elements) el = foldNode(std::move(el));
}

void ConstantFolder::visit(MapLiteralNode& n) {
    for (auto& pair : n.items) {
        pair.first = foldNode(std::move(pair.first));
        pair.second = foldNode(std::move(pair.second));
    }
}

void ConstantFolder::visit(IndexAccessNode& n) {
    n.object = foldNode(std::move(n.object));
    n.index = foldNode(std::move(n.index));
}

void ConstantFolder::visit(IndexSetNode& n) {
    n.object = foldNode(std::move(n.object));
    n.index = foldNode(std::move(n.index));
    n.value = foldNode(std::move(n.value));
}

void ConstantFolder::visit(AskNode& n) {}

void ConstantFolder::visit(ExpressionStmtNode& n) {
    n.expression = foldNode(std::move(n.expression));
}

} // namespace Synapse
