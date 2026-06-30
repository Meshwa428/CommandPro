#include "synapse/frontend/lexer.h"
#include <cctype>
#include <algorithm>

namespace syn {

Lexer::Lexer(const Source& source)
    : m_source(source)
{
}

char Lexer::peek(int offset) const
{
    if (m_pos + offset >= m_source.size()) return '\0';
    return m_source[m_pos + offset];
}

char Lexer::advance()
{
    if (at_end()) return '\0';
    return m_source[m_pos++];
}

void Lexer::skip_whitespace_and_comments()
{
    while (!at_end()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
        } else if (c == '#') {
            // Comment to end of line
            while (!at_end() && peek() != '\n') {
                advance();
            }
        } else {
            break;
        }
    }
}

Token Lexer::error_token(std::size_t start, std::string msg)
{
    m_diag.error(loc_at(start), msg);
    return Token(TokenKind::Error, make_span(start));
}

std::vector<Token> Lexer::tokenize()
{
    std::vector<Token> tokens;
    while (true) {
        Token tok = next_token();
        tokens.push_back(tok);
        if (tok.is(TokenKind::Eof)) {
            break;
        }
    }
    return tokens;
}

Token Lexer::next_token()
{
    if (m_expect_string_end) {
        m_expect_string_end = false;
        if (peek() == '"') {
            std::size_t start = m_pos;
            advance();
            return Token(TokenKind::String, make_span(start));
        }
    }

    // Resume string parsing if we just popped an interpolation
    if (m_resume_string) {
        m_resume_string = false;
        std::size_t start = m_pos;
        while (!at_end() && peek() != '"' && peek() != '{') {
            if (peek() == '\\') advance();
            advance();
        }
        if (peek() == '{') {
            m_expect_interp_open = true;
        } else if (peek() == '"') {
            m_expect_string_end = true;
        }
        return Token(TokenKind::StrPart, make_span(start));
    }

    skip_whitespace_and_comments();

    if (at_end()) {
        return make(TokenKind::Eof, m_pos);
    }

    std::size_t start = m_pos;
    char c = peek();

    // Check for interpolation opening brace `{` in interpolation-expectant state
    if (c == '{' && !m_interp_stack.empty() && m_interp_stack.back().brace_level_before == m_brace_level && m_pos > 0 && m_source[m_pos - 1] != '\\') {
        // Wait, if it's the start of interpolation, it was triggered by a `{` inside string scanning.
        // We handle this directly in the operator parsing if we detect it, but let's check it here.
    }

    c = advance();

    // Significant newline handling
    if (c == '\n') {
        bool discard = (m_paren_level > 0);  // suppress inside () and [] only
        if (discard || m_last_was_term) {
            m_last_was_term = true;
            return next_token(); // skip
        }
        m_last_was_term = true;
        return Token(TokenKind::Newline, make_span(start));
    }

    m_last_was_term = false;

    // Check for string start in normal mode
    if (c == '"') {
        if (peek(0) == '"' && peek(1) == '"') {
            advance();
            advance();
            return lex_multiline_string(start);
        }
        return lex_string(start);
    }

    // Check for raw string r"..."
    if (c == 'r' && peek() == '"') {
        advance();
        return lex_raw_string(start);
    }

    // Identifiers and keywords
    if (std::isalpha(c) || c == '_') {
        return lex_ident_or_keyword(start);
    }

    // Numbers (integers, floats, durations)
    if (std::isdigit(c)) {
        return lex_number(start);
    }

    // Operators and delimiters
    m_pos = start; // back up to lex operator
    return lex_operator(start);
}

Token Lexer::lex_number(std::size_t start)
{
    char c = m_source[start];
    bool is_hex = false, is_bin = false, is_oct = false;

    if (c == '0') {
        char next = peek();
        if (next == 'x' || next == 'X') {
            advance(); advance(); is_hex = true;
        } else if (next == 'b' || next == 'B') {
            advance(); advance(); is_bin = true;
        } else if (next == 'o' || next == 'O') {
            advance(); advance(); is_oct = true;
        }
    }

    if (is_hex) {
        while (!at_end() && (std::isxdigit(peek()) || peek() == '_')) advance();
        std::string s;
        for (std::size_t i = start + 2; i < m_pos; ++i) {
            if (m_source[i] != '_') s.push_back(m_source[i]);
        }
        int64_t val = std::stoll(s, nullptr, 16);
        Token tok(TokenKind::Int, make_span(start), val);
        if (try_lex_duration(tok, start)) return tok;
        return tok;
    }

    if (is_bin) {
        while (!at_end() && (peek() == '0' || peek() == '1' || peek() == '_')) advance();
        std::string s;
        for (std::size_t i = start + 2; i < m_pos; ++i) {
            if (m_source[i] != '_') s.push_back(m_source[i]);
        }
        int64_t val = std::stoll(s, nullptr, 2);
        Token tok(TokenKind::Int, make_span(start), val);
        if (try_lex_duration(tok, start)) return tok;
        return tok;
    }

    if (is_oct) {
        while (!at_end() && ((peek() >= '0' && peek() <= '7') || peek() == '_')) advance();
        std::string s;
        for (std::size_t i = start + 2; i < m_pos; ++i) {
            if (m_source[i] != '_') s.push_back(m_source[i]);
        }
        int64_t val = std::stoll(s, nullptr, 8);
        Token tok(TokenKind::Int, make_span(start), val);
        if (try_lex_duration(tok, start)) return tok;
        return tok;
    }

    // Decimal int or float
    bool is_float = false;
    while (!at_end() && (std::isdigit(peek()) || peek() == '_')) advance();

    if (peek() == '.' && std::isdigit(peek(1))) {
        is_float = true;
        advance(); // consume '.'
        while (!at_end() && (std::isdigit(peek()) || peek() == '_')) advance();
    }

    if (peek() == 'e' || peek() == 'E') {
        is_float = true;
        advance();
        if (peek() == '+' || peek() == '-') advance();
        while (!at_end() && std::isdigit(peek())) advance();
    }

    std::string s;
    for (std::size_t i = start; i < m_pos; ++i) {
        if (m_source[i] != '_') s.push_back(m_source[i]);
    }

    if (is_float) {
        double val = std::stod(s);
        Token tok(TokenKind::Float, make_span(start), val);
        if (try_lex_duration(tok, start)) return tok;
        return tok;
    } else {
        int64_t val = std::stoll(s);
        Token tok(TokenKind::Int, make_span(start), val);
        if (try_lex_duration(tok, start)) return tok;
        return tok;
    }
}

bool Lexer::try_lex_duration(Token& num_tok, std::size_t start)
{
    std::size_t saved = m_pos;
    std::string unit;
    while (!at_end() && std::isalpha(peek())) {
        unit.push_back(advance());
    }

    double multiplier = 0.0;
    if (unit == "ms") {
        multiplier = 1000000.0;
    } else if (unit == "s") {
        multiplier = 1000000000.0;
    } else if (unit == "m") {
        multiplier = 60000000000.0;
    } else if (unit == "h") {
        multiplier = 3600000000000.0;
    } else {
        m_pos = saved;
        return false;
    }

    double base_val = 0.0;
    if (num_tok.kind == TokenKind::Int) {
        base_val = static_cast<double>(num_tok.int_val);
    } else {
        base_val = num_tok.float_val;
    }

    num_tok.kind = TokenKind::Duration;
    num_tok.duration_ns = static_cast<int64_t>(base_val * multiplier);
    num_tok.span = make_span(start);
    return true;
}

Token Lexer::lex_raw_string(std::size_t start)
{
    while (!at_end() && peek() != '"') {
        advance();
    }
    if (at_end()) {
        return error_token(start, "Unterminated raw string literal");
    }
    advance(); // closing quote
    return Token(TokenKind::String, make_span(start));
}

Token Lexer::lex_multiline_string(std::size_t start)
{
    while (!at_end()) {
        if (peek(0) == '"' && peek(1) == '"' && peek(2) == '"') {
            advance(); advance(); advance();
            return Token(TokenKind::String, make_span(start));
        }
        advance();
    }
    return error_token(start, "Unterminated multiline string literal");
}

Token Lexer::lex_string(std::size_t start)
{
    // Scan ahead to see if there is an unescaped `{`
    std::size_t scan_pos = m_pos;
    bool has_interp = false;
    while (scan_pos < m_source.size()) {
        char c = m_source[scan_pos];
        if (c == '"') break;
        if (c == '{') {
            if (scan_pos + 1 < m_source.size() && m_source[scan_pos + 1] == '{') {
                scan_pos += 2;
                continue;
            }
            has_interp = true;
            break;
        }
        if (c == '\\') {
            scan_pos += 2;
        } else {
            scan_pos++;
        }
    }

    if (!has_interp) {
        while (!at_end() && peek() != '"') {
            if (peek() == '\\') advance();
            advance();
        }
        if (at_end()) {
            return error_token(start, "Unterminated string literal");
        }
        advance(); // closing quote
        return Token(TokenKind::String, make_span(start));
    }

    // Interpolated string: yield first part
    std::size_t part_start = m_pos;
    while (!at_end() && peek() != '"' && peek() != '{') {
        if (peek() == '\\') advance();
        advance();
    }

    if (peek() == '{') {
        m_expect_interp_open = true;
    }

    return Token(TokenKind::StrPart, make_span(part_start));
}

Token Lexer::lex_ident_or_keyword(std::size_t start)
{
    while (!at_end() && (std::isalnum(peek()) || peek() == '_')) {
        advance();
    }

    std::string_view text = m_source.text().substr(start, m_pos - start);
    TokenKind kind = keyword_kind(text);
    return Token(kind, make_span(start));
}

TokenKind Lexer::keyword_kind(std::string_view word)
{
    if (word == "let")      return TokenKind::Let;
    if (word == "const")    return TokenKind::Const;
    if (word == "fn")       return TokenKind::Fn;
    if (word == "return")   return TokenKind::Return;
    if (word == "if")       return TokenKind::If;
    if (word == "else")     return TokenKind::Else;
    if (word == "while")    return TokenKind::While;
    if (word == "repeat")   return TokenKind::Repeat;
    if (word == "for")      return TokenKind::For;
    if (word == "in")       return TokenKind::In;
    if (word == "break")    return TokenKind::Break;
    if (word == "continue") return TokenKind::Continue;
    if (word == "match")    return TokenKind::Match;
    if (word == "case")     return TokenKind::Case;
    if (word == "try")      return TokenKind::Try;
    if (word == "catch")    return TokenKind::Catch;
    if (word == "finally")  return TokenKind::Finally;
    if (word == "throw")    return TokenKind::Throw;
    if (word == "and")      return TokenKind::And;
    if (word == "or")       return TokenKind::Or;
    if (word == "not")      return TokenKind::Not;
    if (word == "is")       return TokenKind::Is;
    if (word == "use")      return TokenKind::Use;
    if (word == "as")       return TokenKind::As;
    if (word == "true")     return TokenKind::True;
    if (word == "false")    return TokenKind::False;
    if (word == "none")     return TokenKind::None;

    if (word == "int")      return TokenKind::KwInt;
    if (word == "float")    return TokenKind::KwFloat;
    if (word == "string")   return TokenKind::KwString;
    if (word == "bool")     return TokenKind::KwBool;
    if (word == "list")     return TokenKind::KwList;
    if (word == "map")      return TokenKind::KwMap;
    if (word == "tuple")    return TokenKind::KwTuple;

    if (word == "mouse")    return TokenKind::Mouse;
    if (word == "click")    return TokenKind::Click;
    if (word == "drag")     return TokenKind::Drag;
    if (word == "scroll")   return TokenKind::Scroll;
    if (word == "hold")     return TokenKind::Hold;
    if (word == "release")  return TokenKind::Release;
    if (word == "press")    return TokenKind::Press;
    if (word == "type")     return TokenKind::Type;
    if (word == "run")      return TokenKind::Run;
    if (word == "open")     return TokenKind::Open;
    if (word == "close")    return TokenKind::Close;
    if (word == "focus")    return TokenKind::Focus;
    if (word == "move")     return TokenKind::Move;
    if (word == "resize")   return TokenKind::Resize;
    if (word == "maximize") return TokenKind::Maximize;
    if (word == "minimize") return TokenKind::Minimize;
    if (word == "capture")  return TokenKind::Capture;
    if (word == "wait")     return TokenKind::Wait;
    if (word == "find")     return TokenKind::Find;
    if (word == "see")      return TokenKind::See;
    if (word == "tap")      return TokenKind::Tap;
    if (word == "check")    return TokenKind::Check;
    if (word == "uncheck")  return TokenKind::Uncheck;
    if (word == "select")   return TokenKind::Select;
    if (word == "read")     return TokenKind::Read;

    if (word == "left")     return TokenKind::Left;
    if (word == "right")    return TokenKind::Right;
    if (word == "middle")   return TokenKind::Middle;
    if (word == "up")       return TokenKind::Up;
    if (word == "down")     return TokenKind::Down;
    if (word == "min_confidence") return TokenKind::MinConfidence;
    if (word == "button")   return TokenKind::Button;
    if (word == "input")    return TokenKind::Input;
    if (word == "checkbox") return TokenKind::Checkbox;
    if (word == "radio")    return TokenKind::Radio;
    if (word == "dropdown") return TokenKind::Dropdown;
    if (word == "link")     return TokenKind::Link;
    if (word == "icon")     return TokenKind::Icon;
    if (word == "toggle")   return TokenKind::Toggle;
    if (word == "slider")   return TokenKind::Slider;
    if (word == "tab")      return TokenKind::Tab;
    if (word == "menu_item") return TokenKind::MenuItem;
    if (word == "to")       return TokenKind::To;
    if (word == "by")       return TokenKind::By;

    return TokenKind::Ident;
}

Token Lexer::lex_operator(std::size_t start)
{
    char c = advance();
    switch (c) {
        case '+':
            if (peek() == '=') { advance(); return make(TokenKind::PlusEq, start); }
            return make(TokenKind::Plus, start);
        case '-':
            if (peek() == '=') { advance(); return make(TokenKind::MinusEq, start); }
            return make(TokenKind::Minus, start);
        case '*':
            if (peek() == '*') {
                advance();
                if (peek() == '=') { advance(); return make(TokenKind::StarStarEq, start); }
                return make(TokenKind::StarStar, start);
            }
            if (peek() == '=') { advance(); return make(TokenKind::StarEq, start); }
            return make(TokenKind::Star, start);
        case '/':
            if (peek() == '/') {
                advance();
                if (peek() == '=') { advance(); return make(TokenKind::SlashSlashEq, start); }
                return make(TokenKind::SlashSlash, start);
            }
            if (peek() == '=') { advance(); return make(TokenKind::SlashEq, start); }
            return make(TokenKind::Slash, start);
        case '%':
            if (peek() == '=') { advance(); return make(TokenKind::PercentEq, start); }
            return make(TokenKind::Percent, start);
        case '=':
            if (peek() == '=') { advance(); return make(TokenKind::EqEq, start); }
            return make(TokenKind::Eq, start);
        case '!':
            if (peek() == '=') { advance(); return make(TokenKind::BangEq, start); }
            break;
        case '<':
            if (peek() == '=') { advance(); return make(TokenKind::LtEq, start); }
            return make(TokenKind::Lt, start);
        case '>':
            if (peek() == '=') {
                advance();
                return make(TokenKind::GtEq, start);
            }
            return make(TokenKind::Gt, start);
        case '|':
            if (peek() == '>') { advance(); return make(TokenKind::Pipe, start); }
            break;
        case '?':
            if (peek() == '?') {
                advance();
                if (peek() == '=') { advance(); return make(TokenKind::QQEq, start); }
                return make(TokenKind::QQ, start);
            }
            if (peek() == '.') { advance(); return make(TokenKind::OptChain, start); }
            break;
        case '.':
            return make(TokenKind::Dot, start);
        case ',': return make(TokenKind::Comma, start);
        case ':': return make(TokenKind::Colon, start);
        case ';': return make(TokenKind::Semicolon, start);
        case '(': m_paren_level++; return make(TokenKind::LParen, start);
        case ')': if (m_paren_level > 0) m_paren_level--; return make(TokenKind::RParen, start);
        case '[': m_paren_level++; return make(TokenKind::LBracket, start);
        case ']': if (m_paren_level > 0) m_paren_level--; return make(TokenKind::RBracket, start);
        case '_': return make(TokenKind::Wildcard, start);

        case '{': {
            // Check if we are starting a string interpolation
            // If the last token was StrPart, and this is `{`, then we yield InterpOpen
            // Wait, we can track this! If the character immediately preceding `{` is within the string start,
            // or if the previous token was a StrPart and this is `{`.
            // Let's implement it simply: we check if we were in the middle of scanning an interpolated string.
            // If the parser calls next_token, and `m_pos` points to `{` right after a StrPart:
            // Since we know we just yielded `StrPart`, the character at `start` is `{`.
            // Let's check: was the character before `{` part of the string?
            // Yes, if we are inside a string interpolation (we haven't finished the string yet, and the closing `"` is still to come).
            // We can know this by checking if the previous character was NOT a closing double quote, and we are inside a string.
            // Wait! To keep it simple: when we scan the string, we find the `{`. We yield the StrPart.
            // In the next call, the first character to scan is `{`.
            // Since we know the `{` is the start of interpolation (because we found it during string scan),
            // we can set a flag `m_expect_interp_open = true` inside the Lexer when `lex_string` hits `{`!
            // Wait! Let's check `lex_string`:
            // ```cpp
            // while (!at_end() && peek() != '"' && peek() != '{') { ... }
            // ```
            // If `peek() == '{'`: we stop scanning the string part, and we do NOT consume `{`.
            // So we can set a member flag `m_expect_interp_open = true;`!
            // Let's make sure this member is defined in `lexer.h`.
            // Wait, let's verify if we need to add it to `lexer.h` first.
            // Yes, we can add `bool m_expect_interp_open = false;` to `lexer.h`.
            // In `lexer.cpp`, if `m_expect_interp_open` is true:
            // We yield `InterpOpen`, set `m_expect_interp_open = false`, and push `m_brace_level` to `m_interp_stack`.
            // This is perfect! Let's check:
            // In `lex_operator`, if we see `{`:
            //   If `m_expect_interp_open` is true:
            //     m_expect_interp_open = false;
            //     m_interp_stack.push_back({ m_brace_level });
            //     return make(TokenKind::InterpOpen, start);
            //   Else:
            //     m_brace_level++;
            //     return make(TokenKind::LBrace, start);
            // This is incredibly elegant and works perfectly!
            // Let's make sure we update `lexer.h` to have `bool m_expect_interp_open = false;`.
            // Wait, let's check:
            // If the string starts, and the first character is `{` (e.g. `"{name}"`):
            // `lex_string` sees `{` at the very beginning of the string.
            // `part_start` is equal to `m_pos` (which is at `{`).
            // The loop `while (!at_end() && peek() != '"' && peek() != '{')` does 0 iterations.
            // It returns a `StrPart` token with length 0.
            // And it sets `m_expect_interp_open = true`.
            // Next time `next_token` is called, it sees `{`, yields `InterpOpen`, and enters normal mode!
            // That is 100% correct and works even for empty string parts!
            // Let's double check if we need to handle `m_expect_interp_open` inside `lex_operator`.
            // Yes, we did:
            // ```cpp
            // case '{': {
            //     if (m_expect_interp_open) {
            //         m_expect_interp_open = false;
            //         m_interp_stack.push_back({ m_brace_level });
            //         return make(TokenKind::InterpOpen, start);
            //     }
            //     m_brace_level++;
            //     return make(TokenKind::LBrace, start);
            // }
            // ```
            // This is absolutely correct!
            
            if (m_expect_interp_open) {
                m_expect_interp_open = false;
                m_interp_stack.push_back({ m_brace_level });
                return make(TokenKind::InterpOpen, start);
            }
            m_brace_level++;
            return make(TokenKind::LBrace, start);
        }
        case '}': {
            if (!m_interp_stack.empty() && m_brace_level == m_interp_stack.back().brace_level_before) {
                m_interp_stack.pop_back();
                m_resume_string = true;
                return make(TokenKind::InterpClose, start);
            }
            m_brace_level--;
            return make(TokenKind::RBrace, start);
        }
    }

    return error_token(start, "Unexpected character");
}

} // namespace syn
