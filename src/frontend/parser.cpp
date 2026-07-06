#include "synapse/frontend/parser.h"
#include <cassert>
#include <stdexcept>

namespace syn {

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / navigation helpers
// ─────────────────────────────────────────────────────────────────────────────

Parser::Parser(std::vector<Token> tokens, const Source& source)
    : m_tokens(std::move(tokens)), m_source(source)
{
}

const Token& Parser::peek(int offset) const
{
    std::size_t i = m_pos + static_cast<std::size_t>(offset >= 0 ? offset : 0);
    if (i >= m_tokens.size()) return m_tokens.back(); // Eof sentinel
    return m_tokens[i];
}

Token Parser::advance()
{
    if (m_pos < m_tokens.size()) return m_tokens[m_pos++];
    return m_tokens.back();
}

bool Parser::check(TokenKind k) const { return peek().kind == k; }

bool Parser::match(TokenKind k)
{
    if (check(k)) { advance(); return true; }
    return false;
}

Token Parser::expect(TokenKind k, std::string_view msg)
{
    if (check(k)) return advance();
    m_diag.error(m_source.location_of(peek().span.start), std::string(msg));
    // Return current token as placeholder (don't advance)
    return peek();
}

void Parser::skip_newlines()
{
    while (check(TokenKind::Newline)) advance();
}

void Parser::consume_term()
{
    skip_newlines(); // already consumed by skip if we're here after a block
    if (check(TokenKind::Semicolon)) advance();
    else if (check(TokenKind::Newline)) advance();
    // Eof is fine
}

bool Parser::at_term() const
{
    return check(TokenKind::Newline) || check(TokenKind::Semicolon) || check(TokenKind::Eof);
}

bool Parser::is_cmd_keyword(TokenKind k) const
{
    switch (k) {
    case TokenKind::Mouse: case TokenKind::Click: case TokenKind::Drag:
    case TokenKind::Scroll: case TokenKind::Hold: case TokenKind::Release:
    case TokenKind::Press: case TokenKind::Type: case TokenKind::Run:
    case TokenKind::Open: case TokenKind::Close: case TokenKind::Focus:
    case TokenKind::Move: case TokenKind::Resize: case TokenKind::Maximize:
    case TokenKind::Minimize: case TokenKind::Capture: case TokenKind::Wait:
    case TokenKind::Find: case TokenKind::See: case TokenKind::Tap:
    case TokenKind::Check: case TokenKind::Uncheck: case TokenKind::Select:
    case TokenKind::Read:
        return true;
    default: return false;
    }
}

bool Parser::is_aug_assign(TokenKind k) const
{
    switch (k) {
    case TokenKind::PlusEq: case TokenKind::MinusEq: case TokenKind::StarEq:
    case TokenKind::SlashEq: case TokenKind::SlashSlashEq: case TokenKind::PercentEq:
    case TokenKind::StarStarEq: case TokenKind::QQEq:
        return true;
    default: return false;
    }
}

bool Parser::is_btn(TokenKind k) const
{
    return k == TokenKind::Left || k == TokenKind::Right || k == TokenKind::Middle;
}

bool Parser::is_scroll_dir(TokenKind k) const
{
    return k == TokenKind::Up || k == TokenKind::Down ||
           k == TokenKind::Left || k == TokenKind::Right;
}

bool Parser::is_elem_type(TokenKind k) const
{
    switch (k) {
    case TokenKind::Button: case TokenKind::Input: case TokenKind::Checkbox:
    case TokenKind::Radio: case TokenKind::Dropdown: case TokenKind::Link:
    case TokenKind::Icon: case TokenKind::Toggle: case TokenKind::Slider:
    case TokenKind::Tab: case TokenKind::MenuItem:
        return true;
    default: return false;
    }
}

bool Parser::is_key_name(TokenKind k) const
{
    return k == TokenKind::Ident  || k == TokenKind::Left  ||
           k == TokenKind::Right  || k == TokenKind::Up    ||
           k == TokenKind::Down   || k == TokenKind::Middle;
}

bool Parser::is_lvalue(const ExprNode* e) const
{
    if (!e) return false;
    if (dynamic_cast<const IdentExpr*>(e)) return true;
    if (auto* f = dynamic_cast<const FieldExpr*>(e)) return !f->is_optional;
    if (dynamic_cast<const IndexExpr*>(e)) return true;
    return false;
}

// ── Desugar helpers ───────────────────────────────────────────────────────────

Arg Parser::pos_arg(Expr e) { Arg a; a.span = e->span; a.value = std::move(e); return a; }
Arg Parser::named_arg(std::string name, Expr e)
{
    Arg a; a.name = std::move(name); a.span = e->span; a.value = std::move(e); return a;
}

Expr Parser::str_lit(Span span, std::string s)
{
    auto n = std::make_unique<StringLitExpr>();
    n->span = span;
    // Compiler::unescape() assumes raw includes the surrounding quotes (true
    // for real string tokens from the lexer) and strips them unconditionally
    // — wrap synthetic literals in quotes so that holds here too. Safe for
    // every current caller (command-desugared button/key/direction names and
    // bareword map-literal keys) since none contain '"' or '\\'.
    n->raw  = "\"" + std::move(s) + "\"";
    return n;
}

Expr Parser::int_lit(Span span, int64_t v)
{
    auto n = std::make_unique<IntLitExpr>();
    n->span  = span;
    n->value = v;
    return n;
}

Stmt Parser::cmd_call(Span span, std::string obj, std::string method,
                       std::vector<Arg> args)
{
    auto ident = std::make_unique<IdentExpr>();
    ident->span = span; ident->name = std::move(obj);

    auto field = std::make_unique<FieldExpr>();
    field->span = span; field->object = std::move(ident);
    field->field = std::move(method); field->is_optional = false;

    auto call = std::make_unique<CallExpr>();
    call->span = span; call->callee = std::move(field);
    call->args  = std::move(args);

    auto stmt = std::make_unique<ExprStmt>();
    stmt->span = span; stmt->expr = std::move(call);
    return stmt;
}

// ── Error recovery ────────────────────────────────────────────────────────────

Stmt Parser::error_stmt(Span span, std::string msg)
{
    m_diag.error(m_source.location_of(span.start), std::move(msg));
    sync_to_next_stmt();
    auto n = std::make_unique<ExprStmt>();
    n->span = span;
    n->expr = std::make_unique<NoneLitExpr>();
    n->expr->span = span;
    return n;
}

Expr Parser::error_expr(Span span, std::string msg)
{
    m_diag.error(m_source.location_of(span.start), std::move(msg));
    // Guarantee forward progress: this is parse_primary()'s fallback for a
    // token that starts no valid expression (e.g. a reserved keyword like
    // `middle` used where an identifier is expected). Without consuming it,
    // the caller's statement/expression loop re-parses the exact same token
    // forever, appending an identical diagnostic each time until the
    // diagnostics vector exhausts memory (std::bad_alloc, not a clean error).
    if (!check(TokenKind::Eof)) advance();
    auto n = std::make_unique<NoneLitExpr>();
    n->span = span;
    return n;
}

void Parser::sync_to_next_stmt()
{
    while (!check(TokenKind::Newline) && !check(TokenKind::Semicolon) &&
           !check(TokenKind::Eof)) {
        advance();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Program entry point
// ─────────────────────────────────────────────────────────────────────────────

Program Parser::parse()
{
    Program prog;
    skip_newlines();
    prog.stmts = parse_stmt_list();
    prog.has_errors = m_diag.has_errors();
    return prog;
}

// ─────────────────────────────────────────────────────────────────────────────
// Statement list and block
// ─────────────────────────────────────────────────────────────────────────────

Block Parser::parse_stmt_list()
{
    Block stmts;
    while (!check(TokenKind::Eof) && !check(TokenKind::RBrace)) {
        skip_newlines();
        if (check(TokenKind::Eof) || check(TokenKind::RBrace)) break;
        stmts.push_back(parse_stmt());
        // After a statement, consume any trailing term
        while (check(TokenKind::Newline) || check(TokenKind::Semicolon)) advance();
    }
    return stmts;
}

Block Parser::parse_block()
{
    expect(TokenKind::LBrace, "expected '{'");
    skip_newlines();
    Block stmts = parse_stmt_list();
    expect(TokenKind::RBrace, "expected '}'");
    return stmts;
}

// ─────────────────────────────────────────────────────────────────────────────
// Statement dispatch
// ─────────────────────────────────────────────────────────────────────────────

Stmt Parser::parse_stmt()
{
    TokenKind k = peek().kind;

    if (k == TokenKind::Let)      return parse_let_stmt();
    if (k == TokenKind::Const)    return parse_const_stmt();
    if (k == TokenKind::If)       return parse_if_stmt();
    if (k == TokenKind::While)    return parse_while_stmt();
    if (k == TokenKind::Repeat)   return parse_repeat_stmt();
    if (k == TokenKind::For)      return parse_for_stmt();
    if (k == TokenKind::Match)    return parse_match_stmt();
    if (k == TokenKind::Try)      return parse_try_stmt();
    if (k == TokenKind::Fn)       return parse_fn_decl();
    if (k == TokenKind::Return)   return parse_return_stmt();
    if (k == TokenKind::Throw)    return parse_throw_stmt();
    if (k == TokenKind::Break) {
        Span sp = advance().span;
        auto s = std::make_unique<BreakStmt>(); s->span = sp; return s;
    }
    if (k == TokenKind::Continue) {
        Span sp = advance().span;
        auto s = std::make_unique<ContinueStmt>(); s->span = sp; return s;
    }
    if (k == TokenKind::Use)      return parse_use_stmt();
    if (k == TokenKind::In)       return parse_in_scope_stmt();
    if (k == TokenKind::Say)      return parse_say_stmt();
    // A command keyword followed by `.` is method access on the stdlib object
    // (mouse.mode(...), keyboard.type(...)), not a command statement — let the
    // expression parser handle it (it treats command keywords as identifiers).
    if (is_cmd_keyword(k) && peek(1).kind != TokenKind::Dot)
        return parse_cmd_stmt();

    return parse_assign_or_expr_stmt();
}

// ─────────────────────────────────────────────────────────────────────────────
// let / const
// ─────────────────────────────────────────────────────────────────────────────

Stmt Parser::parse_let_stmt()
{
    Span sp = advance().span; // consume 'let'
    auto node = std::make_unique<LetStmt>();
    node->span = sp;

    // Destructuring forms
    if (match(TokenKind::LParen)) {
        // let (x, y) = expr  or  let (first, *rest) = items
        node->bind_kind = LetBindKind::TupleDestruct;
        while (!check(TokenKind::RParen) && !check(TokenKind::Eof)) {
            if (match(TokenKind::Star)) {
                node->rest_name = std::string(expect(TokenKind::Ident, "expected name after '*'").text(m_source));
            } else {
                node->names.push_back(std::string(expect(TokenKind::Ident, "expected name").text(m_source)));
            }
            if (!match(TokenKind::Comma)) break;
        }
        expect(TokenKind::RParen, "expected ')'");
        expect(TokenKind::Eq, "expected '='");
        node->values.push_back(parse_expr());
        return node;
    }

    if (match(TokenKind::LBracket)) {
        // let [x, y] = expr
        node->bind_kind = LetBindKind::ListDestruct;
        while (!check(TokenKind::RBracket) && !check(TokenKind::Eof)) {
            if (match(TokenKind::Star)) {
                node->rest_name = std::string(expect(TokenKind::Ident, "expected name after '*'").text(m_source));
            } else {
                node->names.push_back(std::string(expect(TokenKind::Ident, "expected name").text(m_source)));
            }
            if (!match(TokenKind::Comma)) break;
        }
        expect(TokenKind::RBracket, "expected ']'");
        expect(TokenKind::Eq, "expected '='");
        node->values.push_back(parse_expr());
        return node;
    }

    if (match(TokenKind::LBrace)) {
        // let {name, age} = user  or  let {name: alias} = user
        node->bind_kind = LetBindKind::MapDestruct;
        while (!check(TokenKind::RBrace) && !check(TokenKind::Eof)) {
            MapDestructEntry entry;
            entry.span = peek().span;
            entry.key   = std::string(expect(TokenKind::Ident, "expected key name").text(m_source));
            entry.alias = entry.key;
            if (match(TokenKind::Colon)) {
                entry.alias = std::string(expect(TokenKind::Ident, "expected alias name").text(m_source));
            }
            node->map_entries.push_back(std::move(entry));
            if (!match(TokenKind::Comma)) break;
        }
        expect(TokenKind::RBrace, "expected '}'");
        expect(TokenKind::Eq, "expected '='");
        node->values.push_back(parse_expr());
        return node;
    }

    // Simple: let x = expr  or  let a, b = e1, e2
    node->bind_kind = LetBindKind::Simple;
    node->names.push_back(std::string(expect(TokenKind::Ident, "expected variable name").text(m_source)));
    while (match(TokenKind::Comma)) {
        node->names.push_back(std::string(expect(TokenKind::Ident, "expected variable name").text(m_source)));
    }
    expect(TokenKind::Eq, "expected '='");
    skip_newlines();
    node->values.push_back(parse_expr());
    while (match(TokenKind::Comma)) {
        skip_newlines();
        node->values.push_back(parse_expr());
    }
    return node;
}

Stmt Parser::parse_const_stmt()
{
    Span sp = advance().span;
    auto node  = std::make_unique<ConstStmt>();
    node->span = sp;
    node->name  = std::string(expect(TokenKind::Ident, "expected constant name").text(m_source));
    expect(TokenKind::Eq, "expected '='");
    skip_newlines();
    node->value = parse_expr();
    return node;
}

// ─────────────────────────────────────────────────────────────────────────────
// if
// ─────────────────────────────────────────────────────────────────────────────

Stmt Parser::parse_if_stmt()
{
    Span sp = advance().span; // consume 'if'
    auto node = std::make_unique<IfStmt>();
    node->span = sp;

    IfBranch branch;
    branch.span = sp;
    branch.cond = parse_expr();
    branch.body = parse_block();
    node->branches.push_back(std::move(branch));

    while (check(TokenKind::Else)) {
        Span esp = advance().span; // consume 'else'
        skip_newlines();
        if (match(TokenKind::If)) {
            IfBranch elif;
            elif.span = esp;
            elif.cond = parse_expr();
            elif.body = parse_block();
            node->branches.push_back(std::move(elif));
        } else {
            // final else
            IfBranch else_br;
            else_br.span = esp;
            else_br.body = parse_block();
            node->branches.push_back(std::move(else_br));
            break;
        }
    }
    return node;
}

// ─────────────────────────────────────────────────────────────────────────────
// while / repeat
// ─────────────────────────────────────────────────────────────────────────────

Stmt Parser::parse_while_stmt()
{
    Span sp = advance().span;
    auto node  = std::make_unique<WhileStmt>();
    node->span = sp;
    node->cond = parse_expr();
    node->body = parse_block();
    return node;
}

Stmt Parser::parse_repeat_stmt()
{
    Span sp = advance().span;
    auto node  = std::make_unique<RepeatStmt>();
    node->span = sp;
    node->count = parse_expr();
    if (match(TokenKind::As)) {
        node->index_name = std::string(expect(TokenKind::Ident, "expected index name").text(m_source));
    }
    node->body = parse_block();
    return node;
}

// ─────────────────────────────────────────────────────────────────────────────
// for
// ─────────────────────────────────────────────────────────────────────────────

Stmt Parser::parse_for_stmt()
{
    Span sp = advance().span;
    auto node  = std::make_unique<ForStmt>();
    node->span = sp;

    node->iter1 = std::string(expect(TokenKind::Ident, "expected variable name").text(m_source));
    if (match(TokenKind::Comma)) {
        node->iter2 = std::string(expect(TokenKind::Ident, "expected variable name").text(m_source));
    }

    if (match(TokenKind::Eq)) {
        // for i = start to end [by step]
        skip_newlines();
        node->source = parse_expr();
        expect(TokenKind::To, "expected 'to' after start value");
        skip_newlines();
        node->range_end = parse_expr();
        if (match(TokenKind::By)) {
            skip_newlines();
            node->range_step = parse_expr();
        }
    } else {
        // for i in iterable  OR  for i in start to end [by step]
        expect(TokenKind::In, "expected 'in' or '='");
        skip_newlines();
        node->source = parse_expr();
        if (match(TokenKind::To)) {
            skip_newlines();
            node->range_end = parse_expr();
            if (match(TokenKind::By)) {
                skip_newlines();
                node->range_step = parse_expr();
            }
        }
    }

    node->body = parse_block();
    return node;
}

// ─────────────────────────────────────────────────────────────────────────────
// match
// ─────────────────────────────────────────────────────────────────────────────

std::vector<MatchArm> Parser::parse_match_arms()
{
    std::vector<MatchArm> arms;
    while (check(TokenKind::Case)) {
        MatchArm arm;
        arm.span = advance().span; // consume 'case'
        arm.pattern = parse_pattern();
        if (match(TokenKind::If)) {
            arm.guard = parse_expr();
        }
        arm.body = parse_block();
        skip_newlines();
        arms.push_back(std::move(arm));
    }
    return arms;
}

Stmt Parser::parse_match_stmt()
{
    Span sp = peek().span;
    // Reuse MatchExpr for both statement and expression contexts
    auto expr = parse_expr(); // parse_primary handles 'match'
    auto stmt = std::make_unique<ExprStmt>();
    stmt->span = sp;
    stmt->expr = std::move(expr);
    return stmt;
}

// ─────────────────────────────────────────────────────────────────────────────
// try
// ─────────────────────────────────────────────────────────────────────────────

Stmt Parser::parse_try_stmt()
{
    Span sp = advance().span;
    auto node  = std::make_unique<TryStmt>();
    node->span = sp;
    node->body = parse_block();

    // catch clauses
    while (check(TokenKind::Catch)) {
        Span csp = advance().span;
        CatchClause cc;
        cc.span = csp;
        cc.name = std::string(expect(TokenKind::Ident, "expected error variable name").text(m_source));
        if (match(TokenKind::If)) {
            cc.guard = parse_expr();
        }
        skip_newlines();
        cc.body = parse_block();
        node->catches.push_back(std::move(cc));
    }

    // optional else
    if (check(TokenKind::Else)) {
        advance();
        skip_newlines();
        node->else_body = parse_block();
    }

    // optional finally
    if (check(TokenKind::Finally)) {
        advance();
        skip_newlines();
        node->finally_body = parse_block();
    }

    if (node->catches.empty() && node->else_body.empty() && node->finally_body.empty()) {
        m_diag.error(m_source.location_of(sp.start),
                     "try statement must have at least one catch, else, or finally clause");
    }
    return node;
}

// ─────────────────────────────────────────────────────────────────────────────
// fn declaration
// ─────────────────────────────────────────────────────────────────────────────

Stmt Parser::parse_fn_decl()
{
    Span sp = advance().span; // consume 'fn'
    auto node  = std::make_unique<FnDeclStmt>();
    node->span = sp;
    node->name  = std::string(expect(TokenKind::Ident, "expected function name").text(m_source));
    expect(TokenKind::LParen, "expected '('");
    skip_newlines();
    if (!check(TokenKind::RParen)) {
        node->params = parse_param_list();
    }
    expect(TokenKind::RParen, "expected ')'");
    skip_newlines();
    node->body = parse_block();
    return node;
}

std::vector<Param> Parser::parse_param_list()
{
    std::vector<Param> params;
    do {
        skip_newlines();
        Param p;
        p.span = peek().span;
        if (match(TokenKind::Star)) {
            p.is_variadic = true;
            p.name = std::string(expect(TokenKind::Ident, "expected parameter name").text(m_source));
            params.push_back(std::move(p));
            break; // variadic must be last
        }
        p.name = std::string(expect(TokenKind::Ident, "expected parameter name").text(m_source));
        if (match(TokenKind::Eq)) {
            p.default_val = parse_expr();
        }
        params.push_back(std::move(p));
        skip_newlines();
    } while (match(TokenKind::Comma));
    return params;
}

// ─────────────────────────────────────────────────────────────────────────────
// return / throw
// ─────────────────────────────────────────────────────────────────────────────

Stmt Parser::parse_return_stmt()
{
    Span sp = advance().span;
    auto node  = std::make_unique<ReturnStmt>();
    node->span = sp;
    if (!at_term()) {
        node->values.push_back(parse_expr());
        while (match(TokenKind::Comma)) {
            skip_newlines();
            node->values.push_back(parse_expr());
        }
    }
    return node;
}

Stmt Parser::parse_throw_stmt()
{
    Span sp = advance().span;
    auto node  = std::make_unique<ThrowStmt>();
    node->span = sp;
    node->value = parse_expr();
    return node;
}

// ─────────────────────────────────────────────────────────────────────────────
// use
// ─────────────────────────────────────────────────────────────────────────────

Stmt Parser::parse_use_stmt()
{
    Span sp = advance().span;
    auto node  = std::make_unique<UseStmt>();
    node->span = sp;

    auto parse_one = [&]() {
        UseEntry entry;
        entry.span = peek().span;
        if (check(TokenKind::String)) {
            entry.is_str_ref = true;
            entry.module_str = std::string(advance().text(m_source));
        } else {
            entry.module_path.push_back(
                std::string(expect(TokenKind::Ident, "expected module name").text(m_source)));
            while (check(TokenKind::Dot) && peek(1).kind == TokenKind::Ident) {
                advance(); // '.'
                entry.module_path.push_back(std::string(advance().text(m_source)));
            }
        }
        if (match(TokenKind::As)) {
            entry.alias = std::string(expect(TokenKind::Ident, "expected alias").text(m_source));
        } else if (match(TokenKind::LBrace)) {
            do {
                skip_newlines();
                entry.selects.push_back(
                    std::string(expect(TokenKind::Ident, "expected name").text(m_source)));
                skip_newlines();
            } while (match(TokenKind::Comma));
            expect(TokenKind::RBrace, "expected '}'");
        }
        node->entries.push_back(std::move(entry));
    };

    parse_one();
    while (match(TokenKind::Comma)) {
        parse_one();
    }
    return node;
}

// ─────────────────────────────────────────────────────────────────────────────
// in scope
// ─────────────────────────────────────────────────────────────────────────────

Stmt Parser::parse_in_scope_stmt()
{
    Span sp = advance().span; // consume 'in'
    auto node  = std::make_unique<InScopeStmt>();
    node->span = sp;
    // next must be a string literal for window name
    if (!check(TokenKind::String)) {
        return error_stmt(sp, "expected window name string after 'in'");
    }
    auto raw = std::string(advance().text(m_source));
    // strip surrounding quotes
    node->window_name = (raw.size() >= 2) ? raw.substr(1, raw.size() - 2) : raw;
    node->body = parse_block();
    return node;
}

// ─────────────────────────────────────────────────────────────────────────────
// Assignment / expr statement disambiguation
// ─────────────────────────────────────────────────────────────────────────────

Stmt Parser::parse_assign_or_expr_stmt()
{
    Span sp = peek().span;
    Expr first = parse_expr();

    // Augmented assignment
    if (is_aug_assign(peek().kind)) {
        if (!is_lvalue(first.get())) {
            return error_stmt(sp, "left side of augmented assignment is not an lvalue");
        }
        TokenKind op = advance().kind;
        skip_newlines();
        Expr val = parse_expr();
        auto node = std::make_unique<AugAssignStmt>();
        node->span   = sp;
        node->lvalue = std::move(first);
        node->op     = op;
        node->value  = std::move(val);
        return node;
    }

    // Multi-assign or single assign
    if (check(TokenKind::Comma) || check(TokenKind::Eq)) {
        std::vector<Expr> lvalues;
        lvalues.push_back(std::move(first));

        while (check(TokenKind::Comma)) {
            // Peek ahead: is this a comma in a multi-assign lhs, or something else?
            // If the pattern is: expr ',' expr ... '=' — treat as multi-assign.
            advance(); // consume ','
            skip_newlines();
            lvalues.push_back(parse_expr());
        }

        if (match(TokenKind::Eq)) {
            // Validate all lvalues
            for (auto& lv : lvalues) {
                if (!is_lvalue(lv.get())) {
                    m_diag.error(m_source.location_of(lv->span.start),
                                 "expression is not a valid assignment target");
                }
            }
            skip_newlines();
            std::vector<Expr> values;
            values.push_back(parse_expr());
            while (match(TokenKind::Comma)) {
                skip_newlines();
                values.push_back(parse_expr());
            }
            auto node = std::make_unique<AssignStmt>();
            node->span    = sp;
            node->lvalues = std::move(lvalues);
            node->values  = std::move(values);
            return node;
        }

        // No '=' after collecting: error (bare comma-list is not a valid statement)
        // Return first as expression statement and warn
        m_diag.error(m_source.location_of(sp.start), "unexpected ',' in expression statement");
        auto stmt = std::make_unique<ExprStmt>();
        stmt->span = sp;
        stmt->expr = std::move(lvalues[0]);
        return stmt;
    }

    // Plain expression statement
    auto stmt = std::make_unique<ExprStmt>();
    stmt->span = sp;
    stmt->expr = std::move(first);
    return stmt;
}

// ─────────────────────────────────────────────────────────────────────────────
// Command statements (desugared to method calls)
// ─────────────────────────────────────────────────────────────────────────────

// say EXPR  →  say(EXPR)   (bare `say` prints a blank line)
Stmt Parser::parse_say_stmt()
{
    Span sp = advance().span;

    auto callee = std::make_unique<IdentExpr>();
    callee->span = sp; callee->name = "say";

    auto call = std::make_unique<CallExpr>();
    call->span = sp; call->callee = std::move(callee);
    if (!at_term())
        call->args.push_back(pos_arg(parse_expr()));

    auto stmt = std::make_unique<ExprStmt>();
    stmt->span = sp; stmt->expr = std::move(call);
    return stmt;
}

Stmt Parser::parse_cmd_stmt()
{
    switch (peek().kind) {
    case TokenKind::Mouse:    return parse_mouse_cmd();
    case TokenKind::Click:    return parse_click_cmd();
    case TokenKind::Drag:     return parse_drag_cmd();
    case TokenKind::Scroll:   return parse_scroll_cmd();
    case TokenKind::Hold:     return parse_hold_release_cmd(TokenKind::Hold);
    case TokenKind::Release:  return parse_hold_release_cmd(TokenKind::Release);
    case TokenKind::Press:    return parse_press_cmd();
    case TokenKind::Type:     return parse_type_cmd();
    case TokenKind::Run:      return parse_run_open_close_focus_cmd(TokenKind::Run);
    case TokenKind::Open:     return parse_run_open_close_focus_cmd(TokenKind::Open);
    case TokenKind::Close:    return parse_run_open_close_focus_cmd(TokenKind::Close);
    case TokenKind::Focus:    return parse_run_open_close_focus_cmd(TokenKind::Focus);
    case TokenKind::Move:     return parse_move_resize_cmd(TokenKind::Move);
    case TokenKind::Resize:   return parse_move_resize_cmd(TokenKind::Resize);
    case TokenKind::Maximize: return parse_maximize_minimize_cmd(TokenKind::Maximize);
    case TokenKind::Minimize: return parse_maximize_minimize_cmd(TokenKind::Minimize);
    case TokenKind::Capture:  return parse_capture_cmd();
    case TokenKind::Wait:     return parse_wait_cmd();
    case TokenKind::Tap:      return parse_ui_elem_cmd(TokenKind::Tap);
    case TokenKind::Find:     return parse_ui_elem_cmd(TokenKind::Find);
    case TokenKind::See:      return parse_ui_elem_cmd(TokenKind::See);
    case TokenKind::Check:    return parse_check_cmd();
    case TokenKind::Uncheck:  return parse_uncheck_cmd();
    case TokenKind::Select:   return parse_select_cmd();
    case TokenKind::Read:     return parse_read_cmd();
    default:
        return error_stmt(peek().span, "unknown command keyword");
    }
}

// mouse 300, 400 [, speed]  →  mouse.move(300, 400 [, speed: speed])
Stmt Parser::parse_mouse_cmd()
{
    Span sp = advance().span;
    std::vector<Arg> args;
    args.push_back(pos_arg(parse_expr()));
    expect(TokenKind::Comma, "expected ',' after x coordinate");
    args.push_back(pos_arg(parse_expr()));
    if (match(TokenKind::Comma)) {
        args.push_back(named_arg("speed", parse_expr()));
    }
    return cmd_call(sp, "mouse", "move", std::move(args));
}

// click [btn] [x, y] [, count]  →  mouse.click(btn, [x, y,] [count:n])
Stmt Parser::parse_click_cmd()
{
    Span sp = advance().span;
    std::vector<Arg> args;

    std::string btn_str = "left";
    if (is_btn(peek().kind)) {
        btn_str = std::string(advance().text(m_source));
    }
    args.push_back(pos_arg(str_lit(sp, btn_str)));

    // optional coord: starts with an expression (not a term)
    if (!at_term() && !check(TokenKind::Comma)) {
        args.push_back(pos_arg(parse_expr()));
        expect(TokenKind::Comma, "expected ',' between coordinates");
        args.push_back(pos_arg(parse_expr()));
        if (match(TokenKind::Comma)) {
            args.push_back(named_arg("count", parse_expr()));
        }
    } else if (match(TokenKind::Comma)) {
        // no btn was given but we have coords or count
        args.push_back(pos_arg(parse_expr()));
        if (match(TokenKind::Comma)) {
            args.push_back(pos_arg(parse_expr()));
            if (match(TokenKind::Comma)) {
                args.push_back(named_arg("count", parse_expr()));
            }
        } else {
            // single number after comma = count
            // Replace last pos_arg with count named arg
            Arg cnt = named_arg("count", std::move(args.back().value));
            args.pop_back();
            args.push_back(std::move(cnt));
        }
    }

    return cmd_call(sp, "mouse", "click", std::move(args));
}

// drag x1, y1, x2, y2 [, speed]  →  mouse.drag(x1, y1, x2, y2 [, speed:s])
Stmt Parser::parse_drag_cmd()
{
    Span sp = advance().span;
    std::vector<Arg> args;
    args.push_back(pos_arg(parse_expr()));
    expect(TokenKind::Comma, "expected ','"); args.push_back(pos_arg(parse_expr()));
    expect(TokenKind::Comma, "expected ','"); args.push_back(pos_arg(parse_expr()));
    expect(TokenKind::Comma, "expected ','"); args.push_back(pos_arg(parse_expr()));
    if (match(TokenKind::Comma)) {
        args.push_back(named_arg("speed", parse_expr()));
    }
    return cmd_call(sp, "mouse", "drag", std::move(args));
}

// scroll dir n  →  mouse.scroll(dir, n)
Stmt Parser::parse_scroll_cmd()
{
    Span sp = advance().span;
    if (!is_scroll_dir(peek().kind)) {
        return error_stmt(sp, "expected scroll direction (up/down/left/right)");
    }
    std::string dir = std::string(advance().text(m_source));
    std::vector<Arg> args;
    args.push_back(pos_arg(str_lit(sp, dir)));
    args.push_back(pos_arg(parse_expr()));
    return cmd_call(sp, "mouse", "scroll", std::move(args));
}

// hold/release left|right|middle  →  mouse.hold/release(btn)
// hold/release <key>              →  keyboard.hold/release(key)
Stmt Parser::parse_hold_release_cmd(TokenKind which)
{
    Span sp = advance().span;
    std::string method = (which == TokenKind::Hold) ? "hold" : "release";
    std::vector<Arg> args;

    if (is_btn(peek().kind)) {
        std::string btn = std::string(advance().text(m_source));
        args.push_back(pos_arg(str_lit(sp, btn)));
        return cmd_call(sp, "mouse", method, std::move(args));
    }
    std::string chord = parse_key_chord();
    args.push_back(pos_arg(str_lit(sp, chord)));
    return cmd_call(sp, "keyboard", method, std::move(args));
}

// press key_chord  →  keyboard.press(chord)
Stmt Parser::parse_press_cmd()
{
    Span sp = advance().span;
    std::string chord = parse_key_chord();
    std::vector<Arg> args;
    args.push_back(pos_arg(str_lit(sp, chord)));
    return cmd_call(sp, "keyboard", "press", std::move(args));
}

// type expr  →  keyboard.type(expr)
Stmt Parser::parse_type_cmd()
{
    Span sp = advance().span;
    std::vector<Arg> args;
    args.push_back(pos_arg(parse_expr()));
    return cmd_call(sp, "keyboard", "type", std::move(args));
}

// run/open/close/focus expr  →  app.run/open/close / window.focus
Stmt Parser::parse_run_open_close_focus_cmd(TokenKind which)
{
    Span sp = advance().span;
    std::string obj, method;
    switch (which) {
    case TokenKind::Run:   obj = "app";    method = "run";   break;
    case TokenKind::Open:  obj = "app";    method = "open";  break;
    case TokenKind::Close: obj = "app";    method = "close"; break;
    case TokenKind::Focus: obj = "window"; method = "focus"; break;
    default: break;
    }
    std::vector<Arg> args;
    args.push_back(pos_arg(parse_expr()));
    return cmd_call(sp, obj, method, std::move(args));
}

// move/resize "Window" x, y  →  window.move/resize(name, x, y)
Stmt Parser::parse_move_resize_cmd(TokenKind which)
{
    Span sp = advance().span;
    std::string method = (which == TokenKind::Move) ? "move" : "resize";
    if (!check(TokenKind::String)) {
        return error_stmt(sp, "expected window name string");
    }
    std::vector<Arg> args;
    args.push_back(pos_arg(parse_expr())); // window name
    expect(TokenKind::Comma, "expected ','");
    args.push_back(pos_arg(parse_expr())); // x
    expect(TokenKind::Comma, "expected ','");
    args.push_back(pos_arg(parse_expr())); // y
    return cmd_call(sp, "window", method, std::move(args));
}

// maximize/minimize "Window"  →  window.maximize/minimize(name)
Stmt Parser::parse_maximize_minimize_cmd(TokenKind which)
{
    Span sp = advance().span;
    std::string method = (which == TokenKind::Maximize) ? "maximize" : "minimize";
    std::vector<Arg> args;
    args.push_back(pos_arg(parse_expr()));
    return cmd_call(sp, "window", method, std::move(args));
}

// capture ["file"]  or  capture x1,y1,x2,y2 "file"
// →  screen.capture(file [, x1,y1,x2,y2])
Stmt Parser::parse_capture_cmd()
{
    Span sp = advance().span;
    std::vector<Arg> args;

    if (check(TokenKind::String)) {
        // capture "file"
        args.push_back(pos_arg(parse_expr()));
    } else {
        // capture x1, y1, x2, y2 "file"
        Expr x1 = parse_expr(); expect(TokenKind::Comma, "expected ','");
        Expr y1 = parse_expr(); expect(TokenKind::Comma, "expected ','");
        Expr x2 = parse_expr(); expect(TokenKind::Comma, "expected ','");
        Expr y2 = parse_expr();
        // file comes after coords
        Expr file = parse_expr();
        args.push_back(pos_arg(std::move(file)));
        args.push_back(pos_arg(std::move(x1)));
        args.push_back(pos_arg(std::move(y1)));
        args.push_back(pos_arg(std::move(x2)));
        args.push_back(pos_arg(std::move(y2)));
    }
    return cmd_call(sp, "screen", "capture", std::move(args));
}

// wait duration  →  time.wait(duration)
Stmt Parser::parse_wait_cmd()
{
    Span sp = advance().span;
    std::vector<Arg> args;
    args.push_back(pos_arg(parse_expr()));
    return cmd_call(sp, "time", "wait", std::move(args));
}

// tap/find/see [elem_type] expr [min_confidence n]  →  ui.tap/find/see(expr [, type:t] [, min_confidence:n])
Stmt Parser::parse_ui_elem_cmd(TokenKind which)
{
    Span sp = advance().span;
    std::string method;
    switch (which) {
    case TokenKind::Tap:  method = "tap";  break;
    case TokenKind::Find: method = "find"; break;
    case TokenKind::See:  method = "see";  break;
    default: break;
    }
    std::vector<Arg> args;

    std::string elem_type_str;
    if (is_elem_type(peek().kind)) {
        elem_type_str = std::string(advance().text(m_source));
    }
    args.push_back(pos_arg(parse_expr()));
    if (!elem_type_str.empty()) {
        args.push_back(named_arg("type", str_lit(sp, elem_type_str)));
    }
    if (match(TokenKind::MinConfidence)) {
        args.push_back(named_arg("min_confidence", parse_expr()));
    }
    return cmd_call(sp, "ui", method, std::move(args));
}

// check expr  →  ui.check(expr)
Stmt Parser::parse_check_cmd()
{
    Span sp = advance().span;
    std::vector<Arg> args;
    args.push_back(pos_arg(parse_expr()));
    return cmd_call(sp, "ui", "check", std::move(args));
}

// uncheck expr  →  ui.uncheck(expr)
Stmt Parser::parse_uncheck_cmd()
{
    Span sp = advance().span;
    std::vector<Arg> args;
    args.push_back(pos_arg(parse_expr()));
    return cmd_call(sp, "ui", "uncheck", std::move(args));
}

// select expr in expr  →  ui.select(container, value)
Stmt Parser::parse_select_cmd()
{
    Span sp = advance().span;
    Expr val = parse_expr();
    expect(TokenKind::In, "expected 'in'");
    Expr container = parse_expr();
    std::vector<Arg> args;
    args.push_back(pos_arg(std::move(container)));
    args.push_back(pos_arg(std::move(val)));
    return cmd_call(sp, "ui", "select", std::move(args));
}

// read expr  →  ui.read(expr)
Stmt Parser::parse_read_cmd()
{
    Span sp = advance().span;
    std::vector<Arg> args;
    args.push_back(pos_arg(parse_expr()));
    return cmd_call(sp, "ui", "read", std::move(args));
}

// key_chord ::= key_name { '+' key_name }
std::string Parser::parse_key_chord()
{
    if (!is_key_name(peek().kind)) {
        m_diag.error(m_source.location_of(peek().span.start), "expected key name");
        return "";
    }
    std::string chord = std::string(advance().text(m_source));
    while (check(TokenKind::Plus)) {
        advance(); // consume '+'
        if (!is_key_name(peek().kind)) break;
        chord += "+";
        chord += std::string(advance().text(m_source));
    }
    return chord;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pratt expression parser  (low → high precedence)
// ─────────────────────────────────────────────────────────────────────────────

Expr Parser::parse_expr()   { return parse_ternary(); }

// Level 1: X if C else Y (suffix ternary)
Expr Parser::parse_ternary()
{
    Expr value = parse_pipe();
    if (check(TokenKind::If)) {
        Span sp = peek().span;
        advance(); // consume 'if'
        Expr cond = parse_pipe();
        expect(TokenKind::Else, "expected 'else' in ternary expression");
        skip_newlines();
        Expr else_val = parse_pipe();
        auto node = std::make_unique<TernaryExpr>();
        node->span     = sp;
        node->value    = std::move(value);
        node->cond     = std::move(cond);
        node->else_val = std::move(else_val);
        return node;
    }
    return value;
}

// Level 2: |>
Expr Parser::parse_pipe()
{
    Expr left = parse_null_coalesce();
    while (check(TokenKind::Pipe)) {
        Span sp = advance().span; // consume '|>'
        skip_newlines();
        Expr right = parse_null_coalesce();
        auto node = std::make_unique<BinaryExpr>();
        node->span  = sp;
        node->op    = TokenKind::Pipe;
        node->left  = std::move(left);
        node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

// Level 3: ??
Expr Parser::parse_null_coalesce()
{
    Expr left = parse_or_expr();
    while (check(TokenKind::QQ)) {
        Span sp = advance().span;
        skip_newlines();
        Expr right = parse_or_expr();
        auto node = std::make_unique<BinaryExpr>();
        node->span  = sp; node->op = TokenKind::QQ;
        node->left  = std::move(left); node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

// Level 4: or
Expr Parser::parse_or_expr()
{
    Expr left = parse_and_expr();
    while (check(TokenKind::Or)) {
        Span sp = advance().span;
        skip_newlines();
        Expr right = parse_and_expr();
        auto node = std::make_unique<BinaryExpr>();
        node->span  = sp; node->op = TokenKind::Or;
        node->left  = std::move(left); node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

// Level 5: and
Expr Parser::parse_and_expr()
{
    Expr left = parse_not_expr();
    while (check(TokenKind::And)) {
        Span sp = advance().span;
        skip_newlines();
        Expr right = parse_not_expr();
        auto node = std::make_unique<BinaryExpr>();
        node->span  = sp; node->op = TokenKind::And;
        node->left  = std::move(left); node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

// Level 6: not (prefix)
Expr Parser::parse_not_expr()
{
    if (check(TokenKind::Not)) {
        Span sp = advance().span;
        Expr operand = parse_not_expr();
        auto node = std::make_unique<UnaryExpr>();
        node->span = sp; node->op = TokenKind::Not; node->operand = std::move(operand);
        return node;
    }
    return parse_cmp_expr();
}

// Level 7: == != < <= > >= in not-in is is-not  (chainable)
Expr Parser::parse_cmp_expr()
{
    Expr first = parse_add_expr();

    auto is_cmp_op = [&]() {
        TokenKind k = peek().kind;
        return k == TokenKind::EqEq || k == TokenKind::BangEq ||
               k == TokenKind::Lt   || k == TokenKind::LtEq   ||
               k == TokenKind::Gt   || k == TokenKind::GtEq   ||
               k == TokenKind::In   || k == TokenKind::Is     ||
               (k == TokenKind::Not && peek(1).kind == TokenKind::In) ||
               (k == TokenKind::Is  && peek(1).kind == TokenKind::Not);
    };

    if (!is_cmp_op()) return first;

    // collect chain
    auto chain = std::make_unique<ChainedCmpExpr>();
    chain->span = first->span;
    chain->operands.push_back(std::move(first));

    while (is_cmp_op()) {
        TokenKind op;
        if (peek().kind == TokenKind::Not) { // 'not in'
            advance(); advance(); op = TokenKind::Not; // repurpose Not as "not in" tag
            // ponytail: tag "not in" with a sentinel — compiler disambiguates by context
        } else if (peek().kind == TokenKind::Is && peek(1).kind == TokenKind::Not) {
            advance(); advance(); op = TokenKind::Is; // 'is not' — same trick
        } else {
            op = advance().kind;
        }
        chain->ops.push_back(op);
        skip_newlines();
        chain->operands.push_back(parse_add_expr());
    }

    if (chain->operands.size() == 2) {
        // single comparison → ordinary BinaryExpr
        auto node = std::make_unique<BinaryExpr>();
        node->span  = chain->span;
        node->op    = chain->ops[0];
        node->left  = std::move(chain->operands[0]);
        node->right = std::move(chain->operands[1]);
        return node;
    }
    return chain;
}

// Level 8: + -
Expr Parser::parse_add_expr()
{
    Expr left = parse_mul_expr();
    while (check(TokenKind::Plus) || check(TokenKind::Minus)) {
        TokenKind op = peek().kind;
        Span sp = advance().span;
        skip_newlines();
        Expr right = parse_mul_expr();
        auto node = std::make_unique<BinaryExpr>();
        node->span = sp; node->op = op;
        node->left = std::move(left); node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

// Level 9: * / // %
Expr Parser::parse_mul_expr()
{
    Expr left = parse_unary_expr();
    while (check(TokenKind::Star) || check(TokenKind::Slash) ||
           check(TokenKind::SlashSlash) || check(TokenKind::Percent)) {
        TokenKind op = peek().kind;
        Span sp = advance().span;
        skip_newlines();
        Expr right = parse_unary_expr();
        auto node = std::make_unique<BinaryExpr>();
        node->span = sp; node->op = op;
        node->left = std::move(left); node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

// Level 10: unary minus
Expr Parser::parse_unary_expr()
{
    if (check(TokenKind::Minus)) {
        Span sp = advance().span;
        Expr operand = parse_unary_expr();
        auto node = std::make_unique<UnaryExpr>();
        node->span = sp; node->op = TokenKind::Minus; node->operand = std::move(operand);
        return node;
    }
    return parse_power_expr();
}

// Level 11: ** (right-associative)
Expr Parser::parse_power_expr()
{
    Expr base = parse_postfix_expr();
    if (check(TokenKind::StarStar)) {
        Span sp = advance().span;
        skip_newlines();
        Expr exp = parse_unary_expr(); // right-assoc: recurse into unary
        auto node = std::make_unique<BinaryExpr>();
        node->span = sp; node->op = TokenKind::StarStar;
        node->left = std::move(base); node->right = std::move(exp);
        return node;
    }
    return base;
}

// Level 12: postfix — call / index / slice / field / optional chain
Expr Parser::parse_postfix_expr()
{
    Expr expr = parse_primary();
    while (true) {
        if (check(TokenKind::LParen)) {
            Span sp = advance().span; // consume '('
            skip_newlines();
            std::vector<Arg> args;
            if (!check(TokenKind::RParen)) {
                args = parse_arg_list();
            }
            expect(TokenKind::RParen, "expected ')'");
            auto call = std::make_unique<CallExpr>();
            call->span   = sp;
            call->callee = std::move(expr);
            call->args   = std::move(args);
            expr = std::move(call);

        } else if (check(TokenKind::LBracket)) {
            Span sp = advance().span; // consume '['
            skip_newlines();
            // Slice: [expr? to expr?]  or  [expr]
            if (check(TokenKind::To)) {
                // [to end]
                advance(); // consume 'to'
                skip_newlines();
                Expr end = check(TokenKind::RBracket) ? nullptr : parse_expr();
                expect(TokenKind::RBracket, "expected ']'");
                auto s = std::make_unique<SliceExpr>();
                s->span = sp; s->object = std::move(expr);
                s->end_ = std::move(end);
                expr = std::move(s);
            } else {
                Expr idx = parse_expr();
                if (check(TokenKind::To)) {
                    advance(); // consume 'to'
                    skip_newlines();
                    Expr end = check(TokenKind::RBracket) ? nullptr : parse_expr();
                    expect(TokenKind::RBracket, "expected ']'");
                    auto s = std::make_unique<SliceExpr>();
                    s->span = sp; s->object = std::move(expr);
                    s->start = std::move(idx); s->end_ = std::move(end);
                    expr = std::move(s);
                } else {
                    expect(TokenKind::RBracket, "expected ']'");
                    auto ix = std::make_unique<IndexExpr>();
                    ix->span = sp; ix->object = std::move(expr); ix->index = std::move(idx);
                    expr = std::move(ix);
                }
            }

        } else if (check(TokenKind::Dot) || check(TokenKind::OptChain)) {
            bool is_opt = peek().kind == TokenKind::OptChain;
            Span sp = advance().span;
            std::string field_name;
            // field name can be a regular ident or certain keyword-turned-ident
            if (check(TokenKind::Ident)) {
                field_name = std::string(advance().text(m_source));
            } else {
                // Allow keyword identifiers as field names (e.g., obj.type)
                field_name = std::string(peek().text(m_source));
                advance();
            }
            auto f = std::make_unique<FieldExpr>();
            f->span = sp; f->object = std::move(expr);
            f->field = std::move(field_name); f->is_optional = is_opt;
            expr = std::move(f);

        } else {
            break;
        }
    }
    return expr;
}

// ── Argument list ─────────────────────────────────────────────────────────────

std::vector<Arg> Parser::parse_arg_list()
{
    std::vector<Arg> args;
    do {
        skip_newlines();
        if (check(TokenKind::RParen)) break;
        Arg a;
        a.span = peek().span;
        // Named arg: IDENT ':' expr
        if (check(TokenKind::Ident) && peek(1).kind == TokenKind::Colon) {
            a.name = std::string(advance().text(m_source));
            advance(); // consume ':'
            skip_newlines();
        }
        a.value = parse_expr();
        args.push_back(std::move(a));
        skip_newlines();
    } while (match(TokenKind::Comma));
    return args;
}

// ─────────────────────────────────────────────────────────────────────────────
// Primary expressions
// ─────────────────────────────────────────────────────────────────────────────

Expr Parser::parse_primary()
{
    Span sp = peek().span;
    TokenKind k = peek().kind;

    // Literals
    if (k == TokenKind::Int) {
        auto n = std::make_unique<IntLitExpr>();
        n->span = sp; n->value = peek().int_val;
        advance(); return n;
    }
    if (k == TokenKind::Float) {
        auto n = std::make_unique<FloatLitExpr>();
        n->span = sp; n->value = peek().float_val;
        advance(); return n;
    }
    if (k == TokenKind::Duration) {
        auto n = std::make_unique<DurationLitExpr>();
        n->span = sp; n->ns = peek().duration_ns;
        advance(); return n;
    }
    if (k == TokenKind::True) {
        auto n = std::make_unique<BoolLitExpr>(); n->span = sp; n->value = true;
        advance(); return n;
    }
    if (k == TokenKind::False) {
        auto n = std::make_unique<BoolLitExpr>(); n->span = sp; n->value = false;
        advance(); return n;
    }
    if (k == TokenKind::None) {
        auto n = std::make_unique<NoneLitExpr>(); n->span = sp;
        advance(); return n;
    }

    // String literals
    if (k == TokenKind::String) {
        auto n = std::make_unique<StringLitExpr>();
        n->span = sp; n->raw = std::string(advance().text(m_source));
        return n;
    }
    if (k == TokenKind::StrPart) {
        return parse_interp_string();
    }

    // Identifier (and command keywords used as idents in expression context)
    if (k == TokenKind::Ident || is_cmd_keyword(k)) {
        auto n = std::make_unique<IdentExpr>();
        n->span = sp; n->name = std::string(advance().text(m_source));
        return n;
    }

    // Grouped / tuple: (expr) or (expr,) or (expr, expr, ...)
    if (k == TokenKind::LParen) {
        advance(); // consume '('
        skip_newlines();
        Expr first = parse_expr();
        skip_newlines();
        if (match(TokenKind::RParen)) return first; // grouped

        // Tuple
        auto tup = std::make_unique<TupleExpr>();
        tup->span = sp;
        tup->elements.push_back(std::move(first));
        while (match(TokenKind::Comma)) {
            skip_newlines();
            if (check(TokenKind::RParen)) break; // trailing comma
            tup->elements.push_back(parse_expr());
            skip_newlines();
        }
        expect(TokenKind::RParen, "expected ')'");
        return tup;
    }

    // List literal or comprehension
    if (k == TokenKind::LBracket) {
        advance(); // consume '['
        skip_newlines();
        if (match(TokenKind::RBracket)) {
            // empty list
            auto n = std::make_unique<ListExpr>(); n->span = sp; return n;
        }
        Expr first = parse_expr();
        skip_newlines();
        if (check(TokenKind::For)) {
            // list comprehension
            auto comp = std::make_unique<ListCompExpr>();
            comp->span = sp; comp->body = std::move(first);
            while (check(TokenKind::For)) {
                advance(); // consume 'for'
                CompFor cf; cf.span = peek().span;
                cf.iter1 = std::string(expect(TokenKind::Ident, "expected variable").text(m_source));
                if (match(TokenKind::Comma)) {
                    cf.iter2 = std::string(expect(TokenKind::Ident, "expected variable").text(m_source));
                }
                expect(TokenKind::In, "expected 'in'");
                // pipe level, not full expr: 'if' here is the comp filter, not a ternary
                cf.source = parse_pipe();
                comp->comp_fors.push_back(std::move(cf));
                skip_newlines();
            }
            if (match(TokenKind::If)) {
                comp->filter = parse_expr();
                skip_newlines();
            }
            expect(TokenKind::RBracket, "expected ']'");
            return comp;
        }
        // list literal
        auto list = std::make_unique<ListExpr>();
        list->span = sp;
        list->elements.push_back(std::move(first));
        while (match(TokenKind::Comma)) {
            skip_newlines();
            if (check(TokenKind::RBracket)) break;
            list->elements.push_back(parse_expr());
            skip_newlines();
        }
        expect(TokenKind::RBracket, "expected ']'");
        return list;
    }

    // Map literal or comprehension: {}  { key: val, ... }
    if (k == TokenKind::LBrace) {
        advance(); // consume '{'
        skip_newlines();
        if (match(TokenKind::RBrace)) {
            auto n = std::make_unique<MapExpr>(); n->span = sp; return n;
        }
        // Parse first key
        Expr first_key;
        std::string bare_key;   // set when key was a bare ident
        Span        bare_key_span{};
        if (check(TokenKind::Ident)) {
            // bare ident key → string literal (str_lit adds the quotes that
            // Compiler::unescape() strips; a raw ident text would otherwise
            // lose its first+last char, e.g. "id" → "")
            Span ksp = peek().span;
            std::string kname(peek().text(m_source));
            advance();
            bare_key = kname;
            bare_key_span = ksp;
            first_key = str_lit(ksp, std::move(kname));
        } else {
            first_key = parse_expr();
        }
        expect(TokenKind::Colon, "expected ':' in map literal");
        skip_newlines();
        Expr first_val = parse_expr();
        skip_newlines();

        if (check(TokenKind::For)) {
            // map comprehension
            auto comp = std::make_unique<MapCompExpr>();
            comp->span  = sp;
            // {k: v for k, v in m} — a bare ident key is the loop VARIABLE
            // here, not a string key like in map literals
            if (!bare_key.empty()) {
                auto id  = std::make_unique<IdentExpr>();
                id->span = bare_key_span;
                id->name = bare_key;
                comp->key = std::move(id);
            } else {
                comp->key = std::move(first_key);
            }
            comp->value = std::move(first_val);
            advance(); // consume 'for'
            comp->comp_for.span  = peek().span;
            comp->comp_for.iter1 = std::string(expect(TokenKind::Ident, "expected variable").text(m_source));
            if (match(TokenKind::Comma)) {
                comp->comp_for.iter2 = std::string(expect(TokenKind::Ident, "expected variable").text(m_source));
            }
            expect(TokenKind::In, "expected 'in'");
            // pipe level, not full expr: 'if' here is the comp filter, not a ternary
            comp->comp_for.source = parse_pipe();
            skip_newlines();
            if (match(TokenKind::If)) {
                comp->filter = parse_expr();
                skip_newlines();
            }
            expect(TokenKind::RBrace, "expected '}'");
            return comp;
        }

        // map literal
        auto map = std::make_unique<MapExpr>();
        map->span = sp;
        map->pairs.push_back({std::move(first_key), std::move(first_val)});
        while (match(TokenKind::Comma)) {
            skip_newlines();
            if (check(TokenKind::RBrace)) break;
            Expr key;
            if (check(TokenKind::Ident)) {
                Span ksp = peek().span;
                std::string kname(peek().text(m_source));
                advance(); key = str_lit(ksp, std::move(kname));
            } else {
                key = parse_expr();
            }
            expect(TokenKind::Colon, "expected ':'");
            skip_newlines();
            Expr val = parse_expr();
            map->pairs.push_back({std::move(key), std::move(val)});
            skip_newlines();
        }
        expect(TokenKind::RBrace, "expected '}'");
        return map;
    }

    // Anonymous function: fn(params) { body }
    if (k == TokenKind::Fn) {
        advance(); // consume 'fn'
        expect(TokenKind::LParen, "expected '('");
        skip_newlines();
        std::vector<Param> params;
        if (!check(TokenKind::RParen)) params = parse_param_list();
        expect(TokenKind::RParen, "expected ')'");
        skip_newlines();
        Block body = parse_block();
        auto n = std::make_unique<FnExpr>();
        n->span = sp; n->params = std::move(params); n->body = std::move(body);
        return n;
    }

    // Match expression: match expr { arms }
    if (k == TokenKind::Match) {
        advance(); // consume 'match'
        Expr subject = parse_expr();
        expect(TokenKind::LBrace, "expected '{'");
        skip_newlines();
        auto arms = parse_match_arms();
        Block else_body;
        if (match(TokenKind::Else)) {
            skip_newlines();
            else_body = parse_block();
            skip_newlines();
        }
        expect(TokenKind::RBrace, "expected '}'");
        auto n = std::make_unique<MatchExpr>();
        n->span      = sp;
        n->subject   = std::move(subject);
        n->arms      = std::move(arms);
        n->else_body = std::move(else_body);
        return n;
    }

    // Type keywords usable as identifiers in expression context
    if (k == TokenKind::KwInt || k == TokenKind::KwFloat || k == TokenKind::KwString ||
        k == TokenKind::KwBool || k == TokenKind::KwList || k == TokenKind::KwMap ||
        k == TokenKind::KwTuple) {
        auto n = std::make_unique<IdentExpr>();
        n->span = sp; n->name = std::string(advance().text(m_source));
        return n;
    }

    return error_expr(sp, "expected expression");
}

// ── Interpolated string ───────────────────────────────────────────────────────

Expr Parser::parse_interp_string()
{
    Span sp = peek().span;
    auto node = std::make_unique<InterpStringExpr>();
    node->span = sp;

    while (!check(TokenKind::Eof)) {
        if (check(TokenKind::StrPart)) {
            InterpPart part;
            part.is_str = true;
            part.text   = std::string(advance().text(m_source));
            node->parts.push_back(std::move(part));
        } else if (check(TokenKind::InterpOpen)) {
            advance(); // consume '{'
            InterpPart part;
            part.is_str = false;
            part.expr   = parse_expr();
            expect(TokenKind::InterpClose, "expected '}' to close interpolation");
            node->parts.push_back(std::move(part));
        } else if (check(TokenKind::String)) {
            // closing sentinel (just the `"`)
            advance();
            break;
        } else {
            break;
        }
    }
    return node;
}

// ── Pattern parsing ───────────────────────────────────────────────────────────

Pattern Parser::parse_pattern()
{
    Pattern pat;
    pat.span = peek().span;

    // Wildcard
    if (match(TokenKind::Wildcard)) {
        pat.kind = PatternKind::Wildcard;
        return pat;
    }

    // Literal: numbers, strings, bool, none
    if (check(TokenKind::Int)) {
        pat.kind = PatternKind::Literal;
        pat.lit_kind = TokenKind::Int;
        pat.int_val  = peek().int_val;
        advance(); return pat;
    }
    if (check(TokenKind::Float)) {
        pat.kind = PatternKind::Literal;
        pat.lit_kind = TokenKind::Float;
        pat.float_val = peek().float_val;
        advance(); return pat;
    }
    if (check(TokenKind::Minus) && peek(1).kind == TokenKind::Int) {
        advance(); // consume '-'
        pat.kind = PatternKind::Literal;
        pat.lit_kind = TokenKind::Int;
        pat.int_val  = -peek().int_val;
        advance(); return pat;
    }
    if (check(TokenKind::String)) {
        pat.kind = PatternKind::Literal;
        pat.lit_kind = TokenKind::String;
        pat.str_val  = std::string(advance().text(m_source));
        return pat;
    }
    if (check(TokenKind::True)) {
        pat.kind = PatternKind::Literal; pat.lit_kind = TokenKind::True;
        pat.int_val = 1; advance(); return pat;
    }
    if (check(TokenKind::False)) {
        pat.kind = PatternKind::Literal; pat.lit_kind = TokenKind::False;
        pat.int_val = 0; advance(); return pat;
    }
    if (check(TokenKind::None)) {
        pat.kind = PatternKind::Literal; pat.lit_kind = TokenKind::None;
        advance(); return pat;
    }

    // Type patterns
    if (check(TokenKind::KwInt) || check(TokenKind::KwFloat) ||
        check(TokenKind::KwString) || check(TokenKind::KwBool) ||
        check(TokenKind::KwList) || check(TokenKind::KwMap) ||
        check(TokenKind::KwTuple)) {
        pat.kind   = PatternKind::TypeCheck;
        pat.type_kw = peek().kind;
        advance(); return pat;
    }

    // Tuple pattern: (pat, pat, ...)
    if (match(TokenKind::LParen)) {
        pat.kind = PatternKind::Tuple;
        while (!check(TokenKind::RParen) && !check(TokenKind::Eof)) {
            pat.children.push_back(parse_pattern());
            if (!match(TokenKind::Comma)) break;
        }
        expect(TokenKind::RParen, "expected ')'");
        return pat;
    }

    // Capture pattern: any ident
    if (check(TokenKind::Ident)) {
        pat.kind = PatternKind::Capture;
        pat.name = std::string(advance().text(m_source));
        return pat;
    }

    m_diag.error(m_source.location_of(peek().span.start), "expected pattern");
    pat.kind = PatternKind::Wildcard;
    return pat;
}

} // namespace syn
