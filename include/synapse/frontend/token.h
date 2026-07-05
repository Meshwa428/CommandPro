#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include "synapse/common/source.h"

namespace syn {

// ── TokenKind ─────────────────────────────────────────────────────────────────
// Every terminal in the grammar gets exactly one kind.
// Grouped by category for readability.
enum class TokenKind : uint16_t
{
    // ── Literals ──────────────────────────────────────────────────────────
    Int,         // 42  0xFF  0b1010  0o17
    Float,       // 3.14  1e10
    Duration,    // 2s  500ms  1m  0.5h
    String,      // "hello"  r"raw"  """multi"""
    True,        // true
    False,       // false
    None,        // none

    // ── Identifier ────────────────────────────────────────────────────────
    Ident,       // any user-defined name that isn't a keyword

    // ── Language keywords ─────────────────────────────────────────────────
    Let,
    Const,
    Fn,
    Return,
    If,
    Else,
    While,
    Repeat,
    For,
    In,
    Break,
    Continue,
    Match,
    Case,
    Try,
    Catch,
    Finally,
    Throw,
    And,
    Or,
    Not,
    Is,
    Use,
    As,

    // ── Type-name keywords (reserved in patterns, ident elsewhere) ────────
    KwInt,       // int
    KwFloat,     // float
    KwString,    // string
    KwBool,      // bool
    KwList,      // list
    KwMap,       // map
    KwTuple,     // tuple

    // ── Command keywords ──────────────────────────────────────────────────
    Say,
    Mouse,
    Click,
    Drag,
    Scroll,
    Hold,
    Release,
    Press,
    Type,
    Run,
    Open,
    Close,
    Focus,
    Move,
    Resize,
    Maximize,
    Minimize,
    Capture,
    Wait,
    Find,
    See,
    Tap,
    Check,
    Uncheck,
    Select,
    Read,

    // ── Command modifiers ─────────────────────────────────────────────────
    Left,        // left (mouse button / scroll dir)
    Right,       // right
    Middle,      // middle
    Up,          // scroll up
    Down,        // scroll down
    MinConfidence, // min_confidence

    // ── Element type keywords ─────────────────────────────────────────────
    Button,
    Input,
    Checkbox,
    Radio,
    Dropdown,
    Link,
    Icon,
    Toggle,
    Slider,
    Tab,
    MenuItem,    // menu_item

    // ── Arithmetic operators ───────────────────────────────────────────────
    Plus,        // +
    Minus,       // -
    Star,        // *
    Slash,       // /
    SlashSlash,  // //
    Percent,     // %
    StarStar,    // **

    // ── Comparison operators ───────────────────────────────────────────────
    EqEq,        // ==
    BangEq,      // !=
    Lt,          // <
    LtEq,        // <=
    Gt,          // >
    GtEq,        // >=

    // ── Assignment operators ───────────────────────────────────────────────
    Eq,          // =
    PlusEq,      // +=
    MinusEq,     // -=
    StarEq,      // *=
    SlashEq,     // /=
    SlashSlashEq,// //=
    PercentEq,   // %=
    StarStarEq,  // **=
    QQEq,        // ??=

    // ── Other operators ────────────────────────────────────────────────────
    QQ,          // ??
    Pipe,        // |>
    OptChain,    // ?.
    Dot,         // .
    DotDot,      // .. (reserved, unused in v2)

    // ── Delimiters ─────────────────────────────────────────────────────────
    LParen,      // (
    RParen,      // )
    LBracket,    // [
    RBracket,    // ]
    LBrace,      // {
    RBrace,      // }
    Comma,       // ,
    Colon,       // :
    Semicolon,   // ;

    // ── Special ────────────────────────────────────────────────────────────
    Newline,     // '\n' — statement terminator
    InterpOpen,  // { inside a string  — starts interpolation
    InterpClose, // } closing an interpolation
    StrPart,     // literal text fragment inside an interpolated string
    To,          // to  (range operator — context-sensitive keyword)
    By,          // by  (range step — context-sensitive keyword)
    Wildcard,    // _

    // ── Sentinels ──────────────────────────────────────────────────────────
    Eof,
    Error,       // lexer error token (bad char / unterminated string)
};

// ── Token ─────────────────────────────────────────────────────────────────────
struct Token
{
    TokenKind  kind;
    Span       span;    // byte range in source
    // Parsed value (filled lazily or by lexer for literals)
    union {
        int64_t  int_val;
        double   float_val;
        int64_t  duration_ns; // nanoseconds
    };

    Token() : kind(TokenKind::Error), span{}, int_val(0) {}
    Token(TokenKind k, Span s) : kind(k), span(s), int_val(0) {}
    Token(TokenKind k, Span s, int64_t v) : kind(k), span(s), int_val(v) {}
    Token(TokenKind k, Span s, double v)  : kind(k), span(s), float_val(v) {}

    // Get the raw source text of this token
    std::string_view text(const Source& src) const { return span.view(src); }

    bool is(TokenKind k)          const { return kind == k; }
    bool is_literal()             const;
    bool is_keyword()             const;
    bool is_command_keyword()     const;
    bool is_aug_assign()          const;
    bool is_term()                const; // Newline or Semicolon

    // For diagnostics and debugging
    std::string to_string()       const;
    static std::string_view kind_name(TokenKind k);
};

} // namespace syn
