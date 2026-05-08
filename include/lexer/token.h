#pragma once
#include <string>

namespace Synapse {

enum class TokenType {
    // ── Literals ──────────────────────────────────────────────
    IDENTIFIER,
    NUMBER,          // 42 or 3.14
    STRING,          // "hello"
    TIME_LIT,        // 500ms, 2s, 5m, 1.5h
    TRUE_LIT,        // true
    FALSE_LIT,       // false
    NULL_LIT,        // null

    // ── Core Keywords ─────────────────────────────────────────
    LET,
    FN,
    RETURN,
    IF,
    ELSE,
    REPEAT,
    TIMES,
    LOOP,
    WHILE,
    PRINT,
    PRINTLN,
    ASK,
    INTO,
    AS,
    WAIT,
    TRY,
    CATCH,

    // ── Automation Keywords ───────────────────────────────────
    MOUSE,
    KEY,
    WINDOW,
    APP,
    SCREEN,
    MOVE,
    CLICK,
    DRAG,
    SCROLL,
    HOLD,
    RELEASE,
    PRESS,
    TYPE_KW,         // TYPE keyword (avoid clash with C++ 'type')
    CAPTURE,
    FOCUS,
    OPEN,
    CLOSE,
    MINIMIZE,
    MAXIMIZE,
    RESTORE,
    RESIZE,
    EXISTS,
    INTERVAL,
    RUN,
    AT,
    FROM,
    TO,
    SPEED,
    LEFT,
    RIGHT,
    MIDDLE,
    UP,
    DOWN,
    BUTTON,

    // ── Operators ─────────────────────────────────────────────
    PLUS,            // +
    MINUS,           // -
    STAR,            // *
    SLASH,           // /
    PERCENT,         // %
    POWER,           // **
    FLOORDIV,        // //

    // ── Comparison ────────────────────────────────────────────
    EQEQ,            // ==
    NEQ,             // !=
    LT,              // <
    GT,              // >
    LEQ,             // <=
    GEQ,             // >=
    STRICT_EQ,       // ===

    // ── Logical ───────────────────────────────────────────────
    AND,             // AND
    OR,              // OR
    NOT,             // NOT

    // ── Bitwise ───────────────────────────────────────────────
    AMP,             // &
    PIPE,            // |
    CARET,           // ^
    TILDE,           // ~
    LSHIFT,          // <<
    RSHIFT,          // >>

    // ── Assignment ────────────────────────────────────────────
    EQUALS,          // =
    PLUS_EQ,         // +=
    MINUS_EQ,        // -=
    STAR_EQ,         // *=
    SLASH_EQ,        // /=
    PERCENT_EQ,      // %=
    FLOORDIV_EQ,     // //=
    POWER_EQ,        // **=
    AMP_EQ,          // &=
    PIPE_EQ,         // |=
    CARET_EQ,        // ^=
    LSHIFT_EQ,       // <<=
    RSHIFT_EQ,       // >>=
    WALRUS,          // :=

    // ── Membership / Identity ─────────────────────────────────
    IN,              // IN
    IS,              // IS

    // ── Punctuation ───────────────────────────────────────────
    LPAREN,          // (
    RPAREN,          // )
    LBRACE,          // {
    RBRACE,          // }
    SEMICOLON,       // ;
    COMMA,           // ,
    COLON,           // :
    DOT,             // .

    // ── Special ───────────────────────────────────────────────
    END_OF_FILE,
    UNKNOWN
};

struct Token {
    TokenType   type;
    std::string value;
    int         line;
    int         column;

    Token()
        : type(TokenType::UNKNOWN), value(""), line(0), column(0) {}

    Token(TokenType t, std::string v, int ln, int col)
        : type(t), value(std::move(v)), line(ln), column(col) {}
};

// Utility: human-readable token name for error messages
const char* tokenTypeName(TokenType t);

} // namespace Synapse
