#pragma once
#include <vector>
#include "synapse/common/source.h"
#include "synapse/common/diag.h"
#include "synapse/frontend/token.h"

namespace syn {

struct InterpContext
{
    int brace_level_before;
};

// ── Lexer ─────────────────────────────────────────────────────────────────────
// Hand-written, zero-copy lexer.
// Produces a flat token stream over the source text.
class Lexer
{
public:
    explicit Lexer(const Source& source);

    // Lex all tokens eagerly and return them.
    // Always ends with TokenKind::Eof.
    std::vector<Token> tokenize();

    // Lex one token (used internally and for streaming).
    Token next_token();

    bool has_errors() const { return m_diag.has_errors(); }
    const DiagEngine& diag() const { return m_diag; }

private:
    // ── Cursor helpers ────────────────────────────────────────────────────
    char        peek(int offset = 0) const;
    char        advance();
    bool        at_end()             const { return m_pos >= m_source.size(); }
    void        skip_whitespace_and_comments();
    Span        make_span(std::size_t start) const { return {start, m_pos}; }
    SourceLocation loc_at(std::size_t offset) const { return m_source.location_of(offset); }

    // ── Token builders ────────────────────────────────────────────────────
    Token make(TokenKind k, std::size_t start) { return Token(k, make_span(start)); }
    Token error_token(std::size_t start, std::string msg);

    // ── Lexing sub-methods ────────────────────────────────────────────────
    Token lex_number(std::size_t start);      // int, float, duration
    Token lex_string(std::size_t start);      // interp string
    Token lex_raw_string(std::size_t start);  // r"..."
    Token lex_multiline_string(std::size_t start); // """..."""
    Token lex_ident_or_keyword(std::size_t start);
    Token lex_operator(std::size_t start);    // all punctuation/operators

    // Duration suffix after a numeric literal
    bool try_lex_duration(Token& num_tok, std::size_t start);

    // Keyword lookup
    static TokenKind keyword_kind(std::string_view word);

    // ── State ─────────────────────────────────────────────────────────────
    const Source& m_source;
    DiagEngine    m_diag;
    std::size_t   m_pos          = 0;
    bool          m_last_was_term = true; // for implicit newline insertion
    
    // String interpolation state
    std::vector<InterpContext> m_interp_stack;
    int  m_brace_level = 0;
    bool m_resume_string = false;
    bool m_expect_interp_open = false;
    bool m_expect_string_end = false;
};

} // namespace syn
