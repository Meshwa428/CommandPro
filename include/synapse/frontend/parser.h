#pragma once
#include <vector>
#include "synapse/common/source.h"
#include "synapse/common/diag.h"
#include "synapse/frontend/token.h"
#include "synapse/frontend/ast.h"

namespace syn {

class Parser {
public:
    Parser(std::vector<Token> tokens, const Source& source);

    Program parse();

    bool             has_errors() const { return m_diag.has_errors(); }
    const DiagEngine& diag()      const { return m_diag; }

private:
    // ── Token navigation ──────────────────────────────────────────────────────
    const Token& peek(int offset = 0) const;
    Token        advance();
    bool         check(TokenKind k) const;
    bool         match(TokenKind k);
    Token        expect(TokenKind k, std::string_view msg);
    void         skip_newlines();
    void         consume_term();
    bool         at_term() const;
    bool         is_cmd_keyword(TokenKind k) const;
    bool         is_aug_assign(TokenKind k)  const;
    bool         is_btn(TokenKind k)         const; // left/right/middle
    bool         is_scroll_dir(TokenKind k)  const; // up/down/left/right
    bool         is_elem_type(TokenKind k)   const;
    bool         is_key_name(TokenKind k)    const;
    bool         is_lvalue(const ExprNode* e) const;

    // ── Statement parsers ────────────────────────────────────────────────────
    Block parse_stmt_list();
    Block parse_block();
    Stmt  parse_stmt();
    Stmt  parse_let_stmt();
    Stmt  parse_const_stmt();
    Stmt  parse_if_stmt();
    Stmt  parse_while_stmt();
    Stmt  parse_repeat_stmt();
    Stmt  parse_for_stmt();
    Stmt  parse_match_stmt();
    Stmt  parse_try_stmt();
    Stmt  parse_fn_decl();
    Stmt  parse_return_stmt();
    Stmt  parse_throw_stmt();
    Stmt  parse_use_stmt();
    Stmt  parse_in_scope_stmt();
    Stmt  parse_assign_or_expr_stmt();

    // ── Command parsers (desugar to CallExpr) ────────────────────────────────
    Stmt parse_say_stmt();
    Stmt parse_cmd_stmt();
    Stmt parse_mouse_cmd();
    Stmt parse_click_cmd();
    Stmt parse_drag_cmd();
    Stmt parse_scroll_cmd();
    Stmt parse_hold_release_cmd(TokenKind which);
    Stmt parse_press_cmd();
    Stmt parse_type_cmd();
    Stmt parse_run_open_close_focus_cmd(TokenKind which);
    Stmt parse_move_resize_cmd(TokenKind which);
    Stmt parse_maximize_minimize_cmd(TokenKind which);
    Stmt parse_capture_cmd();
    Stmt parse_wait_cmd();
    Stmt parse_ui_elem_cmd(TokenKind which); // tap/find/see
    Stmt parse_check_cmd();
    Stmt parse_uncheck_cmd();
    Stmt parse_select_cmd();
    Stmt parse_read_cmd();

    // ── Expression parsers (Pratt via recursive descent) ─────────────────────
    Expr parse_expr();
    Expr parse_ternary();
    Expr parse_pipe();
    Expr parse_null_coalesce();
    Expr parse_or_expr();
    Expr parse_and_expr();
    Expr parse_not_expr();
    Expr parse_cmp_expr();
    Expr parse_add_expr();
    Expr parse_mul_expr();
    Expr parse_unary_expr();
    Expr parse_power_expr();
    Expr parse_postfix_expr();
    Expr parse_primary();
    Expr parse_interp_string();

    // ── Sub-parsers ───────────────────────────────────────────────────────────
    std::vector<Param>    parse_param_list();
    std::vector<Arg>      parse_arg_list();
    Pattern               parse_pattern();
    std::vector<MatchArm> parse_match_arms();
    std::string           parse_key_chord();

    // ── Desugar helpers ───────────────────────────────────────────────────────
    Stmt  cmd_call(Span span, std::string obj, std::string method,
                   std::vector<Arg> args);
    Arg   pos_arg(Expr e);
    Arg   named_arg(std::string name, Expr e);
    Expr  str_lit(Span span, std::string s);
    Expr  int_lit(Span span, int64_t v);

    // ── Error recovery ────────────────────────────────────────────────────────
    Stmt  error_stmt(Span span, std::string msg);
    Expr  error_expr(Span span, std::string msg);
    void  sync_to_next_stmt();

    // ── State ─────────────────────────────────────────────────────────────────
    std::vector<Token> m_tokens;
    const Source&      m_source;
    DiagEngine         m_diag;
    std::size_t        m_pos = 0;
};

} // namespace syn
