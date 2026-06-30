#include "synapse/compiler/compiler.h"
#include "synapse/runtime/vm.h"
#include "synapse/runtime/opcodes.h"
#include "synapse/frontend/token.h"
#include <cassert>
#include <stdexcept>
#include <cstring>

namespace syn {

// ── Public entry point ────────────────────────────────────────────────────────

ObjFunction* Compiler::compile(const Program& prog, const Source& src,
                               DiagEngine& diag, VM& vm)
{
    Compiler c(src, diag, vm);
    return c.compile_program(prog);
}

Compiler::Compiler(const Source& src, DiagEngine& diag, VM& vm)
    : m_source(src), m_diag(diag), m_vm(vm)
{}

ObjFunction* Compiler::compile_program(const Program& prog)
{
    auto* fn = m_vm.alloc<ObjFunction>();
    fn->name = "<main>";
    fn->arity = 0;

    FnState fs;
    fs.fn = fn;
    fs.scope_depth = 0;
    fs.reg_top = 0;
    m_current = &fs;

    compile_block(prog.stmts);
    emit(enc_I(Op::HALT, 0, 0));

    m_current = nullptr;
    return fn;
}

// ── Function compilation ──────────────────────────────────────────────────────

ObjFunction* Compiler::compile_function(const std::string& name, int arity,
                                        const std::vector<std::string>& param_names,
                                        const Block& body,
                                        std::vector<UpvalueInfo>* out_upvalues)
{
    auto* fn = m_vm.alloc<ObjFunction>();
    fn->name  = name;
    fn->arity = arity;

    FnState fs;
    fs.fn = fn;
    fs.scope_depth = 0;
    fs.reg_top = arity; // params occupy R[0..arity-1]
    fs.enclosing = m_current;
    m_current = &fs;

    // register params as locals
    for (int i = 0; i < arity; ++i) {
        Local loc;
        loc.name  = param_names[i];
        loc.slot  = i;
        loc.depth = 0;
        fs.locals.push_back(loc);
    }

    begin_scope();
    compile_block(body);
    end_scope();
    emit(enc_I(Op::RETURN_0, 0, 0));

    fn->upvalue_count = int(fs.upvalues.size());
    if (out_upvalues) *out_upvalues = fs.upvalues;
    m_current = fs.enclosing;
    return fn;
}

// ── Statement compilation ─────────────────────────────────────────────────────

void Compiler::compile_block(const Block& stmts)
{
    for (auto& stmt : stmts) compile_stmt(stmt.get());
}

void Compiler::compile_stmt(const StmtNode* stmt)
{
    if (!stmt) return;
    if (auto* s = dynamic_cast<const LetStmt*>(stmt))        { compile_let(s); return; }
    if (auto* s = dynamic_cast<const ConstStmt*>(stmt))      { compile_const(s); return; }
    if (auto* s = dynamic_cast<const AssignStmt*>(stmt))     { compile_assign(s); return; }
    if (auto* s = dynamic_cast<const AugAssignStmt*>(stmt))  { compile_aug_assign(s); return; }
    if (auto* s = dynamic_cast<const IfStmt*>(stmt))         { compile_if(s); return; }
    if (auto* s = dynamic_cast<const WhileStmt*>(stmt))      { compile_while(s); return; }
    if (auto* s = dynamic_cast<const RepeatStmt*>(stmt))     { compile_repeat(s); return; }
    if (auto* s = dynamic_cast<const ForStmt*>(stmt))        { compile_for(s); return; }
    if (auto* s = dynamic_cast<const ReturnStmt*>(stmt))     { compile_return(s); return; }
    if (dynamic_cast<const BreakStmt*>(stmt))                { compile_break(); return; }
    if (dynamic_cast<const ContinueStmt*>(stmt))             { compile_continue(); return; }
    if (auto* s = dynamic_cast<const FnDeclStmt*>(stmt))     { compile_fn_decl(s); return; }
    if (auto* s = dynamic_cast<const TryStmt*>(stmt))        { compile_try(s); return; }
    if (auto* s = dynamic_cast<const ThrowStmt*>(stmt))      { compile_throw(s); return; }
    if (auto* s = dynamic_cast<const ExprStmt*>(stmt))       { compile_expr_stmt(s); return; }
    // UseStmt, InScopeStmt: skip for core runtime
}

void Compiler::compile_let(const LetStmt* stmt)
{
    if (stmt->bind_kind == LetBindKind::Simple) {
        int slot = declare_local(stmt->names[0]);
        int src = -1;
        if (!stmt->values.empty())
            src = compile_expr(stmt->values[0].get(), slot);
        else
            emit(enc_I(Op::LOAD_NONE, uint8_t(slot), 0));
        ensure_reg(src, slot);
        mark_initialized(slot);
    } else {
        // tuple/list destructuring
        int rhs = alloc_reg();
        compile_expr(stmt->values[0].get(), rhs);
        int n = int(stmt->names.size());
        for (int i = 0; i < n; ++i) {
            int slot = declare_local(stmt->names[i]);
            emit(enc_RI(Op::GETI, uint8_t(slot), uint8_t(rhs), int64_t(i)));
            mark_initialized(slot);
        }
        free_reg();
    }
}

void Compiler::compile_const(const ConstStmt* stmt)
{
    int slot = declare_local(stmt->name);
    compile_expr(stmt->value.get(), slot);
    mark_initialized(slot);
}

void Compiler::compile_assign(const AssignStmt* stmt)
{
    // simple single assignment is the common case
    if (stmt->lvalues.size() == 1) {
        // Optimize: `s = s + x` → ADD R[s], R[s], R[x]  (in-place mutation path in VM)
        auto* lv_id = dynamic_cast<const IdentExpr*>(stmt->lvalues[0].get());
        auto* bin   = dynamic_cast<const BinaryExpr*>(stmt->values[0].get());
        if (lv_id && bin && bin->op == TokenKind::Plus) {
            auto* bl = dynamic_cast<const IdentExpr*>(bin->left.get());
            if (bl && bl->name == lv_id->name) {
                int slot = resolve_local(lv_id->name);
                if (slot >= 0) {
                    int rhs = alloc_reg();
                    compile_expr(bin->right.get(), rhs);
                    emit(enc_R(Op::ADD, uint8_t(slot), uint8_t(slot), uint8_t(rhs)));
                    free_reg();
                    return;
                }
            }
        }
        int tmp = alloc_reg();
        compile_expr(stmt->values[0].get(), tmp);
        assign_to(stmt->lvalues[0].get(), tmp);
        free_reg();
    } else {
        // multi-assign: evaluate all RHS first
        int base = m_current->reg_top;
        for (auto& val : stmt->values) {
            int t = alloc_reg();
            compile_expr(val.get(), t);
        }
        int n = int(stmt->lvalues.size());
        for (int i = 0; i < n; ++i)
            assign_to(stmt->lvalues[i].get(), base + i);
        free_reg(int(stmt->values.size()));
    }
}

void Compiler::compile_aug_assign(const AugAssignStmt* stmt)
{
    int tmp = alloc_reg();
    compile_expr(stmt->lvalue.get(), tmp);
    int rhs = alloc_reg();
    compile_expr(stmt->value.get(), rhs);

    Op op;
    switch (stmt->op) {
    case TokenKind::PlusEq:       op = Op::ADD;  break;
    case TokenKind::MinusEq:      op = Op::SUB;  break;
    case TokenKind::StarEq:       op = Op::MUL;  break;
    case TokenKind::SlashEq:      op = Op::DIV;  break;
    case TokenKind::SlashSlashEq: op = Op::IDIV; break;
    case TokenKind::PercentEq:    op = Op::MOD;  break;
    case TokenKind::StarStarEq:   op = Op::POW;  break;
    default: throw std::runtime_error("unknown aug-assign op");
    }
    emit(enc_R(op, uint8_t(tmp), uint8_t(tmp), uint8_t(rhs)));
    free_reg(); // rhs
    assign_to(stmt->lvalue.get(), tmp);
    free_reg(); // tmp
}

void Compiler::compile_if(const IfStmt* stmt)
{
    PatchList end_jumps;
    // Detect `expr is none` / `expr is not none` for JNIL/JNNIL fast path
    auto try_nil_jump = [&](const ExprNode* cond, std::size_t& jf_out) -> bool {
        auto* bin = dynamic_cast<const BinaryExpr*>(cond);
        if (!bin) return false;
        bool is_is  = (bin->op == TokenKind::Is);
        bool is_not = (bin->op == TokenKind::Not);
        if (!is_is && !is_not) return false;
        if (!dynamic_cast<const NoneLitExpr*>(bin->right.get())) return false;
        bool t = false;
        int r = compile_operand(bin->left.get(), t);  // local → read in place
        // JNIL=jump-if-nil skips body when expr IS nil; for "is none" we want
        // to skip when expr is NOT none → use JNNIL; "is not none" → JNIL.
        Op jop = is_not ? Op::JNIL : Op::JNNIL;
        jf_out = emit_jump(enc_RJ(jop, uint8_t(r), 0));
        if (t) free_reg();
        return true;
    };

    for (auto& br : stmt->branches) {
        if (!br.cond) {
            // else branch
            compile_block(br.body);
            break;
        }

        std::size_t jf;
        if (!try_nil_jump(br.cond.get(), jf)) {
            int cond = alloc_reg();
            compile_expr(br.cond.get(), cond);
            jf = emit_jump(enc_RJ(Op::JF, uint8_t(cond), 0));
            free_reg();
        }
        begin_scope(); compile_block(br.body); end_scope();
        // jump over remaining branches
        if (&br != &stmt->branches.back()) {
            end_jumps.jumps.push_back(emit_jump(enc_J(Op::JUMP, 0)));
        }
        patch_jump(jf);
    }
    patch_list(end_jumps);
}

void Compiler::compile_while(const WhileStmt* stmt)
{
    std::size_t loop_start = chunk().current_offset();

    int cond = alloc_reg();
    compile_expr(stmt->cond.get(), cond);
    std::size_t exit_jmp = emit_jump(enc_RJ(Op::JF, uint8_t(cond), 0));
    free_reg();

    m_loop_stack.push_back({});
    m_loop_stack.back().continue_target = loop_start;

    begin_scope(); compile_block(stmt->body); end_scope();

    // back-edge
    int64_t back = int64_t(loop_start) - int64_t(chunk().current_offset()) - 1;
    emit(enc_J(Op::JUMP, back));
    patch_jump(exit_jmp);

    // patch breaks/continues
    auto& lp = m_loop_stack.back();
    patch_list(lp.breaks);
    // continues already point to loop_start (back-edge relative), no patching needed
    m_loop_stack.pop_back();
}

void Compiler::compile_repeat(const RepeatStmt* stmt)
{
    // repeat N: counter loop
    int count_reg = alloc_reg();
    compile_expr(stmt->count.get(), count_reg);

    int i_reg = alloc_reg(); // loop counter
    emit(enc_I(Op::LOAD_INT, uint8_t(i_reg), 0));

    std::size_t loop_start = chunk().current_offset();
    // exit if i >= count
    std::size_t exit_jmp = emit_jump(enc_RRJ(Op::JGTE, uint8_t(i_reg), uint8_t(count_reg), 0));

    m_loop_stack.push_back({});
    m_loop_stack.back().continue_target = chunk().current_offset();

    begin_scope();
    if (!stmt->index_name.empty()) {
        int idx_slot = declare_local(stmt->index_name);
        emit(enc_R(Op::MOVE, uint8_t(idx_slot), uint8_t(i_reg)));
        mark_initialized(idx_slot);
    }
    compile_block(stmt->body);
    end_scope();

    emit(enc_RI(Op::ADDI, uint8_t(i_reg), uint8_t(i_reg), 1));
    int64_t back = int64_t(loop_start) - int64_t(chunk().current_offset()) - 1;
    emit(enc_J(Op::JUMP, back));
    patch_jump(exit_jmp);

    auto& lp = m_loop_stack.back();
    patch_list(lp.breaks);
    m_loop_stack.pop_back();
    free_reg(2); // i_reg, count_reg
}

void Compiler::compile_for(const ForStmt* stmt)
{
    if (stmt->range_end) {
        // for x = start to end [by step]
        int base = alloc_reg(); // R[base]=start, R[base+1]=stop, R[base+2]=step
        alloc_reg(); alloc_reg(); // reserve slots

        int start_r = compile_expr(stmt->source.get(), base);
        ensure_reg(start_r, base);

        int stop_r  = alloc_reg();
        compile_expr(stmt->range_end.get(), stop_r);
        emit(enc_R(Op::MOVE, uint8_t(base+1), uint8_t(stop_r)));
        free_reg();

        if (stmt->range_step) {
            int step_r = alloc_reg();
            compile_expr(stmt->range_step.get(), step_r);
            emit(enc_R(Op::MOVE, uint8_t(base+2), uint8_t(step_r)));
            free_reg();
        } else {
            emit(enc_I(Op::LOAD_INT, uint8_t(base+2), 1));
        }

        // iter var
        begin_scope();
        int iter_slot = declare_local(stmt->iter1);
        emit(enc_R(Op::MOVE, uint8_t(iter_slot), uint8_t(base)));
        mark_initialized(iter_slot);

        std::size_t loop_start = chunk().current_offset();
        // exit if iter >= stop (assuming step > 0; negative step not optimized)
        std::size_t exit_jmp = emit_jump(enc_RRJ(Op::JGTE, uint8_t(iter_slot), uint8_t(base+1), 0));

        m_loop_stack.push_back({});
        m_loop_stack.back().continue_target = chunk().current_offset();

        compile_block(stmt->body);

        // continue target: increment
        std::size_t cont_target = chunk().current_offset();
        m_loop_stack.back().continue_target = cont_target;
        emit(enc_R(Op::ADD, uint8_t(iter_slot), uint8_t(iter_slot), uint8_t(base+2)));

        int64_t back = int64_t(loop_start) - int64_t(chunk().current_offset()) - 1;
        emit(enc_J(Op::JUMP, back));
        patch_jump(exit_jmp);

        auto& lp = m_loop_stack.back();
        patch_list(lp.breaks);
        m_loop_stack.pop_back();
        end_scope();
        free_reg(3); // base, base+1, base+2
    } else {
        // for x in iterable
        int list_reg = alloc_reg(); // will hold the list
        compile_expr(stmt->source.get(), list_reg);
        int idx_reg  = alloc_reg(); // counter
        emit(enc_I(Op::LOAD_INT, uint8_t(idx_reg), 0));

        std::size_t loop_start = chunk().current_offset();

        begin_scope();
        int iter_slot = declare_local(stmt->iter1);
        emit(enc_I(Op::LOAD_NONE, uint8_t(iter_slot), 0));
        mark_initialized(iter_slot);

        // FOR_STEP: R[idx]>=len(list) → jump out; else R[iter_slot]=list[idx]; idx++
        // ponytail: inline with STR_LEN + JGE
        int len_reg = alloc_reg();
        emit(enc_R(Op::STR_LEN, uint8_t(len_reg), uint8_t(list_reg)));
        std::size_t exit_jmp = emit_jump(enc_RRJ(Op::JGTE, uint8_t(idx_reg), uint8_t(len_reg), 0));
        free_reg(); // len_reg

        emit(enc_R(Op::GET_FIELD, uint8_t(iter_slot),
                   uint8_t(list_reg), uint8_t(idx_reg)));
        emit(enc_RI(Op::ADDI, uint8_t(idx_reg), uint8_t(idx_reg), 1));

        m_loop_stack.push_back({});
        m_loop_stack.back().continue_target = loop_start;

        compile_block(stmt->body);
        end_scope();

        int64_t back = int64_t(loop_start) - int64_t(chunk().current_offset()) - 1;
        emit(enc_J(Op::JUMP, back));
        patch_jump(exit_jmp);

        auto& lp = m_loop_stack.back();
        patch_list(lp.breaks);
        m_loop_stack.pop_back();
        free_reg(2); // list_reg, idx_reg
    }
}

void Compiler::compile_return(const ReturnStmt* stmt)
{
    if (stmt->values.empty()) {
        emit(enc_I(Op::RETURN_0, 0, 0));
    } else if (stmt->values.size() == 1) {
        int r = alloc_reg();
        compile_expr(stmt->values[0].get(), r);
        emit(enc_R(Op::RETURN_1, uint8_t(r), 0));
        free_reg();
    } else {
        int base = m_current->reg_top;
        for (auto& v : stmt->values) {
            int t = alloc_reg();
            compile_expr(v.get(), t);
        }
        emit(enc_R(Op::RETURN, 0, uint8_t(base), uint8_t(stmt->values.size())));
        free_reg(int(stmt->values.size()));
    }
}

void Compiler::compile_break()
{
    if (m_loop_stack.empty()) throw std::runtime_error("break outside loop");
    m_loop_stack.back().breaks.jumps.push_back(emit_jump(enc_J(Op::JUMP, 0)));
}

void Compiler::compile_continue()
{
    if (m_loop_stack.empty()) throw std::runtime_error("continue outside loop");
    auto& lp = m_loop_stack.back();
    int64_t off = int64_t(lp.continue_target) - int64_t(chunk().current_offset()) - 1;
    emit(enc_J(Op::JUMP, off));
}

void Compiler::compile_fn_decl(const FnDeclStmt* stmt)
{
    // Pre-declare the local so recursive calls inside the body can find it as upvalue.
    int dest = declare_local(stmt->name);
    // Emit LOAD_NONE as placeholder so the slot is initialized while compiling the body.
    emit(enc_I(Op::LOAD_NONE, uint8_t(dest), 0));
    mark_initialized(dest);

    std::vector<std::string> param_names;
    for (auto& p : stmt->params) param_names.push_back(p.name);

    std::vector<UpvalueInfo> inner_upvalues;
    ObjFunction* fn = compile_function(stmt->name, int(stmt->params.size()),
                                       param_names, stmt->body, &inner_upvalues);

    uint16_t ki = add_const(Value::from_ptr(fn));
    emit(enc_RI(Op::CLOSURE, uint8_t(dest), 0, int64_t(ki)));

    // Upvalue descriptor instructions read by VM's CLOSURE handler (one per upvalue)
    for (auto& uv : inner_upvalues)
        emit(enc_R(Op::NOP, uint8_t(uv.is_local ? 1 : 0), uint8_t(uv.index)));
}

void Compiler::compile_try(const TryStmt* stmt)
{
    // ponytail: emit body only; try/catch plumbing TODO after benchmarks work
    compile_block(stmt->body);
}

void Compiler::compile_throw(const ThrowStmt* stmt)
{
    int r = alloc_reg();
    compile_expr(stmt->value.get(), r);
    emit(enc_R(Op::THROW, uint8_t(r), 0));
    free_reg();
}

void Compiler::compile_expr_stmt(const ExprStmt* stmt)
{
    int r = alloc_reg();
    compile_expr(stmt->expr.get(), r);
    free_reg();
}

// ── Expression compilation ────────────────────────────────────────────────────

int Compiler::compile_expr(const ExprNode* expr, int dest)
{
    if (!expr) { int r = dest < 0 ? alloc_reg() : dest; emit(enc_I(Op::LOAD_NONE, uint8_t(r), 0)); return r; }

    if (auto* e = dynamic_cast<const IntLitExpr*>(expr))        return compile_int_lit(e, dest);
    if (auto* e = dynamic_cast<const FloatLitExpr*>(expr))      return compile_float_lit(e, dest);
    if (auto* e = dynamic_cast<const StringLitExpr*>(expr))     return compile_string_lit(e, dest);
    if (auto* e = dynamic_cast<const InterpStringExpr*>(expr))  return compile_interp_string(e, dest);
    if (auto* e = dynamic_cast<const IdentExpr*>(expr))         return compile_ident(e, dest);
    if (auto* e = dynamic_cast<const BinaryExpr*>(expr))        return compile_binary(e, dest);
    if (auto* e = dynamic_cast<const UnaryExpr*>(expr))         return compile_unary(e, dest);
    if (auto* e = dynamic_cast<const TernaryExpr*>(expr))       return compile_ternary(e, dest);
    if (auto* e = dynamic_cast<const CallExpr*>(expr))          return compile_call(e, dest);
    if (auto* e = dynamic_cast<const IndexExpr*>(expr))         return compile_index(e, dest);
    if (auto* e = dynamic_cast<const FieldExpr*>(expr))         return compile_field(e, dest);
    if (auto* e = dynamic_cast<const ListExpr*>(expr))          return compile_list(e, dest);
    if (auto* e = dynamic_cast<const MapExpr*>(expr))           return compile_map(e, dest);
    if (auto* e = dynamic_cast<const TupleExpr*>(expr))         return compile_tuple(e, dest);
    if (auto* e = dynamic_cast<const FnExpr*>(expr))            return compile_fn_expr(e, dest);
    if (auto* e = dynamic_cast<const MatchExpr*>(expr))         return compile_match_expr(e, dest);
    if (dynamic_cast<const BoolLitExpr*>(expr)) {
        int r = dest < 0 ? alloc_reg() : dest;
        auto* be = static_cast<const BoolLitExpr*>(expr);
        emit(enc_I(be->value ? Op::LOAD_TRUE : Op::LOAD_FALSE, uint8_t(r), 0));
        return r;
    }
    if (dynamic_cast<const NoneLitExpr*>(expr)) {
        int r = dest < 0 ? alloc_reg() : dest;
        emit(enc_I(Op::LOAD_NONE, uint8_t(r), 0));
        return r;
    }
    if (auto* e = dynamic_cast<const DurationLitExpr*>(expr)) {
        int r = dest < 0 ? alloc_reg() : dest;
        emit(enc_I(Op::LOAD_DURATION, uint8_t(r), e->ns));
        return r;
    }
    if (dynamic_cast<const ChainedCmpExpr*>(expr)) {
        // ponytail: compile first pair only (full chaining = multiple ANDs)
        auto* ce = static_cast<const ChainedCmpExpr*>(expr);
        auto binary = BinaryExpr{};
        // manually handle
        int r = dest < 0 ? alloc_reg() : dest;
        int lhs = alloc_reg();
        compile_expr(ce->operands[0].get(), lhs);
        for (int i = 0; i < int(ce->ops.size()); ++i) {
            int rhs = alloc_reg();
            compile_expr(ce->operands[i+1].get(), rhs);
            auto fake = BinaryExpr{};
            fake.op = ce->ops[i];
            // inline binary emit
            Op bop;
            switch (ce->ops[i]) {
            case TokenKind::Lt:   bop=Op::LT;  break;
            case TokenKind::LtEq: bop=Op::LTE; break;
            case TokenKind::Gt:   bop=Op::LTE; // swap operands
                emit(enc_R(bop, uint8_t(r), uint8_t(rhs), uint8_t(lhs)));
                emit(enc_R(Op::MOVE, uint8_t(lhs), uint8_t(rhs)));
                free_reg();
                continue;
            case TokenKind::GtEq: bop=Op::LT;
                emit(enc_R(bop, uint8_t(r), uint8_t(rhs), uint8_t(lhs)));
                emit(enc_R(Op::MOVE, uint8_t(lhs), uint8_t(rhs)));
                free_reg();
                continue;
            case TokenKind::EqEq:  bop=Op::EQ;  break;
            case TokenKind::BangEq:bop=Op::NEQ; break;
            default: bop=Op::EQ; break;
            }
            emit(enc_R(bop, uint8_t(r), uint8_t(lhs), uint8_t(rhs)));
            emit(enc_R(Op::MOVE, uint8_t(lhs), uint8_t(rhs)));
            free_reg();
        }
        free_reg(); // lhs
        return r;
    }
    // fallback
    int r = dest < 0 ? alloc_reg() : dest;
    emit(enc_I(Op::LOAD_NONE, uint8_t(r), 0));
    return r;
}

int Compiler::compile_int_lit(const IntLitExpr* e, int dest)
{
    int r = dest < 0 ? alloc_reg() : dest;
    // Fits in signed 48-bit? (always true for int64 values that come from source)
    emit(enc_I(Op::LOAD_INT, uint8_t(r), e->value));
    return r;
}

int Compiler::compile_float_lit(const FloatLitExpr* e, int dest)
{
    int r = dest < 0 ? alloc_reg() : dest;
    uint16_t ki = add_const(Value::from_float(e->value));
    emit(enc_RI(Op::LOAD_FLOAT, uint8_t(r), uint8_t(ki), 0));
    return r;
}

static std::string unescape(const std::string& raw)
{
    // raw includes surrounding quotes
    std::string out;
    std::size_t i = 1; // skip opening quote
    std::size_t end = raw.size() - 1; // skip closing quote
    for (; i < end; ++i) {
        if (raw[i] == '\\' && i+1 < end) {
            ++i;
            switch (raw[i]) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case '\\': out += '\\'; break;
            case '"': out += '"'; break;
            case '\'': out += '\''; break;
            default: out += raw[i]; break;
            }
        } else {
            out += raw[i];
        }
    }
    return out;
}

int Compiler::compile_string_lit(const StringLitExpr* e, int dest)
{
    int r = dest < 0 ? alloc_reg() : dest;
    std::string s = unescape(e->raw);
    uint16_t ki = add_str_const(s);
    emit(enc_I(Op::LOAD_CONST, uint8_t(r), int64_t(ki)));
    return r;
}

int Compiler::compile_interp_string(const InterpStringExpr* e, int dest)
{
    int r = dest < 0 ? alloc_reg() : dest;
    int base = m_current->reg_top;
    int n = 0;
    for (auto& part : e->parts) {
        int pr = alloc_reg(); ++n;
        if (part.is_str) {
            uint16_t ki = add_str_const(part.text);
            emit(enc_I(Op::LOAD_CONST, uint8_t(pr), int64_t(ki)));
        } else {
            int er = alloc_reg(); ++n;
            compile_expr(part.expr.get(), er);
            emit(enc_R(Op::TO_STR, uint8_t(pr), uint8_t(er)));
            // er is above pr but already allocated — we need them contiguous
            // ponytail: just put the str-converted result in pr
            // Actually we allocated them out of order, fix:
            --n; free_reg(); // free er
            // pr now has TO_STR result in wrong slot; re-arrange
            // simpler: convert in-place in pr slot
            compile_expr(part.expr.get(), pr);
            emit(enc_R(Op::TO_STR, uint8_t(pr), uint8_t(pr)));
        }
    }
    emit(enc_R(Op::INTERP, uint8_t(r), uint8_t(base), uint8_t(n)));
    free_reg(n);
    return r;
}

int Compiler::compile_ident(const IdentExpr* e, int dest)
{
    int local = resolve_local(e->name);
    if (local >= 0) {
        if (dest >= 0 && dest != local) {
            emit(enc_R(Op::MOVE, uint8_t(dest), uint8_t(local)));
            return dest;
        }
        return local;
    }
    int upval = resolve_upvalue(e->name);
    if (upval >= 0) {
        int r = dest < 0 ? alloc_reg() : dest;
        emit(enc_R(Op::GET_UPVAL, uint8_t(r), uint8_t(upval)));
        return r;
    }
    // global
    int r = dest < 0 ? alloc_reg() : dest;
    uint16_t ki = add_str_const(e->name);
    emit(enc_I(Op::GET_GLOBAL, uint8_t(r), int64_t(ki)));
    return r;
}

// True if evaluating `e` cannot mutate any variable (no calls/assignments).
// Used to decide when an operand can be read from its own register in place
// rather than copied to a temp first. Conservative: unknown nodes → not pure.
static bool expr_is_pure(const ExprNode* e)
{
    if (!e) return true;
    if (dynamic_cast<const IntLitExpr*>(e))   return true;
    if (dynamic_cast<const FloatLitExpr*>(e)) return true;
    if (dynamic_cast<const BoolLitExpr*>(e))  return true;
    if (dynamic_cast<const NoneLitExpr*>(e))  return true;
    if (dynamic_cast<const DurationLitExpr*>(e)) return true;
    if (dynamic_cast<const StringLitExpr*>(e))   return true;
    if (dynamic_cast<const IdentExpr*>(e))    return true;
    if (auto* u = dynamic_cast<const UnaryExpr*>(e))  return expr_is_pure(u->operand.get());
    if (auto* b = dynamic_cast<const BinaryExpr*>(e))
        return expr_is_pure(b->left.get()) && expr_is_pure(b->right.get());
    if (auto* i = dynamic_cast<const IndexExpr*>(e))
        return expr_is_pure(i->object.get()) && expr_is_pure(i->index.get());
    if (auto* f = dynamic_cast<const FieldExpr*>(e)) return expr_is_pure(f->object.get());
    if (auto* t = dynamic_cast<const TernaryExpr*>(e))
        return expr_is_pure(t->cond.get()) && expr_is_pure(t->value.get())
            && expr_is_pure(t->else_val.get());
    return false;  // CallExpr, interpolation, constructors, etc.
}

int Compiler::compile_binary(const BinaryExpr* e, int dest)
{
    // Short-circuit for 'and'/'or'
    if (e->op == TokenKind::And) {
        int r = dest < 0 ? alloc_reg() : dest;
        compile_expr(e->left.get(), r);
        std::size_t jf = emit_jump(enc_RJ(Op::JF, uint8_t(r), 0));
        compile_expr(e->right.get(), r);
        patch_jump(jf);
        return r;
    }
    if (e->op == TokenKind::Or) {
        int r = dest < 0 ? alloc_reg() : dest;
        compile_expr(e->left.get(), r);
        std::size_t jt = emit_jump(enc_RJ(Op::JT, uint8_t(r), 0));
        compile_expr(e->right.get(), r);
        patch_jump(jt);
        return r;
    }

    // Allocate result first so it sits below the temps.
    int r = dest >= 0 ? dest : alloc_reg();
    // Operand registers: prefer reading locals in place (no MOVE-to-temp).
    // Left may read in place only if the right operand has no side effects
    // (right is evaluated after left, must not clobber left's variable).
    // Right may always read in place — it's evaluated last, matching semantics.
    bool lt = false, rt = false;
    int lhs, rhs;
    if (expr_is_pure(e->right.get())) {
        lhs = compile_operand(e->left.get(), lt);
    } else {
        lhs = alloc_reg(); compile_expr(e->left.get(), lhs); lt = true;
    }
    rhs = compile_operand(e->right.get(), rt);

    auto done = [&](int) { if (rt) free_reg(); if (lt) free_reg(); return r; };

    Op op;
    switch (e->op) {
    case TokenKind::Plus:       op = Op::ADD;  break;
    case TokenKind::Minus:      op = Op::SUB;  break;
    case TokenKind::Star:       op = Op::MUL;  break;
    case TokenKind::Slash:      op = Op::DIV;  break;
    case TokenKind::SlashSlash: op = Op::IDIV; break;
    case TokenKind::Percent:    op = Op::MOD;  break;
    case TokenKind::StarStar:   op = Op::POW;  break;
    case TokenKind::EqEq:       op = Op::EQ;   break;
    case TokenKind::BangEq:     op = Op::NEQ;  break;
    case TokenKind::Lt:         op = Op::LT;   break;
    case TokenKind::LtEq:       op = Op::LTE;  break;
    case TokenKind::Gt:
        emit(enc_R(Op::LT,  uint8_t(r), uint8_t(rhs), uint8_t(lhs))); return done(0);
    case TokenKind::GtEq:
        emit(enc_R(Op::LTE, uint8_t(r), uint8_t(rhs), uint8_t(lhs))); return done(0);
    case TokenKind::QQ:         op = Op::NULLC; break;
    case TokenKind::Is:         op = Op::EQ;   break;
    case TokenKind::Not:        op = Op::NEQ;  break;  // 'not in' / 'is not'
    // 'in': HAS_KEY(r, rhs=container, lhs=element) — note swapped operands
    case TokenKind::In:
        emit(enc_R(Op::HAS_KEY, uint8_t(r), uint8_t(rhs), uint8_t(lhs))); return done(0);
    default:
        emit(enc_I(Op::LOAD_NONE, uint8_t(r), 0)); return done(0);
    }
    emit(enc_R(op, uint8_t(r), uint8_t(lhs), uint8_t(rhs)));
    return done(0);
}

int Compiler::compile_unary(const UnaryExpr* e, int dest)
{
    int r = dest < 0 ? alloc_reg() : dest;
    int operand = alloc_reg();
    compile_expr(e->operand.get(), operand);
    switch (e->op) {
    case TokenKind::Minus: emit(enc_R(Op::UNM, uint8_t(r), uint8_t(operand))); break;
    case TokenKind::Not:   emit(enc_R(Op::NOT, uint8_t(r), uint8_t(operand))); break;
    default: emit(enc_R(Op::MOVE, uint8_t(r), uint8_t(operand))); break;
    }
    free_reg();
    return r;
}

int Compiler::compile_ternary(const TernaryExpr* e, int dest)
{
    // value if cond else else_val
    int r = dest < 0 ? alloc_reg() : dest;
    int cond_r = alloc_reg();
    compile_expr(e->cond.get(), cond_r);
    std::size_t jf = emit_jump(enc_RJ(Op::JF, uint8_t(cond_r), 0));
    free_reg();
    compile_expr(e->value.get(), r);
    std::size_t jend = emit_jump(enc_J(Op::JUMP, 0));
    patch_jump(jf);
    compile_expr(e->else_val.get(), r);
    patch_jump(jend);
    return r;
}

int Compiler::compile_call(const CallExpr* e, int dest)
{
    // Method call: obj.method(args) → INVOKE
    if (auto* field = dynamic_cast<const FieldExpr*>(e->callee.get())) {
        int obj_reg = alloc_reg();
        compile_expr(field->object.get(), obj_reg);
        int nargs = 0;
        for (auto& arg : e->args) {
            alloc_reg(); ++nargs;
            compile_expr(arg.value.get(), obj_reg + nargs);
        }
        MethodId mid = resolve_method_id(field->field);
        uint32_t key = (mid != MethodId::Unknown)
            ? uint32_t(mid)
            : (0x80000000u | uint32_t(add_str_const(field->field)));  // fallback: string const
        emit(enc_CALL(Op::INVOKE, uint8_t(obj_reg), uint8_t(nargs), 0, key));
        free_reg(nargs);
        if (dest >= 0 && dest != obj_reg)
            emit(enc_R(Op::MOVE, uint8_t(dest), uint8_t(obj_reg)));
        free_reg();
        return dest >= 0 ? dest : obj_reg;
    }

    // Layout: R[callee_reg]=callee, R[callee_reg+1..] = args.
    // After CALL, result sits in R[callee_reg].
    // We always free callee_reg after moving result to dest (or return it raw if dest=-1).
    int callee_reg = alloc_reg();
    compile_expr(e->callee.get(), callee_reg);

    int nargs = 0;
    for (auto& arg : e->args) {
        int ar = alloc_reg(); ++nargs;
        compile_expr(arg.value.get(), ar);
    }
    emit(enc_CALL(Op::CALL, uint8_t(callee_reg), uint8_t(nargs), 1));
    free_reg(nargs);  // free arg slots; reg_top = callee_reg + 1

    // result is in callee_reg; move to dest if different
    if (dest >= 0 && dest != callee_reg)
        emit(enc_R(Op::MOVE, uint8_t(dest), uint8_t(callee_reg)));
    free_reg();  // always free callee_reg slot; reg_top = callee_reg

    return dest >= 0 ? dest : callee_reg;
    // NOTE: when dest == -1, we return callee_reg but reg_top is already past it;
    // the result value is safe (not overwritten until caller alloc_reg again).
    // Caller must NOT call free_reg() for this result — it's already freed here.
    // Callers that use dest=-1 should treat the returned reg as a "borrowed" temp.
}

int Compiler::compile_index(const IndexExpr* e, int dest)
{
    int r = dest < 0 ? alloc_reg() : dest;
    bool ot = false, it = false;
    int obj;
    if (expr_is_pure(e->index.get())) {
        obj = compile_operand(e->object.get(), ot);
    } else {
        obj = alloc_reg(); compile_expr(e->object.get(), obj); ot = true;
    }
    int idx = compile_operand(e->index.get(), it);
    emit(enc_R(Op::GET_FIELD, uint8_t(r), uint8_t(obj), uint8_t(idx)));
    if (it) free_reg();
    if (ot) free_reg();
    return r;
}

int Compiler::compile_field(const FieldExpr* e, int dest)
{
    int r = dest < 0 ? alloc_reg() : dest;
    bool ot = false;
    int obj = compile_operand(e->object.get(), ot);  // object read-only → in place ok
    uint16_t ki = add_str_const(e->field);
    emit(enc_RI(Op::GET_FIELDK, uint8_t(r), uint8_t(obj), int64_t(ki)));
    if (ot) free_reg();
    return r;
}

int Compiler::compile_list(const ListExpr* e, int dest)
{
    int r = dest >= 0 ? dest : alloc_reg();
    int base = m_current->reg_top;
    int n = int(e->elements.size());
    for (auto& el : e->elements) {
        int er = alloc_reg();
        compile_expr(el.get(), er);
    }
    emit(enc_R(Op::NEW_LIST, uint8_t(r), uint8_t(base), uint8_t(n)));
    free_reg(n);
    return r;
}

int Compiler::compile_map(const MapExpr* e, int dest)
{
    int r = dest < 0 ? alloc_reg() : dest;
    emit(enc_R(Op::NEW_MAP, uint8_t(r), 0));
    for (auto& pair : e->pairs) {
        int kr = alloc_reg();
        compile_expr(pair.key.get(), kr);
        int vr = alloc_reg();
        compile_expr(pair.value.get(), vr);
        emit(enc_R(Op::SET_FIELD, uint8_t(vr), uint8_t(r), uint8_t(kr)));
        free_reg(); free_reg();
    }
    return r;
}

int Compiler::compile_tuple(const TupleExpr* e, int dest)
{
    int r = dest >= 0 ? dest : alloc_reg();
    int base = m_current->reg_top;
    int n = int(e->elements.size());
    for (auto& el : e->elements) {
        int er = alloc_reg();
        compile_expr(el.get(), er);
    }
    emit(enc_R(Op::NEW_TUPLE, uint8_t(r), uint8_t(base), uint8_t(n)));
    free_reg(n);
    return r;
}

int Compiler::compile_fn_expr(const FnExpr* e, int dest)
{
    std::vector<std::string> param_names;
    for (auto& p : e->params) param_names.push_back(p.name);

    std::vector<UpvalueInfo> inner_upvalues;
    ObjFunction* fn = compile_function("<fn>", int(e->params.size()),
                                       param_names, e->body, &inner_upvalues);

    uint16_t ki = add_const(Value::from_ptr(fn));
    int r = dest < 0 ? alloc_reg() : dest;
    emit(enc_RI(Op::CLOSURE, uint8_t(r), 0, int64_t(ki)));
    for (auto& uv : inner_upvalues)
        emit(enc_R(Op::NOP, uint8_t(uv.is_local ? 1 : 0), uint8_t(uv.index)));
    return r;
}

int Compiler::compile_match_expr(const MatchExpr* e, int dest)
{
    int subj = alloc_reg();
    compile_expr(e->subject.get(), subj);
    int r = dest < 0 ? alloc_reg() : dest;
    emit(enc_I(Op::LOAD_NONE, uint8_t(r), 0));

    PatchList end_jumps;
    for (auto& arm : e->arms) {
        // ponytail: only literal/wildcard patterns supported for benchmarks
        bool always_match = arm.pattern.kind == PatternKind::Wildcard;
        std::size_t jf_idx = 0;
        bool has_cond = false;

        if (!always_match && arm.pattern.kind == PatternKind::Literal) {
            int pat_r = alloc_reg();
            switch (arm.pattern.lit_kind) {
            case TokenKind::Int:
                emit(enc_I(Op::LOAD_INT, uint8_t(pat_r), arm.pattern.int_val));
                break;
            case TokenKind::True:
                emit(enc_I(Op::LOAD_TRUE, uint8_t(pat_r), 0));
                break;
            case TokenKind::False:
                emit(enc_I(Op::LOAD_FALSE, uint8_t(pat_r), 0));
                break;
            default:
                emit(enc_I(Op::LOAD_NONE, uint8_t(pat_r), 0));
                break;
            }
            int cmp = alloc_reg();
            emit(enc_R(Op::EQ, uint8_t(cmp), uint8_t(subj), uint8_t(pat_r)));
            jf_idx = emit_jump(enc_RJ(Op::JF, uint8_t(cmp), 0));
            free_reg(); free_reg();
            has_cond = true;
        }

        begin_scope();
        if (arm.pattern.kind == PatternKind::Capture)
            declare_local(arm.pattern.name);
        compile_block(arm.body);
        // result of last expr? For stmt-style match arms, r stays none
        end_scope();
        end_jumps.jumps.push_back(emit_jump(enc_J(Op::JUMP, 0)));
        if (has_cond) patch_jump(jf_idx);
    }

    if (!e->else_body.empty()) {
        begin_scope(); compile_block(e->else_body); end_scope();
    }

    patch_list(end_jumps);
    free_reg(); // subj
    return r;
}

// ── Register management ───────────────────────────────────────────────────────

int Compiler::alloc_reg()
{
    return m_current->reg_top++;
}

int Compiler::compile_operand(const ExprNode* e, bool& is_temp)
{
    // Local variable: read its register directly — skip the redundant MOVE-to-temp.
    if (auto* id = dynamic_cast<const IdentExpr*>(e)) {
        int local = resolve_local(id->name);
        if (local >= 0) { is_temp = false; return local; }
    }
    int t = alloc_reg();
    compile_expr(e, t);
    is_temp = true;
    return t;
}

void Compiler::free_reg(int n)
{
    m_current->reg_top -= n;
}

int Compiler::reg_top() const { return m_current->reg_top; }

// ── Scope management ──────────────────────────────────────────────────────────

void Compiler::begin_scope() { ++m_current->scope_depth; }

void Compiler::end_scope()
{
    --m_current->scope_depth;
    // pop locals from this scope
    while (!m_current->locals.empty() &&
           m_current->locals.back().depth > m_current->scope_depth)
    {
        Local& loc = m_current->locals.back();
        if (loc.is_captured)
            emit(enc_R(Op::CLOSE_UPVAL, uint8_t(loc.slot), 0));
        // free the register
        m_current->reg_top = loc.slot;
        m_current->locals.pop_back();
    }
}

int Compiler::declare_local(const std::string& name)
{
    int slot = m_current->reg_top++;
    Local loc;
    loc.name  = name;
    loc.slot  = slot;
    loc.depth = -1; // uninitialized
    m_current->locals.push_back(loc);
    return slot;
}

void Compiler::mark_initialized(int slot)
{
    for (auto& l : m_current->locals) {
        if (l.slot == slot) { l.depth = m_current->scope_depth; return; }
    }
}

int Compiler::resolve_local(const std::string& name) const
{
    for (int i = int(m_current->locals.size()) - 1; i >= 0; --i) {
        if (m_current->locals[i].name == name && m_current->locals[i].depth >= 0)
            return m_current->locals[i].slot;
    }
    return -1;
}

int Compiler::resolve_upvalue(const std::string& name)
{
    if (!m_current->enclosing) return -1;

    FnState* saved = m_current;
    m_current = m_current->enclosing;
    int local = resolve_local(name);
    m_current = saved;

    if (local >= 0) {
        // mark it as captured
        for (auto& l : m_current->enclosing->locals) {
            if (l.slot == local) { l.is_captured = true; break; }
        }
        return add_upvalue(true, local);
    }

    // Walk one level up: check if enclosing captures name as upvalue
    FnState* saved2 = m_current;
    m_current = m_current->enclosing;
    int upval = resolve_upvalue(name);
    m_current = saved2;
    if (upval >= 0) return add_upvalue(false, upval);
    return -1;
}

int Compiler::add_upvalue(bool is_local, int idx)
{
    for (int i = 0; i < int(m_current->upvalues.size()); ++i) {
        auto& uv = m_current->upvalues[i];
        if (uv.is_local == is_local && uv.index == idx) return i;
    }
    m_current->upvalues.push_back({is_local, idx});
    return int(m_current->upvalues.size()) - 1;
}

// ── Emit helpers ──────────────────────────────────────────────────────────────

Chunk& Compiler::chunk() { return *m_current->fn->chunk; }

void Compiler::emit(uint64_t instr, uint32_t line)
{
    chunk().emit(instr, line);
}

std::size_t Compiler::emit_jump(uint64_t instr, uint32_t line)
{
    return chunk().emit(instr, line);
}

void Compiler::patch_jump(std::size_t jmp_idx)
{
    chunk().patch_jump(jmp_idx);
}

void Compiler::patch_list(PatchList& pl)
{
    for (auto idx : pl.jumps) patch_jump(idx);
    pl.jumps.clear();
}

uint16_t Compiler::add_const(Value v)
{
    return chunk().add_constant(v);
}

uint16_t Compiler::add_str_const(const std::string& s)
{
    return chunk().add_string(s);
}

void Compiler::move_to(int src, int dst)
{
    if (src != dst) emit(enc_R(Op::MOVE, uint8_t(dst), uint8_t(src)));
}

int Compiler::ensure_reg(int result, int dest)
{
    if (result < 0) return dest;
    if (result != dest) move_to(result, dest);
    return dest;
}

// ── Lvalue assignment ─────────────────────────────────────────────────────────

void Compiler::assign_to(const ExprNode* lval, int src_reg)
{
    if (auto* e = dynamic_cast<const IdentExpr*>(lval)) {
        int local = resolve_local(e->name);
        if (local >= 0) {
            if (local != src_reg) emit(enc_R(Op::MOVE, uint8_t(local), uint8_t(src_reg)));
            return;
        }
        int upval = resolve_upvalue(e->name);
        if (upval >= 0) {
            emit(enc_R(Op::SET_UPVAL, uint8_t(src_reg), uint8_t(upval)));
            return;
        }
        uint16_t ki = add_str_const(e->name);
        emit(enc_I(Op::SET_GLOBAL, uint8_t(src_reg), int64_t(ki)));
        return;
    }
    if (auto* e = dynamic_cast<const IndexExpr*>(lval)) {
        int obj = alloc_reg();
        compile_expr(e->object.get(), obj);
        int idx = alloc_reg();
        compile_expr(e->index.get(), idx);
        emit(enc_R(Op::SET_FIELD, uint8_t(src_reg), uint8_t(obj), uint8_t(idx)));
        free_reg(); free_reg();
        return;
    }
    if (auto* e = dynamic_cast<const FieldExpr*>(lval)) {
        int obj = alloc_reg();
        compile_expr(e->object.get(), obj);
        uint16_t ki = add_str_const(e->field);
        emit(enc_RI(Op::SET_FIELDK, uint8_t(src_reg), uint8_t(obj), int64_t(ki)));
        free_reg();
        return;
    }
}

} // namespace syn
