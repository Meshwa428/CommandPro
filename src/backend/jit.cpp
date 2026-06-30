// AST → C transpiler JIT.
// Handles named pure-int/float functions AND top-level code (__main__).
// Compiles with gcc -O3, caches .so by source hash, dlopen at runtime.

#include "synapse/backend/jit.h"
#include "synapse/frontend/ast.h"
#include "synapse/frontend/token.h"

#include <dlfcn.h>
#include <sys/stat.h>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <optional>

namespace syn {

// ── Type inference ────────────────────────────────────────────────────────────

using VarSet = std::unordered_set<std::string>;

// ── Specialization context (fn param → concrete fn alias, for higher-order call sites) ──────────
struct SpecCtx {
    // Body context: param_name → concrete ifns function name (e.g., "f" → "square")
    std::unordered_map<std::string, std::string> aliases;
    // Call site context: spec_fn_name → index of the function-type parameter
    std::unordered_map<std::string, size_t> fn_params;
};
static const SpecCtx g_empty_ctx;  // default for callers that don't need specialization

// ---- Int inference ----

static bool is_int_expr(const ExprNode* e, const VarSet& ivars, const VarSet& ifns,
                         const SpecCtx& ctx = g_empty_ctx);

static bool is_int_cond(const ExprNode* e, const VarSet& ivars, const VarSet& ifns,
                         const SpecCtx& ctx = g_empty_ctx)
{
    if (!e) return false;
    if (dynamic_cast<const BoolLitExpr*>(e)) return true;
    if (dynamic_cast<const NoneLitExpr*>(e)) return true;
    if (auto* n = dynamic_cast<const BinaryExpr*>(e)) {
        using TK = TokenKind;
        switch (n->op) {
        case TK::EqEq: case TK::BangEq:
        case TK::Lt:   case TK::LtEq:
        case TK::Gt:   case TK::GtEq:
            return is_int_expr(n->left.get(), ivars, ifns, ctx) &&
                   is_int_expr(n->right.get(), ivars, ifns, ctx);
        case TK::And: case TK::Or:
            return is_int_cond(n->left.get(), ivars, ifns, ctx) &&
                   is_int_cond(n->right.get(), ivars, ifns, ctx);
        default: break;
        }
    }
    if (auto* n = dynamic_cast<const UnaryExpr*>(e))
        if (n->op == TokenKind::Not) return is_int_cond(n->operand.get(), ivars, ifns, ctx);
    return is_int_expr(e, ivars, ifns, ctx);
}

static bool is_int_expr(const ExprNode* e, const VarSet& ivars, const VarSet& ifns,
                         const SpecCtx& ctx)
{
    if (!e) return false;
    if (dynamic_cast<const IntLitExpr*>(e) || dynamic_cast<const BoolLitExpr*>(e)) return true;
    if (auto* n = dynamic_cast<const IdentExpr*>(e)) return ivars.count(n->name) > 0;
    if (auto* n = dynamic_cast<const BinaryExpr*>(e)) {
        using TK = TokenKind;
        switch (n->op) {
        case TK::Plus: case TK::Minus: case TK::Star:
        case TK::SlashSlash: case TK::Percent: case TK::StarStar:
            return is_int_expr(n->left.get(), ivars, ifns, ctx) &&
                   is_int_expr(n->right.get(), ivars, ifns, ctx);
        default: return false;
        }
    }
    if (auto* n = dynamic_cast<const UnaryExpr*>(e))
        if (n->op == TokenKind::Minus) return is_int_expr(n->operand.get(), ivars, ifns, ctx);
    if (auto* n = dynamic_cast<const CallExpr*>(e)) {
        if (auto* id = dynamic_cast<const IdentExpr*>(n->callee.get())) {
            // Aliased call (fn param → concrete fn): treat as int call
            if (ctx.aliases.count(id->name)) {
                for (auto& a : n->args)
                    if (!is_int_expr(a.value.get(), ivars, ifns, ctx)) return false;
                return true;
            }
            // Regular ifns call
            if (ifns.count(id->name) > 0) {
                for (size_t k = 0; k < n->args.size(); k++) {
                    // Skip the function-type arg position for spec fns
                    if (ctx.fn_params.count(id->name) && ctx.fn_params.at(id->name) == k) continue;
                    if (!is_int_expr(n->args[k].value.get(), ivars, ifns, ctx)) return false;
                }
                return true;
            }
        }
    }
    return false;
}

static bool check_int_block(const Block& b, VarSet ivars, const VarSet& ifns, bool for_main = false,
                             const SpecCtx& ctx = g_empty_ctx);

static bool check_int_stmt(const StmtNode* s, VarSet& ivars, const VarSet& ifns, bool for_main = false,
                             const SpecCtx& ctx = g_empty_ctx)
{
    if (dynamic_cast<const FnDeclStmt*>(s)) return true;  // skip fn defs in block context
    if (auto* n = dynamic_cast<const LetStmt*>(s)) {
        if (n->bind_kind != LetBindKind::Simple || n->values.size() != 1) return false;
        if (is_int_expr(n->values[0].get(), ivars, ifns, ctx))
            for (auto& nm : n->names) ivars.insert(nm);
        return true;
    }
    if (auto* n = dynamic_cast<const AssignStmt*>(s)) {
        if (n->lvalues.size() != 1 || n->values.size() != 1) return false;
        auto* lv = dynamic_cast<const IdentExpr*>(n->lvalues[0].get());
        if (!lv) return false;
        if (!is_int_expr(n->values[0].get(), ivars, ifns, ctx)) return false;
        ivars.insert(lv->name);
        return true;
    }
    if (dynamic_cast<const AugAssignStmt*>(s)) return true;
    if (auto* n = dynamic_cast<const IfStmt*>(s)) {
        for (auto& br : n->branches) {
            if (br.cond && !is_int_cond(br.cond.get(), ivars, ifns, ctx)) return false;
            if (!check_int_block(br.body, ivars, ifns, for_main, ctx)) return false;
        }
        return true;
    }
    if (auto* n = dynamic_cast<const WhileStmt*>(s)) {
        if (!is_int_cond(n->cond.get(), ivars, ifns, ctx)) return false;
        return check_int_block(n->body, ivars, ifns, for_main, ctx);
    }
    if (auto* n = dynamic_cast<const ForStmt*>(s)) {
        if (!n->range_end) return false;
        if (!is_int_expr(n->source.get(), ivars, ifns, ctx)) return false;
        if (!is_int_expr(n->range_end.get(), ivars, ifns, ctx)) return false;
        if (n->range_step && !is_int_expr(n->range_step.get(), ivars, ifns, ctx)) return false;
        VarSet lv = ivars; lv.insert(n->iter1);
        return check_int_block(n->body, lv, ifns, for_main, ctx);
    }
    if (auto* n = dynamic_cast<const ReturnStmt*>(s)) {
        if (n->values.empty()) return false;
        return n->values.size() == 1 && is_int_expr(n->values[0].get(), ivars, ifns, ctx);
    }
    if (auto* n = dynamic_cast<const ExprStmt*>(s)) {
        if (!for_main) return true;  // inside named fn: tolerate side effects
        // for __main__: only allow print(int_args) or call to int fn
        if (auto* ce = dynamic_cast<const CallExpr*>(n->expr.get())) {
            if (auto* id = dynamic_cast<const IdentExpr*>(ce->callee.get())) {
                if (id->name == "print") {
                    for (auto& arg : ce->args)
                        if (!is_int_expr(arg.value.get(), ivars, ifns, ctx)) return false;
                    return true;
                }
                if (ifns.count(id->name)) return true;
            }
        }
        return false;
    }
    if (dynamic_cast<const BreakStmt*>(s) || dynamic_cast<const ContinueStmt*>(s)) return true;
    return false;
}

static bool check_int_block(const Block& b, VarSet ivars, const VarSet& ifns, bool for_main,
                             const SpecCtx& ctx)
{
    for (auto& s : b) if (!check_int_stmt(s.get(), ivars, ifns, for_main, ctx)) return false;
    return true;
}

static VarSet find_int_fns(const std::vector<const FnDeclStmt*>& fns)
{
    VarSet result;
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto* fn : fns) {
            if (result.count(fn->name)) continue;
            VarSet params;
            for (auto& p : fn->params) params.insert(p.name);
            VarSet candidate = result; candidate.insert(fn->name);
            if (check_int_block(fn->body, params, candidate, false)) {
                result.insert(fn->name); changed = true;
            }
        }
    }
    return result;
}

// ---- Float inference ----

static bool is_float_expr(const ExprNode* e, const VarSet& fvars, const VarSet& ffns);

static bool is_float_cond(const ExprNode* e, const VarSet& fvars, const VarSet& ffns)
{
    if (!e) return false;
    if (dynamic_cast<const BoolLitExpr*>(e)) return true;
    if (auto* n = dynamic_cast<const BinaryExpr*>(e)) {
        using TK = TokenKind;
        switch (n->op) {
        case TK::EqEq: case TK::BangEq:
        case TK::Lt:   case TK::LtEq:
        case TK::Gt:   case TK::GtEq:
            return is_float_expr(n->left.get(), fvars, ffns) &&
                   is_float_expr(n->right.get(), fvars, ffns);
        case TK::And: case TK::Or:
            return is_float_cond(n->left.get(), fvars, ffns) &&
                   is_float_cond(n->right.get(), fvars, ffns);
        default: break;
        }
    }
    if (auto* n = dynamic_cast<const UnaryExpr*>(e))
        if (n->op == TokenKind::Not) return is_float_cond(n->operand.get(), fvars, ffns);
    return is_float_expr(e, fvars, ffns);
}

static bool is_float_expr(const ExprNode* e, const VarSet& fvars, const VarSet& ffns)
{
    if (!e) return false;
    if (dynamic_cast<const FloatLitExpr*>(e)) return true;
    if (dynamic_cast<const IntLitExpr*>(e)) return true;  // int lits promote to float
    if (auto* n = dynamic_cast<const IdentExpr*>(e)) return fvars.count(n->name) > 0;
    if (auto* n = dynamic_cast<const BinaryExpr*>(e)) {
        using TK = TokenKind;
        switch (n->op) {
        case TK::Plus: case TK::Minus: case TK::Star: case TK::Slash:
            return is_float_expr(n->left.get(), fvars, ffns) &&
                   is_float_expr(n->right.get(), fvars, ffns);
        default: return false;
        }
    }
    if (auto* n = dynamic_cast<const UnaryExpr*>(e))
        if (n->op == TokenKind::Minus) return is_float_expr(n->operand.get(), fvars, ffns);
    if (auto* n = dynamic_cast<const CallExpr*>(e))
        if (auto* id = dynamic_cast<const IdentExpr*>(n->callee.get()))
            if (ffns.count(id->name) > 0) {
                for (auto& a : n->args)
                    if (!is_float_expr(a.value.get(), fvars, ffns)) return false;
                return true;
            }
    return false;
}

static bool check_float_block(const Block& b, VarSet fvars, const VarSet& ffns, bool for_main = false);

static bool check_float_stmt(const StmtNode* s, VarSet& fvars, const VarSet& ffns, bool for_main = false)
{
    if (dynamic_cast<const FnDeclStmt*>(s)) return true;
    if (auto* n = dynamic_cast<const LetStmt*>(s)) {
        if (n->bind_kind != LetBindKind::Simple || n->values.size() != 1) return false;
        if (is_float_expr(n->values[0].get(), fvars, ffns))
            for (auto& nm : n->names) fvars.insert(nm);
        return true;
    }
    if (auto* n = dynamic_cast<const AssignStmt*>(s)) {
        if (n->lvalues.size() != 1 || n->values.size() != 1) return false;
        auto* lv = dynamic_cast<const IdentExpr*>(n->lvalues[0].get());
        if (!lv) return false;
        if (!is_float_expr(n->values[0].get(), fvars, ffns)) return false;  // strict
        fvars.insert(lv->name);
        return true;
    }
    if (dynamic_cast<const AugAssignStmt*>(s)) return true;
    if (auto* n = dynamic_cast<const IfStmt*>(s)) {
        for (auto& br : n->branches) {
            if (br.cond && !is_float_cond(br.cond.get(), fvars, ffns)) return false;
            if (!check_float_block(br.body, fvars, ffns, for_main)) return false;
        }
        return true;
    }
    if (auto* n = dynamic_cast<const WhileStmt*>(s)) {
        if (!is_float_cond(n->cond.get(), fvars, ffns)) return false;
        return check_float_block(n->body, fvars, ffns, for_main);
    }
    if (auto* n = dynamic_cast<const ForStmt*>(s)) {
        if (!n->range_end) return false;
        if (!is_float_expr(n->source.get(), fvars, ffns)) return false;
        if (!is_float_expr(n->range_end.get(), fvars, ffns)) return false;
        VarSet lv = fvars; lv.insert(n->iter1);
        return check_float_block(n->body, lv, ffns, for_main);
    }
    if (auto* n = dynamic_cast<const ReturnStmt*>(s)) {
        if (n->values.empty()) return false;
        return n->values.size() == 1 && is_float_expr(n->values[0].get(), fvars, ffns);
    }
    if (auto* n = dynamic_cast<const ExprStmt*>(s)) {
        if (!for_main) return true;
        if (auto* ce = dynamic_cast<const CallExpr*>(n->expr.get())) {
            if (auto* id = dynamic_cast<const IdentExpr*>(ce->callee.get())) {
                if (id->name == "print") {
                    for (auto& arg : ce->args)
                        if (!is_float_expr(arg.value.get(), fvars, ffns)) return false;
                    return true;
                }
                if (ffns.count(id->name)) return true;
            }
        }
        return false;
    }
    if (dynamic_cast<const BreakStmt*>(s) || dynamic_cast<const ContinueStmt*>(s)) return true;
    return false;
}

static bool check_float_block(const Block& b, VarSet fvars, const VarSet& ffns, bool for_main)
{
    for (auto& s : b) if (!check_float_stmt(s.get(), fvars, ffns, for_main)) return false;
    return true;
}

static VarSet find_float_fns(const std::vector<const FnDeclStmt*>& fns)
{
    VarSet result;
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto* fn : fns) {
            if (result.count(fn->name)) continue;
            VarSet params;
            for (auto& p : fn->params) params.insert(p.name);
            VarSet candidate = result; candidate.insert(fn->name);
            if (check_float_block(fn->body, params, candidate, false)) {
                result.insert(fn->name); changed = true;
            }
        }
    }
    return result;
}

// ── Mixed int+float type inference ───────────────────────────────────────────
// false = int64_t, true = double.  std::nullopt = cannot determine type.

using VTMap = std::unordered_map<std::string, bool>;

static const std::unordered_set<std::string> g_math_ffns = {
    "sqrt","floor","ceil","sin","cos","exp","log","abs","pow"
};

static std::optional<bool> infer_type(const ExprNode* e, const VTMap& vt,
                                       const VarSet& ifns, const VarSet& ffns)
{
    if (!e) return std::nullopt;
    if (dynamic_cast<const IntLitExpr*>(e) || dynamic_cast<const BoolLitExpr*>(e))
        return false;  // INT
    if (dynamic_cast<const FloatLitExpr*>(e)) return true;  // FLOAT
    if (auto* n = dynamic_cast<const IdentExpr*>(e)) {
        auto it = vt.find(n->name);
        return it == vt.end() ? std::optional<bool>{} : std::optional<bool>{it->second};
    }
    if (auto* n = dynamic_cast<const UnaryExpr*>(e)) {
        if (n->op == TokenKind::Minus || n->op == TokenKind::Not)
            return infer_type(n->operand.get(), vt, ifns, ffns);
        return std::nullopt;
    }
    if (auto* n = dynamic_cast<const BinaryExpr*>(e)) {
        using TK = TokenKind;
        if (n->op == TK::Slash) {
            auto l = infer_type(n->left.get(), vt, ifns, ffns);
            auto r = infer_type(n->right.get(), vt, ifns, ffns);
            return (l && r) ? std::optional<bool>{true} : std::nullopt;
        }
        if (n->op == TK::SlashSlash || n->op == TK::Percent || n->op == TK::StarStar) {
            auto l = infer_type(n->left.get(), vt, ifns, ffns);
            auto r = infer_type(n->right.get(), vt, ifns, ffns);
            if (!l || !r || *l || *r) return std::nullopt;  // require both INT
            return false;
        }
        // +, -, *: FLOAT if either operand is FLOAT
        auto l = infer_type(n->left.get(), vt, ifns, ffns);
        auto r = infer_type(n->right.get(), vt, ifns, ffns);
        if (!l || !r) return std::nullopt;
        return *l || *r;
    }
    if (auto* n = dynamic_cast<const CallExpr*>(e)) {
        auto* id = dynamic_cast<const IdentExpr*>(n->callee.get());
        if (!id) return std::nullopt;
        if (g_math_ffns.count(id->name)) return true;
        if (id->name == "len") return false;
        if (ifns.count(id->name)) return false;
        if (ffns.count(id->name)) return true;
        return std::nullopt;
    }
    return std::nullopt;
}

static bool is_mixed_cond(const ExprNode* e, const VTMap& vt,
                           const VarSet& ifns, const VarSet& ffns)
{
    if (!e) return false;
    if (dynamic_cast<const BoolLitExpr*>(e) || dynamic_cast<const NoneLitExpr*>(e)) return true;
    if (auto* n = dynamic_cast<const BinaryExpr*>(e)) {
        using TK = TokenKind;
        switch (n->op) {
        case TK::EqEq: case TK::BangEq:
        case TK::Lt:   case TK::LtEq:
        case TK::Gt:   case TK::GtEq: {
            auto l = infer_type(n->left.get(), vt, ifns, ffns);
            auto r = infer_type(n->right.get(), vt, ifns, ffns);
            return l.has_value() && r.has_value();
        }
        case TK::And: case TK::Or:
            return is_mixed_cond(n->left.get(), vt, ifns, ffns) &&
                   is_mixed_cond(n->right.get(), vt, ifns, ffns);
        default: break;
        }
    }
    if (auto* n = dynamic_cast<const UnaryExpr*>(e))
        if (n->op == TokenKind::Not) return is_mixed_cond(n->operand.get(), vt, ifns, ffns);
    return infer_type(e, vt, ifns, ffns).has_value();
}

static bool check_mixed_block(const Block& b, VTMap& vt, const VarSet& ifns,
                               const VarSet& ffns, bool for_main = false);

static bool check_mixed_stmt(const StmtNode* s, VTMap& vt,
                              const VarSet& ifns, const VarSet& ffns, bool for_main)
{
    if (dynamic_cast<const FnDeclStmt*>(s)) return true;
    if (auto* n = dynamic_cast<const LetStmt*>(s)) {
        if (n->bind_kind != LetBindKind::Simple || n->values.size() != 1) return false;
        auto t = infer_type(n->values[0].get(), vt, ifns, ffns);
        if (!t) return false;
        for (auto& nm : n->names) vt[nm] = *t;
        return true;
    }
    if (auto* n = dynamic_cast<const AssignStmt*>(s)) {
        if (n->lvalues.size() != 1 || n->values.size() != 1) return false;
        auto* lv = dynamic_cast<const IdentExpr*>(n->lvalues[0].get());
        if (!lv || !vt.count(lv->name)) return false;
        auto t = infer_type(n->values[0].get(), vt, ifns, ffns);
        if (!t) return false;
        if (!vt[lv->name] && *t) vt[lv->name] = true;  // int → float widening
        return true;
    }
    if (auto* n = dynamic_cast<const AugAssignStmt*>(s)) {
        auto* lv = dynamic_cast<const IdentExpr*>(n->lvalue.get());
        if (!lv || !vt.count(lv->name)) return false;
        return infer_type(n->value.get(), vt, ifns, ffns).has_value();
    }
    if (auto* n = dynamic_cast<const IfStmt*>(s)) {
        for (auto& br : n->branches) {
            if (br.cond && !is_mixed_cond(br.cond.get(), vt, ifns, ffns)) return false;
            VTMap local = vt;
            if (!check_mixed_block(br.body, local, ifns, ffns, for_main)) return false;
            for (auto& [nm, t] : local)
                if (!vt.count(nm)) vt[nm] = t;
                else if (!vt[nm] && t) vt[nm] = true;
        }
        return true;
    }
    if (auto* n = dynamic_cast<const WhileStmt*>(s)) {
        if (!is_mixed_cond(n->cond.get(), vt, ifns, ffns)) return false;
        VTMap local = vt;
        if (!check_mixed_block(n->body, local, ifns, ffns, for_main)) return false;
        for (auto& [nm, t] : local)
            if (!vt.count(nm)) vt[nm] = t;
            else if (!vt[nm] && t) vt[nm] = true;
        return true;
    }
    if (auto* n = dynamic_cast<const ForStmt*>(s)) {
        if (!n->range_end) return false;
        auto st = infer_type(n->source.get(), vt, ifns, ffns);
        auto et = infer_type(n->range_end.get(), vt, ifns, ffns);
        if (!st || *st || !et || *et) return false;  // start/end must be INT
        if (n->range_step) {
            auto ss = infer_type(n->range_step.get(), vt, ifns, ffns);
            if (!ss || *ss) return false;
        }
        VTMap local = vt; local[n->iter1] = false;  // iter is INT
        if (!check_mixed_block(n->body, local, ifns, ffns, for_main)) return false;
        for (auto& [nm, t] : local) {
            if (nm == n->iter1) continue;
            if (!vt.count(nm)) vt[nm] = t;
            else if (!vt[nm] && t) vt[nm] = true;
        }
        return true;
    }
    if (auto* n = dynamic_cast<const ReturnStmt*>(s)) {
        if (n->values.size() != 1) return false;
        return infer_type(n->values[0].get(), vt, ifns, ffns).has_value();
    }
    if (auto* n = dynamic_cast<const ExprStmt*>(s)) {
        if (!for_main) return true;
        if (auto* ce = dynamic_cast<const CallExpr*>(n->expr.get())) {
            if (auto* id = dynamic_cast<const IdentExpr*>(ce->callee.get())) {
                if (id->name == "print") {
                    for (auto& arg : ce->args)
                        if (!infer_type(arg.value.get(), vt, ifns, ffns)) return false;
                    return true;
                }
                if (ifns.count(id->name) || ffns.count(id->name)) return true;
            }
        }
        return false;
    }
    if (dynamic_cast<const BreakStmt*>(s) || dynamic_cast<const ContinueStmt*>(s)) return true;
    return false;
}

static bool check_mixed_block(const Block& b, VTMap& vt, const VarSet& ifns,
                               const VarSet& ffns, bool for_main)
{
    for (auto& s : b) if (!check_mixed_stmt(s.get(), vt, ifns, ffns, for_main)) return false;
    return true;
}

// ── C code generation ─────────────────────────────────────────────────────────

// ---- Int emitters ----

static void emit_int_expr(const ExprNode* e, std::ostream& o, const VarSet& ifns,
                           const SpecCtx& ctx = g_empty_ctx)
{
    if (auto* n = dynamic_cast<const IntLitExpr*>(e)) { o << "(" << n->value << "LL)"; return; }
    if (auto* n = dynamic_cast<const BoolLitExpr*>(e)) { o << (n->value ? "1LL" : "0LL"); return; }
    if (auto* n = dynamic_cast<const IdentExpr*>(e)) { o << "_v_" << n->name; return; }
    if (auto* n = dynamic_cast<const UnaryExpr*>(e)) {
        o << "(-"; emit_int_expr(n->operand.get(), o, ifns, ctx); o << ")"; return;
    }
    if (auto* n = dynamic_cast<const BinaryExpr*>(e)) {
        using TK = TokenKind;
        if (n->op == TK::SlashSlash) {
            o << "syn_idiv("; emit_int_expr(n->left.get(), o, ifns, ctx);
            o << ","; emit_int_expr(n->right.get(), o, ifns, ctx); o << ")"; return;
        }
        if (n->op == TK::Percent) {
            o << "syn_imod("; emit_int_expr(n->left.get(), o, ifns, ctx);
            o << ","; emit_int_expr(n->right.get(), o, ifns, ctx); o << ")"; return;
        }
        if (n->op == TK::StarStar) {
            o << "syn_ipow("; emit_int_expr(n->left.get(), o, ifns, ctx);
            o << ","; emit_int_expr(n->right.get(), o, ifns, ctx); o << ")"; return;
        }
        const char* op = nullptr;
        switch (n->op) {
        case TK::Plus: op="+"; break; case TK::Minus: op="-"; break;
        case TK::Star: op="*"; break;
        // comparisons yield 0/1 in C — valid int-valued result
        case TK::Lt: op="<"; break;   case TK::LtEq: op="<="; break;
        case TK::Gt: op=">"; break;   case TK::GtEq: op=">="; break;
        case TK::EqEq: op="=="; break; case TK::BangEq: op="!="; break;
        default: o << "(0LL)"; return;
        }
        o << "("; emit_int_expr(n->left.get(), o, ifns, ctx);
        o << op; emit_int_expr(n->right.get(), o, ifns, ctx); o << ")"; return;
    }
    if (auto* n = dynamic_cast<const CallExpr*>(e)) {
        if (auto* id = dynamic_cast<const IdentExpr*>(n->callee.get())) {
            // Aliased call (fn param → concrete fn): resolve to concrete fn name
            if (ctx.aliases.count(id->name)) {
                o << "_jit_i_" << ctx.aliases.at(id->name) << "(";
                for (size_t i = 0; i < n->args.size(); ++i) {
                    if (i) o << ","; emit_int_expr(n->args[i].value.get(), o, ifns, ctx);
                }
                o << ")"; return;
            }
            if (ifns.count(id->name)) {
                o << "_jit_i_" << id->name << "(";
                for (size_t k = 0; k < n->args.size(); ++k) {
                    if (k) o << ",";
                    // For spec fns: pass 0LL for the function-type arg
                    if (ctx.fn_params.count(id->name) && ctx.fn_params.at(id->name) == k)
                        o << "0LL";
                    else
                        emit_int_expr(n->args[k].value.get(), o, ifns, ctx);
                }
                o << ")"; return;
            }
        }
    }
    o << "(0LL)";
}

static void emit_int_cond(const ExprNode* e, std::ostream& o, const VarSet& ifns,
                           const SpecCtx& ctx = g_empty_ctx)
{
    if (auto* n = dynamic_cast<const BoolLitExpr*>(e)) { o << (n->value?"1":"0"); return; }
    if (dynamic_cast<const NoneLitExpr*>(e)) { o << "0"; return; }
    if (auto* n = dynamic_cast<const BinaryExpr*>(e)) {
        using TK = TokenKind;
        const char* op = nullptr;
        switch (n->op) {
        case TK::EqEq: op="=="; break; case TK::BangEq: op="!="; break;
        case TK::Lt:   op="<";  break; case TK::LtEq:   op="<="; break;
        case TK::Gt:   op=">";  break; case TK::GtEq:   op=">="; break;
        case TK::And:
            o << "("; emit_int_cond(n->left.get(), o, ifns, ctx);
            o << "&&"; emit_int_cond(n->right.get(), o, ifns, ctx); o << ")"; return;
        case TK::Or:
            o << "("; emit_int_cond(n->left.get(), o, ifns, ctx);
            o << "||"; emit_int_cond(n->right.get(), o, ifns, ctx); o << ")"; return;
        default: break;
        }
        if (op) {
            o << "("; emit_int_expr(n->left.get(), o, ifns, ctx);
            o << op; emit_int_expr(n->right.get(), o, ifns, ctx); o << ")"; return;
        }
    }
    if (auto* n = dynamic_cast<const UnaryExpr*>(e)) {
        if (n->op == TokenKind::Not) {
            o << "!("; emit_int_cond(n->operand.get(), o, ifns, ctx); o << ")"; return;
        }
    }
    o << "("; emit_int_expr(e, o, ifns, ctx); o << "!=0LL)";
}

static void emit_int_block(const Block& b, std::ostream& o, const VarSet& ifns, int ind,
                            const SpecCtx& ctx = g_empty_ctx);

static void emit_int_stmt(const StmtNode* s, std::ostream& o, const VarSet& ifns, int ind,
                           const SpecCtx& ctx = g_empty_ctx)
{
    std::string sp(ind * 4, ' ');
    if (dynamic_cast<const FnDeclStmt*>(s)) return;
    if (auto* n = dynamic_cast<const LetStmt*>(s)) {
        o << sp << "int64_t _v_" << n->names[0] << " = ";
        emit_int_expr(n->values[0].get(), o, ifns, ctx); o << ";\n"; return;
    }
    if (auto* n = dynamic_cast<const AssignStmt*>(s)) {
        auto* lv = dynamic_cast<const IdentExpr*>(n->lvalues[0].get());
        o << sp << "_v_" << lv->name << " = ";
        emit_int_expr(n->values[0].get(), o, ifns, ctx); o << ";\n"; return;
    }
    if (auto* n = dynamic_cast<const AugAssignStmt*>(s)) {
        auto* lv = dynamic_cast<const IdentExpr*>(n->lvalue.get());
        using TK = TokenKind;
        if (n->op == TK::SlashSlashEq) {
            o << sp << "_v_" << lv->name << " = syn_idiv(_v_" << lv->name << ",";
            emit_int_expr(n->value.get(), o, ifns, ctx); o << ");\n"; return;
        }
        if (n->op == TK::PercentEq) {
            o << sp << "_v_" << lv->name << " = syn_imod(_v_" << lv->name << ",";
            emit_int_expr(n->value.get(), o, ifns, ctx); o << ");\n"; return;
        }
        const char* op = "+=";
        switch (n->op) {
        case TK::PlusEq: op="+="; break; case TK::MinusEq: op="-="; break;
        case TK::StarEq: op="*="; break; default: break;
        }
        o << sp << "_v_" << lv->name << " " << op << " ";
        emit_int_expr(n->value.get(), o, ifns, ctx); o << ";\n"; return;
    }
    if (auto* n = dynamic_cast<const IfStmt*>(s)) {
        bool first = true;
        for (auto& br : n->branches) {
            if (br.cond) {
                o << sp << (first ? "if (" : "else if (");
                emit_int_cond(br.cond.get(), o, ifns, ctx);
                o << ") {\n"; emit_int_block(br.body, o, ifns, ind+1, ctx); o << sp << "}\n";
            } else {
                o << sp << "else {\n"; emit_int_block(br.body, o, ifns, ind+1, ctx); o << sp << "}\n";
            }
            first = false;
        }
        return;
    }
    if (auto* n = dynamic_cast<const WhileStmt*>(s)) {
        o << sp << "while ("; emit_int_cond(n->cond.get(), o, ifns, ctx);
        o << ") {\n"; emit_int_block(n->body, o, ifns, ind+1, ctx); o << sp << "}\n"; return;
    }
    if (auto* n = dynamic_cast<const ForStmt*>(s)) {
        o << sp << "for (int64_t _v_" << n->iter1 << " = ";
        emit_int_expr(n->source.get(), o, ifns, ctx);
        o << "; _v_" << n->iter1 << " < ";
        emit_int_expr(n->range_end.get(), o, ifns, ctx);
        o << "; _v_" << n->iter1;
        if (n->range_step) { o << " += "; emit_int_expr(n->range_step.get(), o, ifns, ctx); }
        else o << "++";
        o << ") {\n"; emit_int_block(n->body, o, ifns, ind+1, ctx); o << sp << "}\n"; return;
    }
    if (auto* n = dynamic_cast<const ReturnStmt*>(s)) {
        o << sp << "return "; emit_int_expr(n->values[0].get(), o, ifns, ctx); o << ";\n"; return;
    }
    if (auto* n = dynamic_cast<const ExprStmt*>(s)) {
        if (auto* ce = dynamic_cast<const CallExpr*>(n->expr.get())) {
            if (auto* id = dynamic_cast<const IdentExpr*>(ce->callee.get())) {
                if (id->name == "print") {
                    bool first = true;
                    for (auto& arg : ce->args) {
                        if (!first) o << sp << "fputc(' ', stdout);\n";
                        o << sp << "printf(\"%lld\", (long long)(";
                        emit_int_expr(arg.value.get(), o, ifns, ctx);
                        o << "));\n";
                        first = false;
                    }
                    o << sp << "fputc('\\n', stdout);\n";
                    return;
                }
                if (ifns.count(id->name)) {
                    o << sp << "_jit_i_" << id->name << "(";
                    for (size_t k = 0; k < ce->args.size(); ++k) {
                        if (k) o << ",";
                        if (ctx.fn_params.count(id->name) && ctx.fn_params.at(id->name) == k)
                            o << "0LL";
                        else
                            emit_int_expr(ce->args[k].value.get(), o, ifns, ctx);
                    }
                    o << ");\n"; return;
                }
            }
        }
        return;
    }
    if (dynamic_cast<const BreakStmt*>(s))    { o << sp << "break;\n"; return; }
    if (dynamic_cast<const ContinueStmt*>(s)) { o << sp << "continue;\n"; return; }
}

static void emit_int_block(const Block& b, std::ostream& o, const VarSet& ifns, int ind,
                            const SpecCtx& ctx)
{ for (auto& s : b) emit_int_stmt(s.get(), o, ifns, ind, ctx); }

// ---- Float emitters ----

static void emit_float_expr(const ExprNode* e, std::ostream& o, const VarSet& ffns)
{
    if (auto* n = dynamic_cast<const FloatLitExpr*>(e)) {
        char buf[32]; snprintf(buf, sizeof(buf), "(%.17g)", n->value); o << buf; return;
    }
    if (auto* n = dynamic_cast<const IntLitExpr*>(e)) { o << "(double)(" << n->value << "LL)"; return; }
    if (auto* n = dynamic_cast<const IdentExpr*>(e)) { o << "_v_" << n->name; return; }
    if (auto* n = dynamic_cast<const UnaryExpr*>(e)) {
        o << "(-"; emit_float_expr(n->operand.get(), o, ffns); o << ")"; return;
    }
    if (auto* n = dynamic_cast<const BinaryExpr*>(e)) {
        const char* op = nullptr;
        switch (n->op) {
        case TokenKind::Plus:  op="+"; break; case TokenKind::Minus: op="-"; break;
        case TokenKind::Star:  op="*"; break; case TokenKind::Slash: op="/"; break;
        case TokenKind::Lt: op="<"; break;   case TokenKind::LtEq: op="<="; break;
        case TokenKind::Gt: op=">"; break;   case TokenKind::GtEq: op=">="; break;
        case TokenKind::EqEq: op="=="; break; case TokenKind::BangEq: op="!="; break;
        default: o << "(0.0)"; return;
        }
        o << "("; emit_float_expr(n->left.get(), o, ffns);
        o << op; emit_float_expr(n->right.get(), o, ffns); o << ")"; return;
    }
    if (auto* n = dynamic_cast<const CallExpr*>(e)) {
        if (auto* id = dynamic_cast<const IdentExpr*>(n->callee.get())) {
            if (ffns.count(id->name)) {
                o << "_jit_f_" << id->name << "(";
                for (size_t i = 0; i < n->args.size(); ++i) {
                    if (i) o << ","; emit_float_expr(n->args[i].value.get(), o, ffns);
                }
                o << ")"; return;
            }
        }
    }
    o << "(0.0)";
}

static void emit_float_cond(const ExprNode* e, std::ostream& o, const VarSet& ffns)
{
    if (auto* n = dynamic_cast<const BoolLitExpr*>(e)) { o << (n->value?"1":"0"); return; }
    if (auto* n = dynamic_cast<const BinaryExpr*>(e)) {
        using TK = TokenKind;
        const char* op = nullptr;
        switch (n->op) {
        case TK::EqEq: op="=="; break; case TK::BangEq: op="!="; break;
        case TK::Lt:   op="<";  break; case TK::LtEq:   op="<="; break;
        case TK::Gt:   op=">";  break; case TK::GtEq:   op=">="; break;
        case TK::And:
            o << "("; emit_float_cond(n->left.get(), o, ffns);
            o << "&&"; emit_float_cond(n->right.get(), o, ffns); o << ")"; return;
        case TK::Or:
            o << "("; emit_float_cond(n->left.get(), o, ffns);
            o << "||"; emit_float_cond(n->right.get(), o, ffns); o << ")"; return;
        default: break;
        }
        if (op) {
            o << "("; emit_float_expr(n->left.get(), o, ffns);
            o << op; emit_float_expr(n->right.get(), o, ffns); o << ")"; return;
        }
    }
    if (auto* n = dynamic_cast<const UnaryExpr*>(e)) {
        if (n->op == TokenKind::Not) {
            o << "!("; emit_float_cond(n->operand.get(), o, ffns); o << ")"; return;
        }
    }
    o << "("; emit_float_expr(e, o, ffns); o << "!=0.0)";
}

static void emit_float_block(const Block& b, std::ostream& o, const VarSet& ffns, int ind);

static void emit_float_stmt(const StmtNode* s, std::ostream& o, const VarSet& ffns, int ind)
{
    std::string sp(ind * 4, ' ');
    if (dynamic_cast<const FnDeclStmt*>(s)) return;
    if (auto* n = dynamic_cast<const LetStmt*>(s)) {
        o << sp << "double _v_" << n->names[0] << " = ";
        emit_float_expr(n->values[0].get(), o, ffns); o << ";\n"; return;
    }
    if (auto* n = dynamic_cast<const AssignStmt*>(s)) {
        auto* lv = dynamic_cast<const IdentExpr*>(n->lvalues[0].get());
        o << sp << "_v_" << lv->name << " = ";
        emit_float_expr(n->values[0].get(), o, ffns); o << ";\n"; return;
    }
    if (auto* n = dynamic_cast<const AugAssignStmt*>(s)) {
        auto* lv = dynamic_cast<const IdentExpr*>(n->lvalue.get());
        const char* op = "+=";
        switch (n->op) {
        case TokenKind::PlusEq:  op="+="; break; case TokenKind::MinusEq: op="-="; break;
        case TokenKind::StarEq:  op="*="; break; case TokenKind::SlashEq: op="/="; break;
        default: break;
        }
        o << sp << "_v_" << lv->name << " " << op << " ";
        emit_float_expr(n->value.get(), o, ffns); o << ";\n"; return;
    }
    if (auto* n = dynamic_cast<const IfStmt*>(s)) {
        bool first = true;
        for (auto& br : n->branches) {
            if (br.cond) {
                o << sp << (first ? "if (" : "else if (");
                emit_float_cond(br.cond.get(), o, ffns);
                o << ") {\n"; emit_float_block(br.body, o, ffns, ind+1); o << sp << "}\n";
            } else {
                o << sp << "else {\n"; emit_float_block(br.body, o, ffns, ind+1); o << sp << "}\n";
            }
            first = false;
        }
        return;
    }
    if (auto* n = dynamic_cast<const WhileStmt*>(s)) {
        o << sp << "while ("; emit_float_cond(n->cond.get(), o, ffns);
        o << ") {\n"; emit_float_block(n->body, o, ffns, ind+1); o << sp << "}\n"; return;
    }
    if (auto* n = dynamic_cast<const ForStmt*>(s)) {
        o << sp << "for (double _v_" << n->iter1 << " = ";
        emit_float_expr(n->source.get(), o, ffns);
        o << "; _v_" << n->iter1 << " < ";
        emit_float_expr(n->range_end.get(), o, ffns);
        o << "; _v_" << n->iter1;
        if (n->range_step) { o << " += "; emit_float_expr(n->range_step.get(), o, ffns); }
        else o << "++";
        o << ") {\n"; emit_float_block(n->body, o, ffns, ind+1); o << sp << "}\n"; return;
    }
    if (auto* n = dynamic_cast<const ReturnStmt*>(s)) {
        o << sp << "return "; emit_float_expr(n->values[0].get(), o, ffns); o << ";\n"; return;
    }
    if (auto* n = dynamic_cast<const ExprStmt*>(s)) {
        if (auto* ce = dynamic_cast<const CallExpr*>(n->expr.get())) {
            if (auto* id = dynamic_cast<const IdentExpr*>(ce->callee.get())) {
                if (id->name == "print") {
                    bool first = true;
                    for (auto& arg : ce->args) {
                        if (!first) o << sp << "fputc(' ', stdout);\n";
                        o << sp << "syn_print_double((double)(";
                        emit_float_expr(arg.value.get(), o, ffns);
                        o << "));\n";
                        first = false;
                    }
                    o << sp << "fputc('\\n', stdout);\n";
                    return;
                }
                if (ffns.count(id->name)) {
                    o << sp << "_jit_f_" << id->name << "(";
                    for (size_t i = 0; i < ce->args.size(); ++i) {
                        if (i) o << ","; emit_float_expr(ce->args[i].value.get(), o, ffns);
                    }
                    o << ");\n"; return;
                }
            }
        }
        return;
    }
    if (dynamic_cast<const BreakStmt*>(s))    { o << sp << "break;\n"; return; }
    if (dynamic_cast<const ContinueStmt*>(s)) { o << sp << "continue;\n"; return; }
}

static void emit_float_block(const Block& b, std::ostream& o, const VarSet& ffns, int ind)
{ for (auto& s : b) emit_float_stmt(s.get(), o, ffns, ind); }

// ---- Mixed int+float emitters ----

static void emit_as_float(const ExprNode* e, std::ostream& o, const VTMap& vt,
                           const VarSet& ifns, const VarSet& ffns);
static void emit_as_int(const ExprNode* e, std::ostream& o, const VTMap& vt,
                         const VarSet& ifns, const VarSet& ffns);

static void emit_as_float(const ExprNode* e, std::ostream& o, const VTMap& vt,
                           const VarSet& ifns, const VarSet& ffns)
{
    if (auto* n = dynamic_cast<const FloatLitExpr*>(e)) {
        char buf[32]; snprintf(buf, sizeof(buf), "(%.17g)", n->value); o << buf; return;
    }
    if (auto* n = dynamic_cast<const IntLitExpr*>(e)) {
        o << "(double)(" << n->value << "LL)"; return;
    }
    if (auto* n = dynamic_cast<const BoolLitExpr*>(e)) { o << (n->value?"1.0":"0.0"); return; }
    if (auto* n = dynamic_cast<const IdentExpr*>(e)) {
        auto it = vt.find(n->name);
        bool is_float = it != vt.end() && it->second;
        if (is_float) o << "_v_" << n->name;
        else          o << "(double)_v_" << n->name;
        return;
    }
    if (auto* n = dynamic_cast<const UnaryExpr*>(e)) {
        o << "(-"; emit_as_float(n->operand.get(), o, vt, ifns, ffns); o << ")"; return;
    }
    if (auto* n = dynamic_cast<const BinaryExpr*>(e)) {
        using TK = TokenKind;
        if (n->op == TK::Slash) {
            o << "("; emit_as_float(n->left.get(), o, vt, ifns, ffns);
            o << "/"; emit_as_float(n->right.get(), o, vt, ifns, ffns); o << ")"; return;
        }
        if (n->op == TK::SlashSlash) {
            o << "floor(("; emit_as_float(n->left.get(), o, vt, ifns, ffns);
            o << ")/("; emit_as_float(n->right.get(), o, vt, ifns, ffns); o << "))"; return;
        }
        if (n->op == TK::Percent) {
            o << "fmod("; emit_as_float(n->left.get(), o, vt, ifns, ffns);
            o << ","; emit_as_float(n->right.get(), o, vt, ifns, ffns); o << ")"; return;
        }
        if (n->op == TK::StarStar) {
            o << "pow("; emit_as_float(n->left.get(), o, vt, ifns, ffns);
            o << ","; emit_as_float(n->right.get(), o, vt, ifns, ffns); o << ")"; return;
        }
        const char* op = nullptr;
        switch (n->op) {
        case TK::Plus: op="+"; break; case TK::Minus: op="-"; break;
        case TK::Star: op="*"; break;
        case TK::Lt: op="<"; break;   case TK::LtEq: op="<="; break;
        case TK::Gt: op=">"; break;   case TK::GtEq: op=">="; break;
        case TK::EqEq: op="=="; break; case TK::BangEq: op="!="; break;
        default: o << "(0.0)"; return;
        }
        o << "("; emit_as_float(n->left.get(), o, vt, ifns, ffns);
        o << op; emit_as_float(n->right.get(), o, vt, ifns, ffns); o << ")"; return;
    }
    if (auto* n = dynamic_cast<const CallExpr*>(e)) {
        if (auto* id = dynamic_cast<const IdentExpr*>(n->callee.get())) {
            if (g_math_ffns.count(id->name)) {
                // map Synapse 'abs' → C 'fabs' for float context
                std::string fname = (id->name == "abs") ? "fabs" : id->name;
                o << fname << "(";
                for (size_t i = 0; i < n->args.size(); ++i) {
                    if (i) o << ","; emit_as_float(n->args[i].value.get(), o, vt, ifns, ffns);
                }
                o << ")"; return;
            }
            if (ffns.count(id->name)) {
                o << "_jit_f_" << id->name << "(";
                for (size_t i = 0; i < n->args.size(); ++i) {
                    if (i) o << ","; emit_as_float(n->args[i].value.get(), o, vt, ifns, ffns);
                }
                o << ")"; return;
            }
            if (ifns.count(id->name)) {
                o << "(double)_jit_i_" << id->name << "(";
                for (size_t i = 0; i < n->args.size(); ++i) {
                    if (i) o << ","; emit_as_int(n->args[i].value.get(), o, vt, ifns, ffns);
                }
                o << ")"; return;
            }
        }
    }
    o << "(0.0)";
}

static void emit_as_int(const ExprNode* e, std::ostream& o, const VTMap& vt,
                         const VarSet& ifns, const VarSet& ffns)
{
    if (auto* n = dynamic_cast<const IntLitExpr*>(e))   { o << "(" << n->value << "LL)"; return; }
    if (auto* n = dynamic_cast<const FloatLitExpr*>(e)) { o << "(int64_t)(" << n->value << ")"; return; }
    if (auto* n = dynamic_cast<const BoolLitExpr*>(e))  { o << (n->value?"1LL":"0LL"); return; }
    if (auto* n = dynamic_cast<const IdentExpr*>(e)) {
        auto it = vt.find(n->name);
        bool is_float = it != vt.end() && it->second;
        if (is_float) o << "(int64_t)_v_" << n->name;
        else          o << "_v_" << n->name;
        return;
    }
    if (auto* n = dynamic_cast<const UnaryExpr*>(e)) {
        o << "(-"; emit_as_int(n->operand.get(), o, vt, ifns, ffns); o << ")"; return;
    }
    if (auto* n = dynamic_cast<const BinaryExpr*>(e)) {
        using TK = TokenKind;
        if (n->op == TK::SlashSlash) {
            o << "syn_idiv("; emit_as_int(n->left.get(), o, vt, ifns, ffns);
            o << ","; emit_as_int(n->right.get(), o, vt, ifns, ffns); o << ")"; return;
        }
        if (n->op == TK::Percent) {
            o << "syn_imod("; emit_as_int(n->left.get(), o, vt, ifns, ffns);
            o << ","; emit_as_int(n->right.get(), o, vt, ifns, ffns); o << ")"; return;
        }
        if (n->op == TK::StarStar) {
            o << "syn_ipow("; emit_as_int(n->left.get(), o, vt, ifns, ffns);
            o << ","; emit_as_int(n->right.get(), o, vt, ifns, ffns); o << ")"; return;
        }
        const char* op = nullptr;
        switch (n->op) {
        case TK::Plus: op="+"; break; case TK::Minus: op="-"; break;
        case TK::Star: op="*"; break;
        case TK::Lt: op="<"; break;   case TK::LtEq: op="<="; break;
        case TK::Gt: op=">"; break;   case TK::GtEq: op=">="; break;
        case TK::EqEq: op="=="; break; case TK::BangEq: op="!="; break;
        default: o << "(0LL)"; return;
        }
        o << "("; emit_as_int(n->left.get(), o, vt, ifns, ffns);
        o << op; emit_as_int(n->right.get(), o, vt, ifns, ffns); o << ")"; return;
    }
    if (auto* n = dynamic_cast<const CallExpr*>(e)) {
        if (auto* id = dynamic_cast<const IdentExpr*>(n->callee.get())) {
            if (ifns.count(id->name)) {
                o << "_jit_i_" << id->name << "(";
                for (size_t i = 0; i < n->args.size(); ++i) {
                    if (i) o << ","; emit_as_int(n->args[i].value.get(), o, vt, ifns, ffns);
                }
                o << ")"; return;
            }
        }
    }
    o << "(0LL)";
}

static void emit_mixed_cond(const ExprNode* e, std::ostream& o, const VTMap& vt,
                             const VarSet& ifns, const VarSet& ffns)
{
    if (auto* n = dynamic_cast<const BoolLitExpr*>(e)) { o << (n->value?"1":"0"); return; }
    if (dynamic_cast<const NoneLitExpr*>(e)) { o << "0"; return; }
    if (auto* n = dynamic_cast<const BinaryExpr*>(e)) {
        using TK = TokenKind;
        const char* cmp = nullptr;
        switch (n->op) {
        case TK::EqEq: cmp="=="; break; case TK::BangEq: cmp="!="; break;
        case TK::Lt:   cmp="<";  break; case TK::LtEq:   cmp="<="; break;
        case TK::Gt:   cmp=">";  break; case TK::GtEq:   cmp=">="; break;
        case TK::And:
            o << "("; emit_mixed_cond(n->left.get(), o, vt, ifns, ffns);
            o << "&&"; emit_mixed_cond(n->right.get(), o, vt, ifns, ffns); o << ")"; return;
        case TK::Or:
            o << "("; emit_mixed_cond(n->left.get(), o, vt, ifns, ffns);
            o << "||"; emit_mixed_cond(n->right.get(), o, vt, ifns, ffns); o << ")"; return;
        default: break;
        }
        if (cmp) {
            auto l = infer_type(n->left.get(), vt, ifns, ffns);
            auto r = infer_type(n->right.get(), vt, ifns, ffns);
            bool use_float = (l && *l) || (r && *r);
            o << "(";
            if (use_float) {
                emit_as_float(n->left.get(), o, vt, ifns, ffns); o << cmp;
                emit_as_float(n->right.get(), o, vt, ifns, ffns);
            } else {
                emit_as_int(n->left.get(), o, vt, ifns, ffns); o << cmp;
                emit_as_int(n->right.get(), o, vt, ifns, ffns);
            }
            o << ")"; return;
        }
    }
    if (auto* n = dynamic_cast<const UnaryExpr*>(e)) {
        if (n->op == TokenKind::Not) {
            o << "!("; emit_mixed_cond(n->operand.get(), o, vt, ifns, ffns); o << ")"; return;
        }
    }
    auto t = infer_type(e, vt, ifns, ffns);
    if (t && *t) { o << "("; emit_as_float(e, o, vt, ifns, ffns); o << "!=0.0)"; }
    else         { o << "("; emit_as_int(e, o, vt, ifns, ffns);   o << "!=0LL)"; }
}

static void emit_mixed_block(const Block& b, std::ostream& o, const VTMap& vt,
                              const VarSet& ifns, const VarSet& ffns, int ind);

static void emit_mixed_stmt(const StmtNode* s, std::ostream& o, const VTMap& vt,
                             const VarSet& ifns, const VarSet& ffns, int ind)
{
    std::string sp(ind * 4, ' ');
    if (dynamic_cast<const FnDeclStmt*>(s)) return;
    if (auto* n = dynamic_cast<const LetStmt*>(s)) {
        auto it = vt.find(n->names[0]);
        bool is_float = it != vt.end() && it->second;
        if (is_float) {
            o << sp << "double _v_" << n->names[0] << " = ";
            emit_as_float(n->values[0].get(), o, vt, ifns, ffns);
        } else {
            o << sp << "int64_t _v_" << n->names[0] << " = ";
            emit_as_int(n->values[0].get(), o, vt, ifns, ffns);
        }
        o << ";\n"; return;
    }
    if (auto* n = dynamic_cast<const AssignStmt*>(s)) {
        auto* lv = dynamic_cast<const IdentExpr*>(n->lvalues[0].get());
        auto it = vt.find(lv->name);
        bool is_float = it != vt.end() && it->second;
        o << sp << "_v_" << lv->name << " = ";
        if (is_float) emit_as_float(n->values[0].get(), o, vt, ifns, ffns);
        else          emit_as_int(n->values[0].get(), o, vt, ifns, ffns);
        o << ";\n"; return;
    }
    if (auto* n = dynamic_cast<const AugAssignStmt*>(s)) {
        auto* lv = dynamic_cast<const IdentExpr*>(n->lvalue.get());
        auto it = vt.find(lv->name);
        bool is_float = it != vt.end() && it->second;
        using TK = TokenKind;
        if (!is_float && n->op == TK::SlashSlashEq) {
            o << sp << "_v_" << lv->name << " = syn_idiv(_v_" << lv->name << ",";
            emit_as_int(n->value.get(), o, vt, ifns, ffns); o << ");\n"; return;
        }
        if (!is_float && n->op == TK::PercentEq) {
            o << sp << "_v_" << lv->name << " = syn_imod(_v_" << lv->name << ",";
            emit_as_int(n->value.get(), o, vt, ifns, ffns); o << ");\n"; return;
        }
        const char* op = "+=";
        switch (n->op) {
        case TK::PlusEq:  op="+="; break; case TK::MinusEq: op="-="; break;
        case TK::StarEq:  op="*="; break; case TK::SlashEq: op="/="; break;
        default: break;
        }
        o << sp << "_v_" << lv->name << " " << op << " ";
        if (is_float) emit_as_float(n->value.get(), o, vt, ifns, ffns);
        else          emit_as_int(n->value.get(), o, vt, ifns, ffns);
        o << ";\n"; return;
    }
    if (auto* n = dynamic_cast<const IfStmt*>(s)) {
        bool first = true;
        for (auto& br : n->branches) {
            if (br.cond) {
                o << sp << (first ? "if (" : "else if (");
                emit_mixed_cond(br.cond.get(), o, vt, ifns, ffns);
                o << ") {\n"; emit_mixed_block(br.body, o, vt, ifns, ffns, ind+1); o << sp << "}\n";
            } else {
                o << sp << "else {\n"; emit_mixed_block(br.body, o, vt, ifns, ffns, ind+1); o << sp << "}\n";
            }
            first = false;
        }
        return;
    }
    if (auto* n = dynamic_cast<const WhileStmt*>(s)) {
        o << sp << "while ("; emit_mixed_cond(n->cond.get(), o, vt, ifns, ffns);
        o << ") {\n"; emit_mixed_block(n->body, o, vt, ifns, ffns, ind+1); o << sp << "}\n"; return;
    }
    if (auto* n = dynamic_cast<const ForStmt*>(s)) {
        o << sp << "for (int64_t _v_" << n->iter1 << " = ";
        emit_as_int(n->source.get(), o, vt, ifns, ffns);
        o << "; _v_" << n->iter1 << " < ";
        emit_as_int(n->range_end.get(), o, vt, ifns, ffns);
        o << "; _v_" << n->iter1;
        if (n->range_step) { o << " += "; emit_as_int(n->range_step.get(), o, vt, ifns, ffns); }
        else o << "++";
        o << ") {\n"; emit_mixed_block(n->body, o, vt, ifns, ffns, ind+1); o << sp << "}\n"; return;
    }
    if (auto* n = dynamic_cast<const ReturnStmt*>(s)) {
        if (n->values.empty()) { o << sp << "return (0LL);\n"; return; }
        auto t = infer_type(n->values[0].get(), vt, ifns, ffns);
        o << sp << "return ";
        if (t && *t) emit_as_float(n->values[0].get(), o, vt, ifns, ffns);
        else         emit_as_int(n->values[0].get(), o, vt, ifns, ffns);
        o << ";\n"; return;
    }
    if (auto* n = dynamic_cast<const ExprStmt*>(s)) {
        if (auto* ce = dynamic_cast<const CallExpr*>(n->expr.get())) {
            if (auto* id = dynamic_cast<const IdentExpr*>(ce->callee.get())) {
                if (id->name == "print") {
                    bool first = true;
                    for (auto& arg : ce->args) {
                        if (!first) o << sp << "fputc(' ', stdout);\n";
                        auto t = infer_type(arg.value.get(), vt, ifns, ffns);
                        if (t && *t) {
                            o << sp << "syn_print_double(";
                            emit_as_float(arg.value.get(), o, vt, ifns, ffns); o << ");\n";
                        } else {
                            o << sp << "printf(\"%lld\", (long long)(";
                            emit_as_int(arg.value.get(), o, vt, ifns, ffns); o << "));\n";
                        }
                        first = false;
                    }
                    o << sp << "fputc('\\n', stdout);\n"; return;
                }
                if (ifns.count(id->name)) {
                    o << sp << "_jit_i_" << id->name << "(";
                    for (size_t i = 0; i < ce->args.size(); ++i) {
                        if (i) o << ","; emit_as_int(ce->args[i].value.get(), o, vt, ifns, ffns);
                    }
                    o << ");\n"; return;
                }
                if (ffns.count(id->name)) {
                    o << sp << "_jit_f_" << id->name << "(";
                    for (size_t i = 0; i < ce->args.size(); ++i) {
                        if (i) o << ","; emit_as_float(ce->args[i].value.get(), o, vt, ifns, ffns);
                    }
                    o << ");\n"; return;
                }
            }
        }
        return;
    }
    if (dynamic_cast<const BreakStmt*>(s))    { o << sp << "break;\n"; return; }
    if (dynamic_cast<const ContinueStmt*>(s)) { o << sp << "continue;\n"; return; }
}

static void emit_mixed_block(const Block& b, std::ostream& o, const VTMap& vt,
                              const VarSet& ifns, const VarSet& ffns, int ind)
{ for (auto& s : b) emit_mixed_stmt(s.get(), o, vt, ifns, ffns, ind); }

// ── List-specialized JIT (malloc C arrays, bypasses GC) ──────────────────────

struct ListInfo {
    bool is_float = false;
    bool is_fill  = false;
    bool is_literal = false;                   // non-empty list literal (not a fill loop)
    bool is_bool  = false;                     // fill initialized with true/false → use int8_t
    std::vector<const ExprNode*> literal_elems;// element exprs when is_literal
    const ExprNode* size_expr = nullptr;
    const ExprNode* init_expr = nullptr;
};
using LVMap = std::unordered_map<std::string, ListInfo>;

struct LFnParam { bool is_list; bool is_float; };
using LFnMap = std::unordered_map<std::string, std::vector<LFnParam>>;
using LFnRetMap = std::unordered_map<std::string, std::optional<bool>>; // fn → return type

// Thread-local global variable context (set by GCtxGuard during list JIT emit/check)
static thread_local const VTMap* tl_gvt = nullptr;    // global scalar types
static thread_local const LVMap* tl_glv = nullptr;    // global list info
static thread_local bool tl_is_main = false;           // true = emitting __main__
static thread_local const LFnRetMap* tl_lfn_rets = nullptr; // list fn return types

struct GCtxGuard {
    const VTMap* pv; const LVMap* pl; bool im;
    GCtxGuard(const VTMap& gvt, const LVMap& glv, bool is_main = false)
        : pv(tl_gvt), pl(tl_glv), im(tl_is_main)
        { tl_gvt = &gvt; tl_glv = &glv; tl_is_main = is_main; }
    ~GCtxGuard() { tl_gvt = pv; tl_glv = pl; tl_is_main = im; }
};

static std::optional<bool> infer_type_l(const ExprNode* e, const VTMap& vt, const LVMap& lv,
                                         const VarSet& ifns, const VarSet& ffns)
{
    if (!e) return std::nullopt;
    if (auto* n = dynamic_cast<const IndexExpr*>(e)) {
        auto* lid = dynamic_cast<const IdentExpr*>(n->object.get());
        if (lid) {
            auto it = lv.find(lid->name);
            if (it != lv.end() && (it->second.is_fill || it->second.is_literal)) {
                auto idx = infer_type_l(n->index.get(), vt, lv, ifns, ffns);
                if (!idx || *idx) return std::nullopt;
                return it->second.is_float;
            }
            // Check global lv
            if (tl_glv) {
                auto git = tl_glv->find(lid->name);
                if (git != tl_glv->end() && (git->second.is_fill || git->second.is_literal)) {
                    auto idx = infer_type_l(n->index.get(), vt, lv, ifns, ffns);
                    if (!idx || *idx) return std::nullopt;
                    return git->second.is_float;
                }
            }
        }
        return std::nullopt;
    }
    if (dynamic_cast<const IntLitExpr*>(e) || dynamic_cast<const BoolLitExpr*>(e)) return false;
    if (dynamic_cast<const FloatLitExpr*>(e)) return true;
    if (auto* n = dynamic_cast<const IdentExpr*>(e)) {
        auto it = vt.find(n->name);
        if (it != vt.end()) return it->second;
        if (tl_gvt) { auto git = tl_gvt->find(n->name); if (git != tl_gvt->end()) return git->second; }
        return std::nullopt;
    }
    if (auto* n = dynamic_cast<const UnaryExpr*>(e)) {
        if (n->op == TokenKind::Minus || n->op == TokenKind::Not)
            return infer_type_l(n->operand.get(), vt, lv, ifns, ffns);
        return std::nullopt;
    }
    if (auto* n = dynamic_cast<const BinaryExpr*>(e)) {
        using TK = TokenKind;
        if (n->op == TK::Slash) {
            auto l = infer_type_l(n->left.get(), vt, lv, ifns, ffns);
            auto r = infer_type_l(n->right.get(), vt, lv, ifns, ffns);
            return (l && r) ? std::optional<bool>{true} : std::nullopt;
        }
        if (n->op == TK::SlashSlash || n->op == TK::Percent || n->op == TK::StarStar) {
            auto l = infer_type_l(n->left.get(), vt, lv, ifns, ffns);
            auto r = infer_type_l(n->right.get(), vt, lv, ifns, ffns);
            if (!l || !r || *l || *r) return std::nullopt;
            return false;
        }
        auto l = infer_type_l(n->left.get(), vt, lv, ifns, ffns);
        auto r = infer_type_l(n->right.get(), vt, lv, ifns, ffns);
        if (!l || !r) return std::nullopt;
        return *l || *r;
    }
    if (auto* n = dynamic_cast<const CallExpr*>(e)) {
        auto* id = dynamic_cast<const IdentExpr*>(n->callee.get());
        if (!id) return std::nullopt;
        if (g_math_ffns.count(id->name)) return true;
        if (id->name == "len") return false;
        if (ifns.count(id->name)) return false;
        if (ffns.count(id->name)) return true;
        if (tl_lfn_rets) {
            auto rit = tl_lfn_rets->find(id->name);
            if (rit != tl_lfn_rets->end()) return rit->second;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

static bool is_list_cond(const ExprNode* e, const VTMap& vt, const LVMap& lv,
                          const VarSet& ifns, const VarSet& ffns)
{
    if (!e) return false;
    if (dynamic_cast<const BoolLitExpr*>(e) || dynamic_cast<const NoneLitExpr*>(e)) return true;
    if (auto* n = dynamic_cast<const BinaryExpr*>(e)) {
        using TK = TokenKind;
        switch (n->op) {
        case TK::EqEq: case TK::BangEq:
        case TK::Lt:   case TK::LtEq:
        case TK::Gt:   case TK::GtEq: {
            auto l = infer_type_l(n->left.get(), vt, lv, ifns, ffns);
            auto r = infer_type_l(n->right.get(), vt, lv, ifns, ffns);
            return l.has_value() && r.has_value();
        }
        case TK::And: case TK::Or:
            return is_list_cond(n->left.get(), vt, lv, ifns, ffns) &&
                   is_list_cond(n->right.get(), vt, lv, ifns, ffns);
        default: break;
        }
    }
    if (auto* n = dynamic_cast<const UnaryExpr*>(e))
        if (n->op == TokenKind::Not) return is_list_cond(n->operand.get(), vt, lv, ifns, ffns);
    return infer_type_l(e, vt, lv, ifns, ffns).has_value();
}

static void emit_as_float_l(const ExprNode* e, std::ostream& o, const VTMap& vt, const LVMap& lv,
                              const VarSet& ifns, const VarSet& ffns);
static void emit_as_int_l(const ExprNode* e, std::ostream& o, const VTMap& vt, const LVMap& lv,
                           const VarSet& ifns, const VarSet& ffns);

static void emit_as_float_l(const ExprNode* e, std::ostream& o, const VTMap& vt, const LVMap& lv,
                              const VarSet& ifns, const VarSet& ffns)
{
    if (auto* n = dynamic_cast<const IndexExpr*>(e)) {
        auto* lid = dynamic_cast<const IdentExpr*>(n->object.get());
        if (lid) {
            auto it = lv.find(lid->name);
            if (it != lv.end() && (it->second.is_fill || it->second.is_literal)) {
                if (!it->second.is_float) o << "(double)";
                // Literal lists in __main__ are global statics; fill-loop lists use malloc _vptr_
                bool use_g = tl_is_main && it->second.is_literal;
                o << (use_g ? "g_vptr_" : "_vptr_") << lid->name << "[";
                emit_as_int_l(n->index.get(), o, vt, lv, ifns, ffns);
                o << "]"; return;
            }
            if (tl_glv) {
                auto git = tl_glv->find(lid->name);
                if (git != tl_glv->end() && (git->second.is_fill || git->second.is_literal)) {
                    if (!git->second.is_float) o << "(double)";
                    o << "g_vptr_" << lid->name << "[";
                    emit_as_int_l(n->index.get(), o, vt, lv, ifns, ffns);
                    o << "]"; return;
                }
            }
        }
        o << "(0.0)"; return;
    }
    if (auto* n = dynamic_cast<const FloatLitExpr*>(e)) {
        char buf[32]; snprintf(buf, sizeof(buf), "(%.17g)", n->value); o << buf; return;
    }
    if (auto* n = dynamic_cast<const IntLitExpr*>(e)) { o << "(double)(" << n->value << "LL)"; return; }
    if (auto* n = dynamic_cast<const BoolLitExpr*>(e)) { o << (n->value?"1.0":"0.0"); return; }
    if (auto* n = dynamic_cast<const IdentExpr*>(e)) {
        auto it = vt.find(n->name);
        if (it != vt.end()) {
            bool is_glob = tl_is_main && tl_gvt && tl_gvt->count(n->name);
            std::string pfx = is_glob ? "g_v_" : "_v_";
            if (it->second) o << pfx << n->name;
            else o << "(double)" << pfx << n->name;
            return;
        }
        if (tl_gvt) {
            auto git = tl_gvt->find(n->name);
            if (git != tl_gvt->end()) {
                if (git->second) o << "g_v_" << n->name;
                else o << "(double)g_v_" << n->name;
                return;
            }
        }
        o << "(0.0)"; return;
    }
    if (auto* n = dynamic_cast<const UnaryExpr*>(e)) {
        o << "(-"; emit_as_float_l(n->operand.get(), o, vt, lv, ifns, ffns); o << ")"; return;
    }
    if (auto* n = dynamic_cast<const BinaryExpr*>(e)) {
        using TK = TokenKind;
        if (n->op == TK::Slash) {
            o << "("; emit_as_float_l(n->left.get(), o, vt, lv, ifns, ffns);
            o << "/"; emit_as_float_l(n->right.get(), o, vt, lv, ifns, ffns); o << ")"; return;
        }
        if (n->op == TK::SlashSlash) {
            o << "floor(("; emit_as_float_l(n->left.get(), o, vt, lv, ifns, ffns);
            o << ")/("; emit_as_float_l(n->right.get(), o, vt, lv, ifns, ffns); o << "))"; return;
        }
        if (n->op == TK::Percent) {
            o << "fmod("; emit_as_float_l(n->left.get(), o, vt, lv, ifns, ffns);
            o << ","; emit_as_float_l(n->right.get(), o, vt, lv, ifns, ffns); o << ")"; return;
        }
        if (n->op == TK::StarStar) {
            o << "pow("; emit_as_float_l(n->left.get(), o, vt, lv, ifns, ffns);
            o << ","; emit_as_float_l(n->right.get(), o, vt, lv, ifns, ffns); o << ")"; return;
        }
        const char* op = nullptr;
        switch (n->op) {
        case TK::Plus: op="+"; break; case TK::Minus: op="-"; break;
        case TK::Star: op="*"; break;
        case TK::Lt: op="<"; break;   case TK::LtEq: op="<="; break;
        case TK::Gt: op=">"; break;   case TK::GtEq: op=">="; break;
        case TK::EqEq: op="=="; break; case TK::BangEq: op="!="; break;
        default: o << "(0.0)"; return;
        }
        o << "("; emit_as_float_l(n->left.get(), o, vt, lv, ifns, ffns);
        o << op; emit_as_float_l(n->right.get(), o, vt, lv, ifns, ffns); o << ")"; return;
    }
    if (auto* n = dynamic_cast<const CallExpr*>(e)) {
        if (auto* id = dynamic_cast<const IdentExpr*>(n->callee.get())) {
            if (g_math_ffns.count(id->name)) {
                std::string fname = (id->name == "abs") ? "fabs" : id->name;
                o << fname << "(";
                for (size_t i = 0; i < n->args.size(); ++i) {
                    if (i) o << ","; emit_as_float_l(n->args[i].value.get(), o, vt, lv, ifns, ffns);
                }
                o << ")"; return;
            }
            if (ffns.count(id->name)) {
                o << "_jit_f_" << id->name << "(";
                for (size_t i = 0; i < n->args.size(); ++i) {
                    if (i) o << ","; emit_as_float_l(n->args[i].value.get(), o, vt, lv, ifns, ffns);
                }
                o << ")"; return;
            }
            if (ifns.count(id->name)) {
                o << "(double)_jit_i_" << id->name << "(";
                for (size_t i = 0; i < n->args.size(); ++i) {
                    if (i) o << ","; emit_as_int_l(n->args[i].value.get(), o, vt, lv, ifns, ffns);
                }
                o << ")"; return;
            }
            // List function returning float
            if (tl_lfn_rets) {
                auto rit = tl_lfn_rets->find(id->name);
                if (rit != tl_lfn_rets->end() && rit->second && *rit->second) {
                    o << "jit_l_" << id->name << "(";
                    for (size_t i = 0; i < n->args.size(); ++i) {
                        if (i) o << ","; emit_as_float_l(n->args[i].value.get(), o, vt, lv, ifns, ffns);
                    }
                    o << ")"; return;
                }
            }
        }
    }
    o << "(0.0)";
}

static void emit_as_int_l(const ExprNode* e, std::ostream& o, const VTMap& vt, const LVMap& lv,
                           const VarSet& ifns, const VarSet& ffns)
{
    if (auto* n = dynamic_cast<const IndexExpr*>(e)) {
        auto* lid = dynamic_cast<const IdentExpr*>(n->object.get());
        if (lid) {
            auto it = lv.find(lid->name);
            if (it != lv.end() && (it->second.is_fill || it->second.is_literal)) {
                if (it->second.is_float) o << "(int64_t)";
                bool use_g = tl_is_main && it->second.is_literal;
                o << (use_g ? "g_vptr_" : "_vptr_") << lid->name << "[";
                emit_as_int_l(n->index.get(), o, vt, lv, ifns, ffns);
                o << "]"; return;
            }
            if (tl_glv) {
                auto git = tl_glv->find(lid->name);
                if (git != tl_glv->end() && (git->second.is_fill || git->second.is_literal)) {
                    if (git->second.is_float) o << "(int64_t)";
                    o << "g_vptr_" << lid->name << "[";
                    emit_as_int_l(n->index.get(), o, vt, lv, ifns, ffns);
                    o << "]"; return;
                }
            }
        }
        o << "(0LL)"; return;
    }
    if (auto* n = dynamic_cast<const IntLitExpr*>(e))   { o << "(" << n->value << "LL)"; return; }
    if (auto* n = dynamic_cast<const FloatLitExpr*>(e)) { o << "(int64_t)(" << n->value << ")"; return; }
    if (auto* n = dynamic_cast<const BoolLitExpr*>(e))  { o << (n->value?"1LL":"0LL"); return; }
    if (auto* n = dynamic_cast<const IdentExpr*>(e)) {
        auto it = vt.find(n->name);
        if (it != vt.end()) {
            // In __main__, all scalars are global statics; in list fns, they are locals
            bool is_glob = tl_is_main && tl_gvt && tl_gvt->count(n->name);
            std::string pfx = is_glob ? "g_v_" : "_v_";
            if (it->second) o << "(int64_t)" << pfx << n->name;
            else o << pfx << n->name;
            return;
        }
        if (tl_gvt) {
            auto git = tl_gvt->find(n->name);
            if (git != tl_gvt->end()) {
                if (git->second) o << "(int64_t)g_v_" << n->name;
                else o << "g_v_" << n->name;
                return;
            }
        }
        o << "(0LL)"; return;
    }
    if (auto* n = dynamic_cast<const UnaryExpr*>(e)) {
        o << "(-"; emit_as_int_l(n->operand.get(), o, vt, lv, ifns, ffns); o << ")"; return;
    }
    if (auto* n = dynamic_cast<const BinaryExpr*>(e)) {
        using TK = TokenKind;
        if (n->op == TK::SlashSlash) {
            o << "syn_idiv("; emit_as_int_l(n->left.get(), o, vt, lv, ifns, ffns);
            o << ","; emit_as_int_l(n->right.get(), o, vt, lv, ifns, ffns); o << ")"; return;
        }
        if (n->op == TK::Percent) {
            o << "syn_imod("; emit_as_int_l(n->left.get(), o, vt, lv, ifns, ffns);
            o << ","; emit_as_int_l(n->right.get(), o, vt, lv, ifns, ffns); o << ")"; return;
        }
        if (n->op == TK::StarStar) {
            o << "syn_ipow("; emit_as_int_l(n->left.get(), o, vt, lv, ifns, ffns);
            o << ","; emit_as_int_l(n->right.get(), o, vt, lv, ifns, ffns); o << ")"; return;
        }
        const char* op = nullptr;
        switch (n->op) {
        case TK::Plus: op="+"; break; case TK::Minus: op="-"; break;
        case TK::Star: op="*"; break;
        case TK::Lt: op="<"; break;   case TK::LtEq: op="<="; break;
        case TK::Gt: op=">"; break;   case TK::GtEq: op=">="; break;
        case TK::EqEq: op="=="; break; case TK::BangEq: op="!="; break;
        default: o << "(0LL)"; return;
        }
        o << "("; emit_as_int_l(n->left.get(), o, vt, lv, ifns, ffns);
        o << op; emit_as_int_l(n->right.get(), o, vt, lv, ifns, ffns); o << ")"; return;
    }
    if (auto* n = dynamic_cast<const CallExpr*>(e)) {
        if (auto* id = dynamic_cast<const IdentExpr*>(n->callee.get())) {
            if (ifns.count(id->name)) {
                o << "_jit_i_" << id->name << "(";
                for (size_t i = 0; i < n->args.size(); ++i) {
                    if (i) o << ","; emit_as_int_l(n->args[i].value.get(), o, vt, lv, ifns, ffns);
                }
                o << ")"; return;
            }
            if (id->name == "len" && n->args.size() == 1) {
                auto* lid = dynamic_cast<const IdentExpr*>(n->args[0].value.get());
                if (lid) {
                    auto it = lv.find(lid->name);
                    if (it != lv.end() && (it->second.is_fill || it->second.is_literal))
                        { o << "_vlen_" << lid->name; return; }
                    if (tl_glv) {
                        auto git = tl_glv->find(lid->name);
                        if (git != tl_glv->end()) { o << "g_vlen_" << lid->name; return; }
                    }
                }
            }
        }
    }
    o << "(0LL)";
}

static void emit_list_cond(const ExprNode* e, std::ostream& o, const VTMap& vt, const LVMap& lv,
                            const VarSet& ifns, const VarSet& ffns)
{
    if (auto* n = dynamic_cast<const BoolLitExpr*>(e)) { o << (n->value?"1":"0"); return; }
    if (dynamic_cast<const NoneLitExpr*>(e)) { o << "0"; return; }
    if (auto* n = dynamic_cast<const BinaryExpr*>(e)) {
        using TK = TokenKind;
        const char* cmp = nullptr;
        switch (n->op) {
        case TK::EqEq: cmp="=="; break; case TK::BangEq: cmp="!="; break;
        case TK::Lt:   cmp="<";  break; case TK::LtEq:   cmp="<="; break;
        case TK::Gt:   cmp=">";  break; case TK::GtEq:   cmp=">="; break;
        case TK::And:
            o << "("; emit_list_cond(n->left.get(), o, vt, lv, ifns, ffns);
            o << "&&"; emit_list_cond(n->right.get(), o, vt, lv, ifns, ffns); o << ")"; return;
        case TK::Or:
            o << "("; emit_list_cond(n->left.get(), o, vt, lv, ifns, ffns);
            o << "||"; emit_list_cond(n->right.get(), o, vt, lv, ifns, ffns); o << ")"; return;
        default: break;
        }
        if (cmp) {
            auto l = infer_type_l(n->left.get(), vt, lv, ifns, ffns);
            auto r = infer_type_l(n->right.get(), vt, lv, ifns, ffns);
            bool use_float = (l && *l) || (r && *r);
            o << "(";
            if (use_float) {
                emit_as_float_l(n->left.get(), o, vt, lv, ifns, ffns); o << cmp;
                emit_as_float_l(n->right.get(), o, vt, lv, ifns, ffns);
            } else {
                emit_as_int_l(n->left.get(), o, vt, lv, ifns, ffns); o << cmp;
                emit_as_int_l(n->right.get(), o, vt, lv, ifns, ffns);
            }
            o << ")"; return;
        }
    }
    if (auto* n = dynamic_cast<const UnaryExpr*>(e)) {
        if (n->op == TokenKind::Not) {
            o << "!("; emit_list_cond(n->operand.get(), o, vt, lv, ifns, ffns); o << ")"; return;
        }
    }
    auto t = infer_type_l(e, vt, lv, ifns, ffns);
    if (t && *t) { o << "("; emit_as_float_l(e, o, vt, lv, ifns, ffns); o << "!=0.0)"; }
    else         { o << "("; emit_as_int_l(e, o, vt, lv, ifns, ffns);   o << "!=0LL)"; }
}

static bool check_list_block(const Block& b, VTMap& vt, LVMap& lv,
                              const VarSet& ifns, const VarSet& ffns, bool for_main = false,
                              const std::vector<const FnDeclStmt*>* all_fns = nullptr,
                              LFnMap* lfns = nullptr);

static bool check_list_stmt(const StmtNode* s, VTMap& vt, LVMap& lv,
                             const VarSet& ifns, const VarSet& ffns, bool for_main,
                             const std::vector<const FnDeclStmt*>* all_fns,
                             LFnMap* lfns)
{
    if (dynamic_cast<const FnDeclStmt*>(s)) return true;
    if (auto* n = dynamic_cast<const LetStmt*>(s)) {
        if (n->bind_kind != LetBindKind::Simple || n->values.size() != 1) return false;
        if (auto* le = dynamic_cast<const ListExpr*>(n->values[0].get())) {
            if (le->elements.empty()) { lv[n->names[0]] = ListInfo{}; return true; }
            // Non-empty list literal: all elements must be typed
            bool all_float = false;
            for (auto& elem : le->elements) {
                auto t = infer_type_l(elem.get(), vt, lv, ifns, ffns);
                if (!t) return false;
                if (*t) all_float = true;
            }
            ListInfo li; li.is_float = all_float; li.is_fill = true; li.is_literal = true;
            for (auto& elem : le->elements) li.literal_elems.push_back(elem.get());
            lv[n->names[0]] = std::move(li);
            return true;
        }
        auto t = infer_type_l(n->values[0].get(), vt, lv, ifns, ffns);
        if (!t) return false;
        for (auto& nm : n->names) vt[nm] = *t;
        return true;
    }
    if (auto* n = dynamic_cast<const AssignStmt*>(s)) {
        if (n->lvalues.size() != 1 || n->values.size() != 1) return false;
        if (auto* ie = dynamic_cast<const IndexExpr*>(n->lvalues[0].get())) {
            auto* lid = dynamic_cast<const IdentExpr*>(ie->object.get());
            if (!lid) return false;
            auto it = lv.find(lid->name);
            bool found = it != lv.end() && (it->second.is_fill || it->second.is_literal);
            if (!found && tl_glv) {
                auto git = tl_glv->find(lid->name);
                found = git != tl_glv->end() && (git->second.is_fill || git->second.is_literal);
            }
            if (!found) return false;
            auto idx = infer_type_l(ie->index.get(), vt, lv, ifns, ffns);
            if (!idx || *idx) return false;
            return infer_type_l(n->values[0].get(), vt, lv, ifns, ffns).has_value();
        }
        auto* lv_id = dynamic_cast<const IdentExpr*>(n->lvalues[0].get());
        if (!lv_id) return false;
        bool in_local = vt.count(lv_id->name) > 0;
        bool in_global = tl_gvt && tl_gvt->count(lv_id->name) > 0;
        if (!in_local && !in_global) return false;
        auto t = infer_type_l(n->values[0].get(), vt, lv, ifns, ffns);
        if (!t) return false;
        if (in_local && !vt[lv_id->name] && *t) vt[lv_id->name] = true;
        return true;
    }
    if (auto* n = dynamic_cast<const AugAssignStmt*>(s)) {
        auto* lv_id = dynamic_cast<const IdentExpr*>(n->lvalue.get());
        if (!lv_id) return false;
        bool in_local = vt.count(lv_id->name) > 0;
        bool in_global = tl_gvt && tl_gvt->count(lv_id->name) > 0;
        if (!in_local && !in_global) return false;
        return infer_type_l(n->value.get(), vt, lv, ifns, ffns).has_value();
    }
    if (auto* n = dynamic_cast<const IfStmt*>(s)) {
        for (auto& br : n->branches) {
            if (br.cond && !is_list_cond(br.cond.get(), vt, lv, ifns, ffns)) return false;
            VTMap local = vt;
            if (!check_list_block(br.body, local, lv, ifns, ffns, for_main, all_fns, lfns)) return false;
            for (auto& [nm, t] : local)
                if (!vt.count(nm)) vt[nm] = t; else if (!vt[nm] && t) vt[nm] = true;
        }
        return true;
    }
    if (auto* n = dynamic_cast<const WhileStmt*>(s)) {
        if (!is_list_cond(n->cond.get(), vt, lv, ifns, ffns)) return false;
        VTMap local = vt;
        if (!check_list_block(n->body, local, lv, ifns, ffns, for_main, all_fns, lfns)) return false;
        for (auto& [nm, t] : local)
            if (!vt.count(nm)) vt[nm] = t; else if (!vt[nm] && t) vt[nm] = true;
        return true;
    }
    if (auto* n = dynamic_cast<const ForStmt*>(s)) {
        // Helper: decode both append(lst,val) and lst.append(val) → (list_name, val_expr)
        auto decode_append = [](const ExprStmt* es, std::string& lst_nm, const ExprNode*& val)
            -> bool {
            auto* ce = dynamic_cast<const CallExpr*>(es->expr.get());
            if (!ce) return false;
            if (auto* id = dynamic_cast<const IdentExpr*>(ce->callee.get())) {
                if (id->name == "append" && ce->args.size() == 2) {
                    if (auto* lid = dynamic_cast<const IdentExpr*>(ce->args[0].value.get())) {
                        lst_nm = lid->name; val = ce->args[1].value.get(); return true;
                    }
                }
            }
            if (auto* fe = dynamic_cast<const FieldExpr*>(ce->callee.get())) {
                if (fe->field == "append" && ce->args.size() == 1) {
                    if (auto* lid = dynamic_cast<const IdentExpr*>(fe->object.get())) {
                        lst_nm = lid->name; val = ce->args[0].value.get(); return true;
                    }
                }
            }
            return false;
        };

        if (!n->range_end) {
            // For-in loop: source must be a known typed array
            auto* src_id = dynamic_cast<const IdentExpr*>(n->source.get());
            if (!src_id) return false;
            auto it = lv.find(src_id->name);
            bool in_local = it != lv.end() && (it->second.is_fill || it->second.is_literal);
            bool in_global = !in_local && tl_glv && tl_glv->count(src_id->name) &&
                             (tl_glv->at(src_id->name).is_fill || tl_glv->at(src_id->name).is_literal);
            if (!in_local && !in_global) return false;
            bool elem_float = in_local ? it->second.is_float : tl_glv->at(src_id->name).is_float;
            VTMap local = vt; local[n->iter1] = elem_float;
            if (!check_list_block(n->body, local, lv, ifns, ffns, for_main, all_fns, lfns)) return false;
            for (auto& [nm, t] : local) {
                if (nm == n->iter1) continue;
                if (!vt.count(nm)) vt[nm] = t; else if (!vt[nm] && t) vt[nm] = true;
            }
            return true;
        }

        // Detect fill loop: body = scalar assigns + append calls, source must be 0
        auto* src_lit = dynamic_cast<const IntLitExpr*>(n->source.get());
        if (src_lit && src_lit->value == 0) {
            VTMap local = vt; local[n->iter1] = false;
            std::vector<std::pair<std::string, const ExprNode*>> fills;
            std::unordered_set<std::string> seen;
            bool all_fill = !n->body.empty();
            for (auto& bs : n->body) {
                // Allow scalar assigns to existing variables
                if (auto* as = dynamic_cast<const AssignStmt*>(bs.get())) {
                    if (as->lvalues.size() == 1 && as->values.size() == 1) {
                        auto* lid2 = dynamic_cast<const IdentExpr*>(as->lvalues[0].get());
                        if (lid2 && vt.count(lid2->name)) {
                            auto t = infer_type_l(as->values[0].get(), local, lv, ifns, ffns);
                            if (t) { local[lid2->name] = *t; continue; }
                        }
                    }
                    all_fill = false; break;
                }
                auto* es = dynamic_cast<const ExprStmt*>(bs.get());
                if (!es) { all_fill = false; break; }
                std::string lst_nm; const ExprNode* val_e = nullptr;
                if (!decode_append(es, lst_nm, val_e)) { all_fill = false; break; }
                auto it = lv.find(lst_nm);
                if (it == lv.end() || it->second.is_fill || seen.count(lst_nm))
                    { all_fill = false; break; }
                fills.emplace_back(lst_nm, val_e);
                seen.insert(lst_nm);
            }
            if (all_fill && !fills.empty()) {
                auto et = infer_type_l(n->range_end.get(), vt, lv, ifns, ffns);
                if (!et || *et) return false;
                for (auto& [lst_nm, val_e] : fills) {
                    auto val_t = infer_type_l(val_e, local, lv, ifns, ffns);
                    if (!val_t) return false;
                    { ListInfo li; li.is_float = *val_t; li.is_fill = true;
                      li.is_bool = !*val_t && dynamic_cast<const BoolLitExpr*>(val_e);
                      li.size_expr = n->range_end.get(); li.init_expr = val_e;
                      lv[lst_nm] = std::move(li); }
                }
                return true;
            }
        }
        // Regular for loop
        auto et = infer_type_l(n->range_end.get(), vt, lv, ifns, ffns);
        if (!et || *et) return false;
        auto st = infer_type_l(n->source.get(), vt, lv, ifns, ffns);
        if (!st || *st) return false;
        VTMap local = vt; local[n->iter1] = false;
        if (!check_list_block(n->body, local, lv, ifns, ffns, for_main, all_fns, lfns)) return false;
        for (auto& [nm, t] : local) {
            if (nm == n->iter1) continue;
            if (!vt.count(nm)) vt[nm] = t; else if (!vt[nm] && t) vt[nm] = true;
        }
        return true;
    }
    if (auto* n = dynamic_cast<const ReturnStmt*>(s)) {
        if (n->values.empty()) return true;  // void return
        if (dynamic_cast<const NoneLitExpr*>(n->values[0].get())) return true;  // return none → void
        if (n->values.size() != 1) return false;
        return infer_type_l(n->values[0].get(), vt, lv, ifns, ffns).has_value();
    }
    if (auto* n = dynamic_cast<const ExprStmt*>(s)) {
        // In function-body context without classification mode: always OK
        if (!for_main && !all_fns) return true;
        if (auto* ce = dynamic_cast<const CallExpr*>(n->expr.get())) {
            if (auto* id = dynamic_cast<const IdentExpr*>(ce->callee.get())) {
                if (id->name == "print") {
                    if (!for_main) return true;
                    for (auto& arg : ce->args) {
                        if (infer_type_l(arg.value.get(), vt, lv, ifns, ffns)) continue;
                        // Arg might be a list-fn call — try to classify and accept
                        bool accepted = false;
                        if (all_fns && lfns) {
                            if (auto* ice = dynamic_cast<const CallExpr*>(arg.value.get())) {
                                if (auto* iid = dynamic_cast<const IdentExpr*>(ice->callee.get())) {
                                    if (lfns->count(iid->name)) { accepted = true; }
                                    else for (auto* fn2 : *all_fns) {
                                        if (fn2->name != iid->name || fn2->params.size() != ice->args.size()) continue;
                                        std::vector<LFnParam> p2;
                                        bool ok2 = true;
                                        for (size_t k = 0; k < fn2->params.size(); k++) {
                                            auto* ae2 = ice->args[k].value.get();
                                            if (auto* lid2 = dynamic_cast<const IdentExpr*>(ae2)) {
                                                auto it2 = lv.find(lid2->name);
                                                if (it2 != lv.end() && it2->second.is_fill)
                                                    { p2.push_back({true,it2->second.is_float}); continue; }
                                            }
                                            auto t2 = infer_type_l(ae2, vt, lv, ifns, ffns);
                                            if (!t2) { ok2=false; break; }
                                            p2.push_back({false,*t2});
                                        }
                                        if (!ok2) break;
                                        VTMap fvt2; LVMap flv2;
                                        for (size_t k=0; k<fn2->params.size(); k++) {
                                            if (p2[k].is_list) { ListInfo li; li.is_float=p2[k].is_float; li.is_fill=true; flv2[fn2->params[k].name]=std::move(li); }
                                            else fvt2[fn2->params[k].name]=p2[k].is_float;
                                        }
                                        if (lfns) (*lfns)[iid->name]=p2;
                                        GCtxGuard gctx(vt,lv,false);
                                        if (check_list_block(fn2->body,fvt2,flv2,ifns,ffns,false,all_fns,lfns)) {
                                            accepted=true;
                                        } else { if (lfns) lfns->erase(iid->name); }
                                        break;
                                    }
                                }
                            }
                        }
                        if (!accepted) return false;
                    }
                    return true;
                }
                if (ifns.count(id->name) || ffns.count(id->name)) {
                    // If any arg is a list, don't use scalar JIT — fall through to list-fn path
                    bool has_list = false;
                    for (auto& arg : ce->args) {
                        if (auto* lid2 = dynamic_cast<const IdentExpr*>(arg.value.get()))
                            if (lv.count(lid2->name) && lv.at(lid2->name).is_fill) { has_list=true; break; }
                    }
                    if (!has_list) return true;
                }
                if (lfns && lfns->count(id->name)) return true;
                // Try to classify as a list function using call-site arg types
                if (all_fns && lfns) {
                    for (auto* fn : *all_fns) {
                        if (fn->name != id->name) continue;
                        if (fn->params.size() != ce->args.size()) break;
                        std::vector<LFnParam> params;
                        bool ok = true;
                        for (size_t k = 0; k < fn->params.size(); k++) {
                            auto* ae = ce->args[k].value.get();
                            if (auto* lid = dynamic_cast<const IdentExpr*>(ae)) {
                                auto it = lv.find(lid->name);
                                if (it != lv.end() && it->second.is_fill) {
                                    params.push_back({true, it->second.is_float}); continue;
                                }
                            }
                            auto t = infer_type_l(ae, vt, lv, ifns, ffns);
                            if (!t) { ok = false; break; }
                            params.push_back({false, *t});
                        }
                        if (!ok) break;
                        VTMap fn_vt; LVMap fn_lv;
                        for (size_t k = 0; k < fn->params.size(); k++) {
                            if (params[k].is_list)
                                { ListInfo li; li.is_float = params[k].is_float; li.is_fill = true;
                                  fn_lv[fn->params[k].name] = std::move(li); }
                            else
                                fn_vt[fn->params[k].name] = params[k].is_float;
                        }
                        {
                            // Pre-register to handle recursive calls
                            if (lfns) (*lfns)[id->name] = params;
                            GCtxGuard gctx(vt, lv, false);
                            if (check_list_block(fn->body, fn_vt, fn_lv, ifns, ffns, false, all_fns, lfns)) {
                                return true;
                            }
                            if (lfns) lfns->erase(id->name);
                        }
                        break;
                    }
                }
            }
        }
        // Unknown call/statement: the emitter cannot express it and would
        // silently drop it (e.g. a call to a non-JIT-able function), producing
        // wrong results. Reject so the whole function falls back to the
        // interpreter. (The !for_main && !all_fns permissive case already
        // returned true at the top of the ExprStmt branch.)
        return false;
    }
    if (dynamic_cast<const BreakStmt*>(s) || dynamic_cast<const ContinueStmt*>(s)) return true;
    return false;
}

static bool check_list_block(const Block& b, VTMap& vt, LVMap& lv,
                              const VarSet& ifns, const VarSet& ffns, bool for_main,
                              const std::vector<const FnDeclStmt*>* all_fns,
                              LFnMap* lfns)
{
    for (auto& s : b) if (!check_list_stmt(s.get(), vt, lv, ifns, ffns, for_main, all_fns, lfns)) return false;
    return true;
}

static void emit_list_block(const Block& b, std::ostream& o, const VTMap& vt, const LVMap& lv,
                             const VarSet& ifns, const VarSet& ffns, int ind,
                             const LFnMap& lfns = {});

static void emit_list_stmt(const StmtNode* s, std::ostream& o, const VTMap& vt, const LVMap& lv,
                            const VarSet& ifns, const VarSet& ffns, int ind,
                            const LFnMap& lfns = {})
{
    std::string sp(ind * 4, ' ');
    if (dynamic_cast<const FnDeclStmt*>(s)) return;
    if (auto* n = dynamic_cast<const LetStmt*>(s)) {
        // List var: check for global literal list or local fill loop
        if (lv.count(n->names[0]) || (tl_glv && tl_glv->count(n->names[0]))) {
            auto& li = lv.count(n->names[0]) ? lv.at(n->names[0]) : tl_glv->at(n->names[0]);
            bool is_global = tl_glv && tl_glv->count(n->names[0]);
            std::string pfx = is_global ? "g_vptr_" : "_vptr_";
            std::string lenpfx = is_global ? "g_vlen_" : "_vlen_";
            if (li.is_literal) {
                // Emit element-by-element initialization
                if (!is_global) {
                    // Local literal list: declare and initialize
                    const char* tn = li.is_float ? "double" : "int64_t";
                    o << sp << "int64_t " << lenpfx << n->names[0] << " = "
                      << (int64_t)li.literal_elems.size() << "LL;\n";
                    o << sp << tn << "* " << pfx << n->names[0] << " = (" << tn
                      << "*)malloc(" << lenpfx << n->names[0] << " * sizeof(" << tn << "));\n";
                }
                for (size_t i = 0; i < li.literal_elems.size(); i++) {
                    o << sp << pfx << n->names[0] << "[" << i << "] = ";
                    if (li.is_float) emit_as_float_l(li.literal_elems[i], o, vt, lv, ifns, ffns);
                    else             emit_as_int_l(li.literal_elems[i], o, vt, lv, ifns, ffns);
                    o << ";\n";
                }
                return;
            }
            // Empty list / fill-loop list: handled by fill loop emitter
            return;
        }
        // Scalar var
        auto it = vt.find(n->names[0]);
        bool is_float = (it != vt.end()) ? it->second :
                        (tl_gvt && tl_gvt->count(n->names[0]) ? tl_gvt->at(n->names[0]) : false);
        bool is_global_scalar = tl_is_main && tl_gvt && tl_gvt->count(n->names[0]);
        if (is_global_scalar) {
            // Assign to pre-declared global
            o << sp << "g_v_" << n->names[0] << " = ";
        } else {
            o << sp << (is_float ? "double" : "int64_t") << " _v_" << n->names[0] << " = ";
        }
        if (is_float) emit_as_float_l(n->values[0].get(), o, vt, lv, ifns, ffns);
        else          emit_as_int_l(n->values[0].get(), o, vt, lv, ifns, ffns);
        o << ";\n"; return;
    }
    if (auto* n = dynamic_cast<const AssignStmt*>(s)) {
        if (auto* ie = dynamic_cast<const IndexExpr*>(n->lvalues[0].get())) {
            auto* lid = dynamic_cast<const IdentExpr*>(ie->object.get());
            bool is_global_list = tl_glv && tl_glv->count(lid->name) && tl_glv->at(lid->name).is_literal;
            bool elem_float = is_global_list ? tl_glv->at(lid->name).is_float :
                              (lv.count(lid->name) && lv.at(lid->name).is_float);
            std::string pfx = is_global_list ? "g_vptr_" : "_vptr_";
            o << sp << pfx << lid->name << "[";
            emit_as_int_l(ie->index.get(), o, vt, lv, ifns, ffns);
            o << "] = ";
            if (elem_float) emit_as_float_l(n->values[0].get(), o, vt, lv, ifns, ffns);
            else            emit_as_int_l(n->values[0].get(), o, vt, lv, ifns, ffns);
            o << ";\n"; return;
        }
        auto* lv_id = dynamic_cast<const IdentExpr*>(n->lvalues[0].get());
        bool is_global_sc = tl_is_main && tl_gvt && tl_gvt->count(lv_id->name);
        bool is_float;
        if (is_global_sc) is_float = tl_gvt->at(lv_id->name);
        else { auto it = vt.find(lv_id->name); is_float = it != vt.end() && it->second; }
        o << sp << (is_global_sc ? "g_v_" : "_v_") << lv_id->name << " = ";
        if (is_float) emit_as_float_l(n->values[0].get(), o, vt, lv, ifns, ffns);
        else          emit_as_int_l(n->values[0].get(), o, vt, lv, ifns, ffns);
        o << ";\n"; return;
    }
    if (auto* n = dynamic_cast<const AugAssignStmt*>(s)) {
        auto* lv_id = dynamic_cast<const IdentExpr*>(n->lvalue.get());
        bool is_global_sc = tl_is_main && tl_gvt && tl_gvt->count(lv_id->name);
        bool is_float;
        if (is_global_sc) is_float = tl_gvt->at(lv_id->name);
        else { auto it = vt.find(lv_id->name); is_float = it != vt.end() && it->second; }
        std::string vname = is_global_sc ? ("g_v_" + lv_id->name) : ("_v_" + lv_id->name);
        using TK = TokenKind;
        if (!is_float && n->op == TK::SlashSlashEq) {
            o << sp << vname << " = syn_idiv(" << vname << ",";
            emit_as_int_l(n->value.get(), o, vt, lv, ifns, ffns); o << ");\n"; return;
        }
        if (!is_float && n->op == TK::PercentEq) {
            o << sp << vname << " = syn_imod(" << vname << ",";
            emit_as_int_l(n->value.get(), o, vt, lv, ifns, ffns); o << ");\n"; return;
        }
        const char* op = "+=";
        switch (n->op) {
        case TK::PlusEq: op="+="; break; case TK::MinusEq: op="-="; break;
        case TK::StarEq: op="*="; break; case TK::SlashEq: op="/="; break;
        default: break;
        }
        o << sp << vname << " " << op << " ";
        if (is_float) emit_as_float_l(n->value.get(), o, vt, lv, ifns, ffns);
        else          emit_as_int_l(n->value.get(), o, vt, lv, ifns, ffns);
        o << ";\n"; return;
    }
    if (auto* n = dynamic_cast<const IfStmt*>(s)) {
        bool first = true;
        for (auto& br : n->branches) {
            if (br.cond) {
                o << sp << (first ? "if (" : "else if (");
                emit_list_cond(br.cond.get(), o, vt, lv, ifns, ffns);
                o << ") {\n"; emit_list_block(br.body, o, vt, lv, ifns, ffns, ind+1, lfns); o << sp << "}\n";
            } else {
                o << sp << "else {\n"; emit_list_block(br.body, o, vt, lv, ifns, ffns, ind+1, lfns); o << sp << "}\n";
            }
            first = false;
        }
        return;
    }
    if (auto* n = dynamic_cast<const WhileStmt*>(s)) {
        o << sp << "while ("; emit_list_cond(n->cond.get(), o, vt, lv, ifns, ffns);
        o << ") {\n"; emit_list_block(n->body, o, vt, lv, ifns, ffns, ind+1, lfns); o << sp << "}\n"; return;
    }
    if (auto* n = dynamic_cast<const ForStmt*>(s)) {
        // Helper: decode both append(lst,val) and lst.append(val)
        auto decode_append_emit = [](const ExprStmt* es, std::string& lst_nm, const ExprNode*& val) -> bool {
            auto* ce = dynamic_cast<const CallExpr*>(es->expr.get());
            if (!ce) return false;
            if (auto* id = dynamic_cast<const IdentExpr*>(ce->callee.get())) {
                if (id->name == "append" && ce->args.size() == 2) {
                    if (auto* lid = dynamic_cast<const IdentExpr*>(ce->args[0].value.get())) {
                        lst_nm = lid->name; val = ce->args[1].value.get(); return true;
                    }
                }
            }
            if (auto* fe = dynamic_cast<const FieldExpr*>(ce->callee.get())) {
                if (fe->field == "append" && ce->args.size() == 1) {
                    if (auto* lid = dynamic_cast<const IdentExpr*>(fe->object.get())) {
                        lst_nm = lid->name; val = ce->args[0].value.get(); return true;
                    }
                }
            }
            return false;
        };

        // For-in loop over typed array
        if (!n->range_end) {
            auto* src_id = dynamic_cast<const IdentExpr*>(n->source.get());
            auto it = lv.find(src_id->name);
            bool in_local = it != lv.end() && (it->second.is_fill || it->second.is_literal);
            const ListInfo& li = in_local ? it->second : tl_glv->at(src_id->name);
            bool use_g = !in_local || (tl_is_main && li.is_literal);
            std::string ptr_pfx = use_g ? "g_vptr_" : "_vptr_";
            std::string len_pfx = use_g ? "g_vlen_" : "_vlen_";
            const char* tn = li.is_float ? "double" : "int64_t";
            std::string idx_var = "_vfi_" + n->iter1;
            VTMap body_vt = vt; body_vt[n->iter1] = li.is_float;
            o << sp << "for (int64_t " << idx_var << " = 0; " << idx_var << " < " << len_pfx << src_id->name << "; " << idx_var << "++) {\n";
            o << sp << "    " << tn << " _v_" << n->iter1 << " = " << ptr_pfx << src_id->name << "[" << idx_var << "];\n";
            emit_list_block(n->body, o, body_vt, lv, ifns, ffns, ind+1, lfns);
            o << sp << "}\n"; return;
        }

        // Detect fill loop: body = scalar assigns + append calls to known fill-target lists
        bool is_fill = !n->body.empty();
        std::vector<std::string> fill_lists;
        for (auto& bs : n->body) {
            if (dynamic_cast<const AssignStmt*>(bs.get())) continue;
            auto* es = dynamic_cast<const ExprStmt*>(bs.get());
            if (!es) { is_fill = false; break; }
            std::string lst_nm; const ExprNode* val = nullptr;
            if (!decode_append_emit(es, lst_nm, val)) { is_fill = false; break; }
            if (!lv.count(lst_nm) || !lv.at(lst_nm).is_fill) { is_fill = false; break; }
            fill_lists.push_back(lst_nm);
        }
        if (is_fill && !fill_lists.empty()) {
            for (auto& lst_nm : fill_lists) {
                auto& li = lv.at(lst_nm);
                const char* tn = li.is_float ? "double" : (li.is_bool ? "int8_t" : "int64_t");
                o << sp << "int64_t _vlen_" << lst_nm << " = ";
                emit_as_int_l(li.size_expr, o, vt, lv, ifns, ffns);
                o << ";\n";
                o << sp << tn << "* _vptr_" << lst_nm << " = (" << tn << "*)malloc(_vlen_" << lst_nm << " * sizeof(" << tn << "));\n";
                // Bool arrays: use memset for fast constant fill
                if (li.is_bool) {
                    auto* bl = dynamic_cast<const BoolLitExpr*>(li.init_expr);
                    o << sp << "memset(_vptr_" << lst_nm << ", " << (bl && bl->value ? 1 : 0)
                      << ", _vlen_" << lst_nm << " * sizeof(" << tn << "));\n";
                }
            }
            // Skip element loop if all lists are bool (already memset above)
            bool all_bool = std::all_of(fill_lists.begin(), fill_lists.end(),
                                        [&](const std::string& nm){ return lv.at(nm).is_bool; });
            if (!all_bool) {
            o << sp << "for (int64_t _v_" << n->iter1 << " = 0; _v_" << n->iter1 << " < _vlen_" << fill_lists[0] << "; _v_" << n->iter1 << "++) {\n";
            { VTMap fill_vt = vt; fill_vt[n->iter1] = false;
            for (auto& bs : n->body) {
                if (auto* as = dynamic_cast<const AssignStmt*>(bs.get())) {
                    auto* lid2 = dynamic_cast<const IdentExpr*>(as->lvalues[0].get());
                    auto it2 = fill_vt.find(lid2->name);
                    bool is_f = it2 != fill_vt.end() && it2->second;
                    bool is_glob_lhs = tl_is_main && tl_gvt && tl_gvt->count(lid2->name);
                    o << sp << "    " << (is_glob_lhs ? "g_v_" : "_v_") << lid2->name << " = ";
                    if (is_f) emit_as_float_l(as->values[0].get(), o, fill_vt, lv, ifns, ffns);
                    else      emit_as_int_l(as->values[0].get(), o, fill_vt, lv, ifns, ffns);
                    o << ";\n";
                } else if (auto* es = dynamic_cast<const ExprStmt*>(bs.get())) {
                    std::string lst_nm; const ExprNode* val_e = nullptr;
                    decode_append_emit(es, lst_nm, val_e);
                    auto& li = lv.at(lst_nm);
                    o << sp << "    _vptr_" << lst_nm << "[_v_" << n->iter1 << "] = ";
                    if (li.is_float) emit_as_float_l(val_e, o, fill_vt, lv, ifns, ffns);
                    else             emit_as_int_l(val_e, o, fill_vt, lv, ifns, ffns);
                    o << ";\n";
                }
            } }
            o << sp << "}\n";
            }  // !all_bool
            return;
        }
        // Regular for loop
        o << sp << "for (int64_t _v_" << n->iter1 << " = ";
        emit_as_int_l(n->source.get(), o, vt, lv, ifns, ffns);
        o << "; _v_" << n->iter1 << " < ";
        emit_as_int_l(n->range_end.get(), o, vt, lv, ifns, ffns);
        o << "; _v_" << n->iter1;
        if (n->range_step) { o << " += "; emit_as_int_l(n->range_step.get(), o, vt, lv, ifns, ffns); }
        else o << "++";
        { VTMap body_vt = vt; body_vt[n->iter1] = false;
        o << ") {\n"; emit_list_block(n->body, o, body_vt, lv, ifns, ffns, ind+1, lfns); }
        o << sp << "}\n"; return;
    }
    if (auto* n = dynamic_cast<const ReturnStmt*>(s)) {
        // void return: empty or "return none"
        if (n->values.empty() || dynamic_cast<const NoneLitExpr*>(n->values[0].get()))
            { o << sp << "return;\n"; return; }
        auto t = infer_type_l(n->values[0].get(), vt, lv, ifns, ffns);
        o << sp << "return ";
        if (t && *t) emit_as_float_l(n->values[0].get(), o, vt, lv, ifns, ffns);
        else         emit_as_int_l(n->values[0].get(), o, vt, lv, ifns, ffns);
        o << ";\n"; return;
    }
    if (auto* n = dynamic_cast<const ExprStmt*>(s)) {
        if (auto* ce = dynamic_cast<const CallExpr*>(n->expr.get())) {
            if (auto* id = dynamic_cast<const IdentExpr*>(ce->callee.get())) {
                if (id->name == "print") {
                    bool first = true;
                    for (auto& arg : ce->args) {
                        if (!first) o << sp << "fputc(' ', stdout);\n";
                        auto t = infer_type_l(arg.value.get(), vt, lv, ifns, ffns);
                        if (t && *t) {
                            o << sp << "syn_print_double(";
                            emit_as_float_l(arg.value.get(), o, vt, lv, ifns, ffns); o << ");\n";
                        } else {
                            o << sp << "printf(\"%lld\", (long long)(";
                            emit_as_int_l(arg.value.get(), o, vt, lv, ifns, ffns); o << "));\n";
                        }
                        first = false;
                    }
                    o << sp << "fputc('\\n', stdout);\n"; return;
                }
                if (ifns.count(id->name) || ffns.count(id->name)) {
                    // Prefer list-fn if any arg is a list pointer (local or global)
                    bool has_list = false;
                    for (auto& arg : ce->args)
                        if (auto* lid2 = dynamic_cast<const IdentExpr*>(arg.value.get()))
                            if ((lv.count(lid2->name) && lv.at(lid2->name).is_fill) ||
                                (tl_glv && tl_glv->count(lid2->name))) { has_list=true; break; }
                    if (!has_list) {
                        if (ifns.count(id->name)) {
                            o << sp << "_jit_i_" << id->name << "(";
                            for (size_t i=0; i<ce->args.size(); ++i) {
                                if (i) o << ","; emit_as_int_l(ce->args[i].value.get(), o, vt, lv, ifns, ffns);
                            }
                            o << ");\n"; return;
                        } else {
                            o << sp << "_jit_f_" << id->name << "(";
                            for (size_t i=0; i<ce->args.size(); ++i) {
                                if (i) o << ","; emit_as_float_l(ce->args[i].value.get(), o, vt, lv, ifns, ffns);
                            }
                            o << ");\n"; return;
                        }
                    }
                    // has list args → fall through to lfns emit
                }
                if (lfns.count(id->name)) {
                    auto& params = lfns.at(id->name);
                    o << sp << "jit_l_" << id->name << "(";
                    bool first_arg = true;
                    for (size_t k = 0; k < ce->args.size(); k++) {
                        if (!first_arg) o << ", ";
                        first_arg = false;
                        auto* ae = ce->args[k].value.get();
                        if (k < params.size() && params[k].is_list) {
                            auto* lid = dynamic_cast<const IdentExpr*>(ae);
                            if (lid) {
                                bool glb = tl_glv && tl_glv->count(lid->name) && tl_glv->at(lid->name).is_literal;
                                o << (glb ? "g_vptr_" : "_vptr_") << lid->name << ", "
                                  << (glb ? "g_vlen_" : "_vlen_") << lid->name;
                            } else { o << "0, 0"; }
                        } else {
                            bool is_f = k < params.size() && params[k].is_float;
                            if (is_f) emit_as_float_l(ae, o, vt, lv, ifns, ffns);
                            else      emit_as_int_l(ae, o, vt, lv, ifns, ffns);
                        }
                    }
                    o << ");\n"; return;
                }
            }
        }
        return;
    }
    if (dynamic_cast<const BreakStmt*>(s))    { o << sp << "break;\n"; return; }
    if (dynamic_cast<const ContinueStmt*>(s)) { o << sp << "continue;\n"; return; }
}

static void emit_list_block(const Block& b, std::ostream& o, const VTMap& vt, const LVMap& lv,
                             const VarSet& ifns, const VarSet& ffns, int ind, const LFnMap& lfns)
{
    // Pre-scan to build complete local vt so later stmts see types of earlier lets
    VTMap local = vt;
    for (auto& stmt : b) {
        if (auto* ls = dynamic_cast<const LetStmt*>(stmt.get())) {
            if (ls->bind_kind == LetBindKind::Simple && ls->names.size() == 1 &&
                ls->values.size() == 1 && !lv.count(ls->names[0]) &&
                !(tl_glv && tl_glv->count(ls->names[0]))) {
                auto t = infer_type_l(ls->values[0].get(), local, lv, ifns, ffns);
                local[ls->names[0]] = (t && *t);
            }
        }
    }
    for (auto& s : b) emit_list_stmt(s.get(), o, local, lv, ifns, ffns, ind, lfns);
}

// ── List function emitter ──────────────────────────────────────────────────────

static std::optional<bool> infer_list_fn_ret(const FnDeclStmt* fn, const VTMap& fn_vt,
                                               const LVMap& fn_lv, const VarSet& ifns,
                                               const VarSet& ffns)
{
    // Pre-scan top-level lets to collect local var types for return type inference
    VTMap local = fn_vt;
    for (auto& s : fn->body) {
        if (auto* ls = dynamic_cast<const LetStmt*>(s.get())) {
            if (ls->bind_kind == LetBindKind::Simple && ls->names.size() == 1 &&
                ls->values.size() == 1 && !fn_lv.count(ls->names[0])) {
                auto t = infer_type_l(ls->values[0].get(), local, fn_lv, ifns, ffns);
                if (t) local[ls->names[0]] = *t;
            }
        }
        auto* rs = dynamic_cast<const ReturnStmt*>(s.get());
        if (rs && !rs->values.empty() && !dynamic_cast<const NoneLitExpr*>(rs->values[0].get()))
            return infer_type_l(rs->values[0].get(), local, fn_lv, ifns, ffns);
    }
    return std::nullopt;
}

static void emit_list_fn(const FnDeclStmt* fn, const std::vector<LFnParam>& params,
                          const std::vector<const FnDeclStmt*>& all_fns,
                          std::ostream& o, const VarSet& ifns, const VarSet& ffns,
                          const LFnMap& lfns, LFnRetMap& lfn_rets)
{
    // Build VTMap and LVMap for body
    VTMap fn_vt; LVMap fn_lv;
    for (size_t k = 0; k < fn->params.size(); k++) {
        if (params[k].is_list) {
            ListInfo li; li.is_float = params[k].is_float; li.is_fill = true;
            fn_lv[fn->params[k].name] = std::move(li);
        } else {
            fn_vt[fn->params[k].name] = params[k].is_float;
        }
    }
    // Detect return type (using global context already set)
    auto ret_t = infer_list_fn_ret(fn, fn_vt, fn_lv, ifns, ffns);
    lfn_rets[fn->name] = ret_t;

    // Build signature
    const char* ret_str = "void";
    if (ret_t) ret_str = *ret_t ? "double" : "int64_t";
    o << "static " << ret_str << " jit_l_" << fn->name << "(";
    bool first_param = true;
    for (size_t k = 0; k < fn->params.size(); k++) {
        if (!first_param) o << ", ";
        first_param = false;
        if (params[k].is_list) {
            const char* tn = params[k].is_float ? "double" : "int64_t";
            o << tn << "* _vptr_" << fn->params[k].name << ", int64_t _vlen_" << fn->params[k].name;
        } else {
            const char* tn = params[k].is_float ? "double" : "int64_t";
            o << tn << " _v_" << fn->params[k].name;
        }
    }
    o << ") {\n";
    emit_list_block(fn->body, o, fn_vt, fn_lv, ifns, ffns, 1, lfns);
    if (ret_t) o << "    return " << (*ret_t ? "0.0" : "0LL") << ";\n";
    o << "}\n\n";
}

// ════════════════════════════════════════════════════════════════════════════
// Generic value-typed JIT (HotSpot-style): compiles arbitrary functions that
// the int/float/list specializers reject (tuples, recursion over heap values).
// Everything is a NaN-boxed uint64_t; dynamic ops go through syn_rt_* helpers.
// Aggressive bail-out: any unsupported construct => function not value-JIT'd,
// runs in the interpreter (correctness preserved).
// ════════════════════════════════════════════════════════════════════════════

// ---- Analysis: is expr/stmt within the supported value-JIT subset? ----
static bool vjit_expr_ok(const ExprNode* e, const VarSet& vfns);

static bool vjit_call_ok(const CallExpr* c, const VarSet& vfns)
{
    auto* id = dynamic_cast<const IdentExpr*>(c->callee.get());
    if (!id || !vfns.count(id->name)) return false;   // only calls to value-JIT fns
    for (auto& a : c->args) if (!vjit_expr_ok(a.value.get(), vfns)) return false;
    return true;
}

static bool vjit_expr_ok(const ExprNode* e, const VarSet& vfns)
{
    if (!e) return false;
    if (dynamic_cast<const IntLitExpr*>(e))   return true;
    if (dynamic_cast<const FloatLitExpr*>(e)) return true;
    if (dynamic_cast<const BoolLitExpr*>(e))  return true;
    if (dynamic_cast<const NoneLitExpr*>(e))  return true;
    if (dynamic_cast<const IdentExpr*>(e))    return true;
    if (auto* u = dynamic_cast<const UnaryExpr*>(e)) {
        if (u->op != TokenKind::Minus && u->op != TokenKind::Not) return false;
        return vjit_expr_ok(u->operand.get(), vfns);
    }
    if (auto* b = dynamic_cast<const BinaryExpr*>(e)) {
        using TK = TokenKind;
        switch (b->op) {
        case TK::Plus: case TK::Minus: case TK::Star: case TK::Slash:
        case TK::SlashSlash: case TK::Percent: case TK::StarStar:
        case TK::EqEq: case TK::BangEq: case TK::Lt: case TK::LtEq:
        case TK::Gt: case TK::GtEq: case TK::And: case TK::Or:
            return vjit_expr_ok(b->left.get(), vfns) && vjit_expr_ok(b->right.get(), vfns);
        default: return false;
        }
    }
    if (auto* ix = dynamic_cast<const IndexExpr*>(e))
        return vjit_expr_ok(ix->object.get(), vfns) && vjit_expr_ok(ix->index.get(), vfns);
    if (auto* t = dynamic_cast<const TupleExpr*>(e)) {
        for (auto& el : t->elements) if (!vjit_expr_ok(el.get(), vfns)) return false;
        return true;
    }
    if (auto* c = dynamic_cast<const CallExpr*>(e)) return vjit_call_ok(c, vfns);
    return false;
}

static bool vjit_block_ok(const Block& b, const VarSet& vfns);

static bool vjit_stmt_ok(const StmtNode* s, const VarSet& vfns)
{
    if (auto* l = dynamic_cast<const LetStmt*>(s)) {
        if (l->bind_kind != LetBindKind::Simple || l->names.size() != 1 ||
            l->values.size() != 1) return false;
        return vjit_expr_ok(l->values[0].get(), vfns);
    }
    if (auto* r = dynamic_cast<const ReturnStmt*>(s)) {
        if (r->values.empty()) return true;
        if (r->values.size() != 1) return false;
        return vjit_expr_ok(r->values[0].get(), vfns);
    }
    if (auto* f = dynamic_cast<const IfStmt*>(s)) {
        for (auto& br : f->branches) {
            if (br.cond && !vjit_expr_ok(br.cond.get(), vfns)) return false;
            if (!vjit_block_ok(br.body, vfns)) return false;
        }
        return true;
    }
    if (auto* es = dynamic_cast<const ExprStmt*>(s))
        return vjit_expr_ok(es->expr.get(), vfns);
    return false;
}

static bool vjit_block_ok(const Block& b, const VarSet& vfns)
{
    for (auto& s : b) if (!vjit_stmt_ok(s.get(), vfns)) return false;
    return true;
}

// Fixpoint: a function is value-JIT-able if its whole body is in-subset given
// that its (mutually recursive) callees are also value-JIT-able.
static VarSet find_value_fns(const std::vector<const FnDeclStmt*>& fns,
                             const VarSet& ifns, const VarSet& ffns)
{
    VarSet result;
    for (auto* fn : fns)                          // seed with everything not already specialized
        if (!ifns.count(fn->name) && !ffns.count(fn->name))
            result.insert(fn->name);
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto* fn : fns) {
            if (!result.count(fn->name)) continue;
            if (!vjit_block_ok(fn->body, result)) { result.erase(fn->name); changed = true; }
        }
    }
    return result;
}

// ---- Codegen ----
struct VJitCtx {
    std::ostream& o;
    int& tmp;                       // unique temp counter
    const VarSet& vfns;
};

// Emit expression, return the name of a C variable/expression holding its Value.
static std::string vjit_emit(const ExprNode* e, VJitCtx& c, int ind)
{
    std::string sp(ind * 4, ' ');
    auto fresh = [&]() { return "_t" + std::to_string(c.tmp++); };

    if (auto* n = dynamic_cast<const IntLitExpr*>(e)) {
        std::string t = fresh();
        c.o << sp << "uint64_t " << t << " = syn_from_int(" << n->value << "LL);\n";
        return t;
    }
    if (auto* n = dynamic_cast<const FloatLitExpr*>(e)) {
        std::string t = fresh(); char buf[40];
        snprintf(buf, sizeof(buf), "%.17g", n->value);
        c.o << sp << "uint64_t " << t << " = syn_from_float(" << buf << ");\n";
        return t;
    }
    if (auto* n = dynamic_cast<const BoolLitExpr*>(e)) {
        std::string t = fresh();
        c.o << sp << "uint64_t " << t << " = " << (n->value ? "VAL_TRUE" : "VAL_FALSE") << ";\n";
        return t;
    }
    if (dynamic_cast<const NoneLitExpr*>(e)) {
        std::string t = fresh();
        c.o << sp << "uint64_t " << t << " = syn_none_val();\n";
        return t;
    }
    if (auto* n = dynamic_cast<const IdentExpr*>(e)) {
        return "_v_" + n->name;   // param/local (declared elsewhere)
    }
    if (auto* u = dynamic_cast<const UnaryExpr*>(e)) {
        std::string a = vjit_emit(u->operand.get(), c, ind);
        std::string t = fresh();
        if (u->op == TokenKind::Minus)
            c.o << sp << "uint64_t " << t << " = syn_rt_sub(syn_from_int(0LL), " << a << ");\n";
        else // Not
            c.o << sp << "uint64_t " << t << " = syn_rt_truthy(" << a << ") ? VAL_FALSE : VAL_TRUE;\n";
        return t;
    }
    if (auto* b = dynamic_cast<const BinaryExpr*>(e)) {
        using TK = TokenKind;
        if (b->op == TK::And || b->op == TK::Or) {
            // short-circuit: emit into a result temp
            std::string t = fresh();
            std::string l = vjit_emit(b->left.get(), c, ind);
            c.o << sp << "uint64_t " << t << " = " << l << ";\n";
            c.o << sp << "if (" << (b->op == TK::Or ? "!" : "") << "syn_rt_truthy(" << t << ")) {\n";
            std::string r = vjit_emit(b->right.get(), c, ind + 1);
            c.o << sp << "    " << t << " = " << r << ";\n";
            c.o << sp << "}\n";
            return t;
        }
        std::string l = vjit_emit(b->left.get(), c, ind);
        std::string r = vjit_emit(b->right.get(), c, ind);
        const char* fn = nullptr;
        switch (b->op) {
        case TK::Plus: fn="syn_rt_add"; break; case TK::Minus: fn="syn_rt_sub"; break;
        case TK::Star: fn="syn_rt_mul"; break; case TK::Slash: fn="syn_rt_div"; break;
        case TK::SlashSlash: fn="syn_rt_idiv"; break; case TK::Percent: fn="syn_rt_mod"; break;
        case TK::StarStar: fn="syn_rt_pow"; break;
        case TK::EqEq: fn="syn_rt_eq"; break; case TK::BangEq: fn="syn_rt_neq"; break;
        case TK::Lt: fn="syn_rt_lt"; break; case TK::LtEq: fn="syn_rt_lte"; break;
        case TK::Gt: fn="syn_rt_gt"; break; case TK::GtEq: fn="syn_rt_gte"; break;
        default: fn="syn_rt_add"; break;
        }
        std::string t = fresh();
        c.o << sp << "uint64_t " << t << " = " << fn << "(" << l << ", " << r << ");\n";
        return t;
    }
    if (auto* ix = dynamic_cast<const IndexExpr*>(e)) {
        std::string ob = vjit_emit(ix->object.get(), c, ind);
        std::string k  = vjit_emit(ix->index.get(), c, ind);
        std::string t = fresh();
        c.o << sp << "uint64_t " << t << " = syn_rt_index(" << ob << ", " << k << ");\n";
        return t;
    }
    if (auto* tup = dynamic_cast<const TupleExpr*>(e)) {
        std::vector<std::string> els;
        for (auto& el : tup->elements) els.push_back(vjit_emit(el.get(), c, ind));
        std::string arr = fresh(), t = fresh();
        c.o << sp << "uint64_t " << arr << "[] = {";
        for (size_t i = 0; i < els.size(); ++i) { if (i) c.o << ", "; c.o << els[i]; }
        c.o << "};\n";
        c.o << sp << "uint64_t " << t << " = syn_rt_tuple_new(" << arr << ", "
            << els.size() << ");\n";
        return t;
    }
    if (auto* call = dynamic_cast<const CallExpr*>(e)) {
        auto* id = static_cast<const IdentExpr*>(call->callee.get());
        std::vector<std::string> as;
        for (auto& a : call->args) as.push_back(vjit_emit(a.value.get(), c, ind));
        std::string t = fresh();
        c.o << sp << "uint64_t " << t << " = _jitv_" << id->name << "(";
        for (size_t i = 0; i < as.size(); ++i) { if (i) c.o << ", "; c.o << as[i]; }
        c.o << ");\n";
        return t;
    }
    // unreachable (analysis guarantees subset)
    std::string t = fresh();
    c.o << sp << "uint64_t " << t << " = syn_none_val();\n";
    return t;
}

static void vjit_emit_block(const Block& b, VJitCtx& c, int ind);

static void vjit_emit_stmt(const StmtNode* s, VJitCtx& c, int ind)
{
    std::string sp(ind * 4, ' ');
    if (auto* l = dynamic_cast<const LetStmt*>(s)) {
        std::string v = vjit_emit(l->values[0].get(), c, ind);
        c.o << sp << "uint64_t _v_" << l->names[0] << " = " << v << ";\n";
        return;
    }
    if (auto* r = dynamic_cast<const ReturnStmt*>(s)) {
        if (r->values.empty()) { c.o << sp << "return syn_none_val();\n"; return; }
        std::string v = vjit_emit(r->values[0].get(), c, ind);
        c.o << sp << "return " << v << ";\n";
        return;
    }
    if (auto* f = dynamic_cast<const IfStmt*>(s)) {
        // Emit as nested if/else so each elif condition's temp sits inside the
        // preceding else block (C has no clean "else if" with a setup stmt).
        std::function<void(size_t,int)> emit_branch = [&](size_t bi, int bind) {
            std::string bsp(bind * 4, ' ');
            const auto& br = f->branches[bi];
            if (!br.cond) { vjit_emit_block(br.body, c, bind); return; }
            std::string cnd = vjit_emit(br.cond.get(), c, bind);
            c.o << bsp << "if (syn_rt_truthy(" << cnd << ")) {\n";
            vjit_emit_block(br.body, c, bind + 1);
            c.o << bsp << "}";
            if (bi + 1 < f->branches.size()) {
                c.o << " else {\n";
                emit_branch(bi + 1, bind + 1);
                c.o << bsp << "}\n";
            } else {
                c.o << "\n";
            }
        };
        if (!f->branches.empty()) emit_branch(0, ind);
        return;
    }
    if (auto* es = dynamic_cast<const ExprStmt*>(s)) {
        vjit_emit(es->expr.get(), c, ind);  // emitted for side effects; result unused
        return;
    }
}

static void vjit_emit_block(const Block& b, VJitCtx& c, int ind)
{
    for (auto& s : b) vjit_emit_stmt(s.get(), c, ind);
}

// ── Source generation ─────────────────────────────────────────────────────────

static std::string generate_c(const std::vector<const FnDeclStmt*>& fns,
                               const VarSet& ifns, const VarSet& ffns,
                               const Block* main_stmts, bool main_is_int, bool main_is_float,
                               bool main_is_mixed, const VTMap& mix_vt,
                               bool main_is_list, const LVMap& mix_lv,
                               const LFnMap& lfns, const SpecCtx& spec_ctx = g_empty_ctx)
{
    std::ostringstream o;
    o << R"(
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

typedef uint64_t Value;
#define VNAN_BASE 0xFFF8000000000000ULL
#define VPAY_MASK 0x0000FFFFFFFFFFFFULL

static inline Value syn_from_int(int64_t i) {
    return VNAN_BASE | ((uint64_t)(i) & VPAY_MASK);
}
static inline int64_t syn_as_int(Value v) {
    uint64_t p = v & VPAY_MASK;
    return (p & ((uint64_t)1<<47)) ? (int64_t)(p | ~VPAY_MASK) : (int64_t)p;
}
static inline Value syn_from_float(double d) { Value v; memcpy(&v, &d, 8); return v; }
static inline double syn_as_float(Value v)   { double d; memcpy(&d, &v, 8); return d; }
static inline Value syn_none_val() { return VNAN_BASE | ((uint64_t)3<<48); }
#define VAL_TRUE  (VNAN_BASE | ((uint64_t)1<<48))
#define VAL_FALSE (VNAN_BASE | ((uint64_t)2<<48))
// Generic value-JIT runtime helpers (resolved against the syn executable).
extern uint64_t syn_rt_add(uint64_t,uint64_t);  extern uint64_t syn_rt_sub(uint64_t,uint64_t);
extern uint64_t syn_rt_mul(uint64_t,uint64_t);  extern uint64_t syn_rt_div(uint64_t,uint64_t);
extern uint64_t syn_rt_idiv(uint64_t,uint64_t); extern uint64_t syn_rt_mod(uint64_t,uint64_t);
extern uint64_t syn_rt_pow(uint64_t,uint64_t);  extern uint64_t syn_rt_eq(uint64_t,uint64_t);
extern uint64_t syn_rt_neq(uint64_t,uint64_t);  extern uint64_t syn_rt_lt(uint64_t,uint64_t);
extern uint64_t syn_rt_lte(uint64_t,uint64_t);  extern uint64_t syn_rt_gt(uint64_t,uint64_t);
extern uint64_t syn_rt_gte(uint64_t,uint64_t);  extern int      syn_rt_truthy(uint64_t);
extern uint64_t syn_rt_tuple_new(const uint64_t*,int);
extern uint64_t syn_rt_list_new(const uint64_t*,int);
extern uint64_t syn_rt_index(uint64_t,uint64_t);

static inline int64_t syn_idiv(int64_t a, int64_t b) {
    if (!b) return 0;
    int64_t q = a/b, r = a - q*b;
    if (r && (r^b) < 0) q--;
    return q;
}
static inline int64_t syn_imod(int64_t a, int64_t b) {
    if (!b) return 0;
    int64_t r = a%b;
    if (r && (r^b) < 0) r += b;
    return r;
}
// Shortest round-trip decimal for double (matches Python repr behavior)
static void syn_print_double(double d) {
    if (d != d)           { fputs("nan", stdout); return; }
    if (d ==  (1.0/0.0)) { fputs("inf", stdout); return; }
    if (d == -(1.0/0.0)) { fputs("-inf", stdout); return; }
    char buf[32];
    for (int prec = 1; prec <= 17; prec++) {
        int n = snprintf(buf, sizeof(buf), "%.*g", prec, d);
        (void)n;
        double check; sscanf(buf, "%lf", &check);
        if (check == d) break;
    }
    fputs(buf, stdout);
}

static inline int64_t syn_ipow(int64_t base, int64_t exp) {
    if (exp < 0) return 0;
    int64_t r = 1;
    for (; exp > 0; exp >>= 1) { if (exp&1) r *= base; base *= base; }
    return r;
}

)";

    // Forward decls
    for (auto* fn : fns) {
        if (ifns.count(fn->name)) {
            o << "static int64_t _jit_i_" << fn->name << "(";
            for (size_t i = 0; i < fn->params.size(); ++i) {
                if (i) o << ","; o << "int64_t _v_" << fn->params[i].name;
            }
            o << ");\n";
        }
        if (ffns.count(fn->name)) {
            o << "static double _jit_f_" << fn->name << "(";
            for (size_t i = 0; i < fn->params.size(); ++i) {
                if (i) o << ","; o << "double _v_" << fn->params[i].name;
            }
            o << ");\n";
        }
    }
    o << "\n";

    // Named int function bodies
    for (auto* fn : fns) {
        if (!ifns.count(fn->name)) continue;
        o << "static int64_t _jit_i_" << fn->name << "(";
        for (size_t i = 0; i < fn->params.size(); ++i) {
            if (i) o << ","; o << "int64_t _v_" << fn->params[i].name;
        }
        o << ") {\n";
        // Specialized functions use aliases in their body; others use plain context
        bool is_spec = !spec_ctx.aliases.empty() && spec_ctx.fn_params.count(fn->name);
        emit_int_block(fn->body, o, ifns, 1, is_spec ? spec_ctx : g_empty_ctx);
        o << "    return 0LL;\n}\n\n";
    }

    // Named float function bodies
    for (auto* fn : fns) {
        if (!ffns.count(fn->name)) continue;
        o << "static double _jit_f_" << fn->name << "(";
        for (size_t i = 0; i < fn->params.size(); ++i) {
            if (i) o << ","; o << "double _v_" << fn->params[i].name;
        }
        o << ") {\n";
        emit_float_block(fn->body, o, ffns, 1);
        o << "    return 0.0;\n}\n\n";
    }

    // ── Generic value-typed functions ──
    VarSet vfns = find_value_fns(fns, ifns, ffns);
    if (!vfns.empty()) {
        // forward decls
        for (auto* fn : fns) {
            if (!vfns.count(fn->name)) continue;
            o << "static Value _jitv_" << fn->name << "(";
            for (size_t i = 0; i < fn->params.size(); ++i) {
                if (i) o << ","; o << "Value _v_" << fn->params[i].name;
            }
            o << ");\n";
        }
        o << "\n";
        // bodies
        for (auto* fn : fns) {
            if (!vfns.count(fn->name)) continue;
            o << "static Value _jitv_" << fn->name << "(";
            for (size_t i = 0; i < fn->params.size(); ++i) {
                if (i) o << ","; o << "Value _v_" << fn->params[i].name;
            }
            o << ") {\n";
            int tmp = 0;
            VJitCtx ctx{o, tmp, vfns};
            vjit_emit_block(fn->body, ctx, 1);
            o << "    return syn_none_val();\n}\n\n";
        }
        // public wrappers
        for (auto* fn : fns) {
            if (!vfns.count(fn->name)) continue;
            o << "Value syn_jitv_" << fn->name << "(int nargs, Value* args) {\n";
            o << "    return _jitv_" << fn->name << "(";
            for (size_t i = 0; i < fn->params.size(); ++i) {
                if (i) o << ",";
                o << "nargs>" << (int)i << "?args[" << (int)i << "]:syn_none_val()";
            }
            o << ");\n}\n\n";
        }
    }

    // Public wrappers for named functions.
    // Return raw int64_t/double — the VM caller boxes via Value::from_int/from_float
    // so large ints are handled correctly through the GC-aware path.
    for (auto* fn : fns) {
        if (ifns.count(fn->name)) {
            o << "int64_t syn_jit_i_" << fn->name << "(int nargs, Value* args) {\n";
            o << "    return _jit_i_" << fn->name << "(";
            for (size_t i = 0; i < fn->params.size(); ++i) {
                if (i) o << ",";
                o << "syn_as_int(nargs>" << (int)i << "?args[" << (int)i << "]:syn_none_val())";
            }
            o << ");\n}\n\n";
        }
        if (ffns.count(fn->name)) {
            o << "double syn_jit_f_" << fn->name << "(int nargs, Value* args) {\n";
            o << "    return _jit_f_" << fn->name << "(";
            for (size_t i = 0; i < fn->params.size(); ++i) {
                if (i) o << ",";
                o << "syn_as_float(nargs>" << (int)i << "?args[" << (int)i << "]:syn_none_val())";
            }
            o << ");\n}\n\n";
        }
    }

    // Top-level __main__ (bypasses interpreter entirely for pure-numeric scripts)
    if (main_stmts && main_is_int) {
        o << "Value syn_jit_i___main__(int nargs, Value* args) {\n";
        emit_int_block(*main_stmts, o, ifns, 1, spec_ctx);
        o << "    return syn_none_val();\n}\n\n";
    }
    if (main_stmts && main_is_float) {
        o << "Value syn_jit_f___main__(int nargs, Value* args) {\n";
        emit_float_block(*main_stmts, o, ffns, 1);
        o << "    return syn_none_val();\n}\n\n";
    }
    if (main_stmts && main_is_mixed) {
        o << "Value syn_jit_m___main__(int nargs, Value* args) {\n";
        emit_mixed_block(*main_stmts, o, mix_vt, ifns, ffns, 1);
        o << "    return syn_none_val();\n}\n\n";
    }
    if (main_stmts && main_is_list) {
        // Emit global statics for all top-level scalars and literal lists
        for (auto& [nm, is_float] : mix_vt)
            o << "static " << (is_float ? "double" : "int64_t") << " g_v_" << nm << ";\n";
        for (auto& [nm, li] : mix_lv) {
            if (li.is_literal) {
                const char* tn = li.is_float ? "double" : (li.is_bool ? "int8_t" : "int64_t");
                o << "static " << tn << " g_vptr_" << nm << "[" << (int64_t)li.literal_elems.size() << "];\n";
                o << "static int64_t g_vlen_" << nm << " = " << (int64_t)li.literal_elems.size() << "LL;\n";
            }
        }
        o << "\n";

        // Emit list helper functions (with global context so fn bodies can access globals)
        LFnRetMap lfn_rets;
        {
            GCtxGuard gctx(mix_vt, mix_lv, false);
            // Forward-declare list fns first
            for (auto* fn : fns) {
                if (!lfns.count(fn->name)) continue;
                auto& params = lfns.at(fn->name);
                // Determine return type: scan body
                VTMap fn_vt; LVMap fn_lv;
                for (size_t k = 0; k < fn->params.size(); k++) {
                    if (k < params.size() && params[k].is_list) {
                        ListInfo li; li.is_float = params[k].is_float; li.is_fill = true;
                        fn_lv[fn->params[k].name] = std::move(li);
                    } else if (k < params.size()) {
                        fn_vt[fn->params[k].name] = params[k].is_float;
                    }
                }
                auto ret_t = infer_list_fn_ret(fn, fn_vt, fn_lv, ifns, ffns);
                lfn_rets[fn->name] = ret_t;
                const char* ret_str = "void";
                if (ret_t) ret_str = *ret_t ? "double" : "int64_t";
                o << "static " << ret_str << " jit_l_" << fn->name << "(";
                bool fp = true;
                for (size_t k = 0; k < fn->params.size(); k++) {
                    if (!fp) o << ", "; fp = false;
                    if (k < params.size() && params[k].is_list) {
                        const char* tn = params[k].is_float ? "double" : "int64_t";
                        o << tn << "* _vptr_" << fn->params[k].name << ", int64_t _vlen_" << fn->params[k].name;
                    } else {
                        const char* tn = (k < params.size() && params[k].is_float) ? "double" : "int64_t";
                        o << tn << " _v_" << fn->params[k].name;
                    }
                }
                o << ");\n";
            }
            o << "\n";

            // Set up lfn_rets thread-local so fn bodies can call other list fns
            const LFnRetMap* prev_rets = tl_lfn_rets;
            tl_lfn_rets = &lfn_rets;

            for (auto* fn : fns) {
                if (!lfns.count(fn->name)) continue;
                emit_list_fn(fn, lfns.at(fn->name), fns, o, ifns, ffns, lfns, lfn_rets);
            }

            tl_lfn_rets = prev_rets;
        }

        o << "Value syn_jit_l___main__(int nargs, Value* args) {\n";
        {
            GCtxGuard gctx(mix_vt, mix_lv, true);  // tl_is_main = true → use g_v_ prefix
            const LFnRetMap* prev_rets = tl_lfn_rets;
            tl_lfn_rets = &lfn_rets;
            emit_list_block(*main_stmts, o, mix_vt, mix_lv, ifns, ffns, 1, lfns);
            tl_lfn_rets = prev_rets;
        }
        // Free only malloc'd (non-literal, non-global) fill arrays
        for (auto& [nm, li] : mix_lv)
            if (li.is_fill && !li.is_literal) o << "    free(_vptr_" << nm << ");\n";
        o << "    return syn_none_val();\n}\n\n";
    }

    return o.str();
}

// ── Compile + dlopen ──────────────────────────────────────────────────────────

static uint64_t fnv1a(const std::string& s)
{
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; }
    return h;
}

static bool file_exists(const std::string& p)
{
    struct stat st;
    return stat(p.c_str(), &st) == 0;
}


JitModule* jit_compile(const Program& prog)
{
    std::vector<const FnDeclStmt*> fns;
    for (auto& stmt : prog.stmts)
        if (auto* fn = dynamic_cast<const FnDeclStmt*>(stmt.get()))
            fns.push_back(fn);

    VarSet ifns = find_int_fns(fns);
    VarSet ffns = find_float_fns(fns);

    // Check if top-level code is pure int, pure float, mixed int+float, or list-array
    bool main_int   = check_int_block(prog.stmts,   {}, ifns, true);
    bool main_float = !main_int && check_float_block(prog.stmts, {}, ffns, true);
    VTMap mix_vt;
    LVMap mix_lv;
    bool main_mixed = !main_int && !main_float &&
                      check_mixed_block(prog.stmts, mix_vt, ifns, ffns, true);
    LFnMap lfns;
    bool main_list  = !main_int && !main_float && !main_mixed &&
                      check_list_block(prog.stmts, mix_vt, mix_lv, ifns, ffns, true, &fns, &lfns);

    // ── Higher-order function specialization ──────────────────────────────────
    // Detect calls F(g, scalar_args) where g ∈ ifns and F ∉ ifns.
    // If F's body is int-JIT-able when g is aliased for the fn param, specialize F.
    SpecCtx spec_ctx;
    if (!main_int) {
        // Scan __main__ stmts for ExprStmt/print containing a call with an ifns fn arg
        std::function<void(const ExprNode*)> scan_expr = [&](const ExprNode* e) {
            if (!e) return;
            auto* ce = dynamic_cast<const CallExpr*>(e);
            if (!ce) {
                // Recurse into sub-expressions if it's a print wrapper or similar
                if (auto* bexp = dynamic_cast<const BinaryExpr*>(e)) { scan_expr(bexp->left.get()); scan_expr(bexp->right.get()); }
                return;
            }
            auto* callee_id = dynamic_cast<const IdentExpr*>(ce->callee.get());
            if (!callee_id) return;
            // If the callee itself is not in ifns, check if it can be specialized
            if (!ifns.count(callee_id->name)) {
                // Look for fn param: an arg that is an IdentExpr in ifns
                for (size_t k = 0; k < ce->args.size(); k++) {
                    auto* aid = dynamic_cast<const IdentExpr*>(ce->args[k].value.get());
                    if (!aid || !ifns.count(aid->name)) continue;
                    // Found: F(g, ...) where g ∈ ifns. Try to specialize F.
                    for (auto* fn : fns) {
                        if (fn->name != callee_id->name || k >= fn->params.size()) continue;
                        // Build spec context: alias fn->params[k] → g
                        SpecCtx try_ctx;
                        try_ctx.aliases[fn->params[k].name] = aid->name;
                        try_ctx.fn_params[fn->name] = k;
                        // Build ivars from non-fn params (treat fn param as unused int)
                        VarSet fn_ivars;
                        for (auto& p : fn->params) fn_ivars.insert(p.name);
                        if (check_int_block(fn->body, fn_ivars, ifns, false, try_ctx)) {
                            // Specialization works: add F to ifns and record ctx
                            ifns.insert(fn->name);
                            spec_ctx = try_ctx;
                            // Re-check main_int with updated ifns
                            main_int = check_int_block(prog.stmts, {}, ifns, true, spec_ctx);
                        }
                        break;
                    }
                }
            }
            // Also scan into args (for nested calls like print(F(g, n)))
            for (auto& arg : ce->args) scan_expr(arg.value.get());
        };
        for (auto& s : prog.stmts) {
            if (auto* es = dynamic_cast<const ExprStmt*>(s.get()))
                scan_expr(es->expr.get());
        }
    }

    if (ifns.empty() && ffns.empty() && !main_int && !main_float && !main_mixed && !main_list
        && find_value_fns(fns, ifns, ffns).empty())
        return nullptr;

    const Block* main_stmts = (main_int || main_float || main_mixed || main_list) ? &prog.stmts : nullptr;
    std::string src = generate_c(fns, ifns, ffns, main_stmts, main_int, main_float, main_mixed, mix_vt, main_list, mix_lv, lfns, spec_ctx);

    uint64_t h = fnv1a(src);
    char so_path[64], c_path[64];
    snprintf(so_path, sizeof(so_path), "/tmp/syn_jit_%llx.so", (unsigned long long)h);
    snprintf(c_path,  sizeof(c_path),  "/tmp/syn_jit_%llx.c",  (unsigned long long)h);

    if (!file_exists(so_path)) {
        std::ofstream f(c_path);
        if (!f) return nullptr;
        f << src; f.close();
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
                 "gcc -O3 -fPIC -shared -o %s %s 2>/dev/null", so_path, c_path);
        if (std::system(cmd) != 0) return nullptr;
    }

    void* hdl = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);
    if (!hdl) return nullptr;

    auto* mod = new JitModule;
    mod->dl_handle = hdl;

    auto load_sym = [&](const std::string& sym) -> void* {
        return dlsym(hdl, sym.c_str());
    };

    for (auto& name : ifns)
        mod->fns[name].int_fn = reinterpret_cast<JitIntFn>(load_sym("syn_jit_i_" + name));
    for (auto& name : ffns)
        mod->fns[name].float_fn = reinterpret_cast<JitFloatFn>(load_sym("syn_jit_f_" + name));
    for (auto& name : find_value_fns(fns, ifns, ffns))
        mod->fns[name].value_fn = reinterpret_cast<JitValueFn>(load_sym("syn_jitv_" + name));
    if (main_int)
        mod->fns["__main__"].main_fn = reinterpret_cast<JitMainFn>(load_sym("syn_jit_i___main__"));
    if (main_float)
        mod->fns["__main__"].main_fn = reinterpret_cast<JitMainFn>(load_sym("syn_jit_f___main__"));
    if (main_mixed)
        mod->fns["__main__"].main_fn = reinterpret_cast<JitMainFn>(load_sym("syn_jit_m___main__"));
    if (main_list)
        mod->fns["__main__"].main_fn = reinterpret_cast<JitMainFn>(load_sym("syn_jit_l___main__"));

    bool any = false;
    for (auto& [n, e] : mod->fns) if (e.int_fn || e.float_fn || e.main_fn || e.value_fn) { any = true; break; }
    if (!any) { delete mod; return nullptr; }
    return mod;
}

} // namespace syn
