#include "lexer/lexer.h"
#include "lexer/token.h"
#include <cctype>
#include <sstream>

namespace Synapse {

// ── Keyword map ────────────────────────────────────────────────────────────
const std::unordered_map<std::string, TokenType> Lexer::KEYWORDS = {
    {"LET",      TokenType::LET},
    {"FN",       TokenType::FN},
    {"RETURN",   TokenType::RETURN},
    {"IF",       TokenType::IF},
    {"ELSE",     TokenType::ELSE},
    {"REPEAT",   TokenType::REPEAT},
    {"TIMES",    TokenType::TIMES},
    {"LOOP",     TokenType::LOOP},
    {"WHILE",    TokenType::WHILE},
    {"PRINT",    TokenType::PRINT},
    {"PRINTLN",  TokenType::PRINTLN},
    {"ASK",      TokenType::ASK},
    {"INTO",     TokenType::INTO},
    {"AS",       TokenType::AS},
    {"WAIT",     TokenType::WAIT},
    {"TRY",      TokenType::TRY},
    {"CATCH",    TokenType::CATCH},
    {"TRUE",     TokenType::TRUE_LIT},
    {"FALSE",    TokenType::FALSE_LIT},
    {"NULL",     TokenType::NULL_LIT},
    {"AND",      TokenType::AND},
    {"OR",       TokenType::OR},
    {"NOT",      TokenType::NOT},
    {"IN",       TokenType::IN},
    {"IS",       TokenType::IS},
    {"MOUSE",    TokenType::MOUSE},
    {"KEY",      TokenType::KEY},
    {"WINDOW",   TokenType::WINDOW},
    {"APP",      TokenType::APP},
    {"SCREEN",   TokenType::SCREEN},
    {"MOVE",     TokenType::MOVE},
    {"CLICK",    TokenType::CLICK},
    {"DRAG",     TokenType::DRAG},
    {"SCROLL",   TokenType::SCROLL},
    {"HOLD",     TokenType::HOLD},
    {"RELEASE",  TokenType::RELEASE},
    {"PRESS",    TokenType::PRESS},
    {"TYPE",     TokenType::TYPE_KW},
    {"CAPTURE",  TokenType::CAPTURE},
    {"FOCUS",    TokenType::FOCUS},
    {"OPEN",     TokenType::OPEN},
    {"LIST",     TokenType::LIST},
    {"CLOSE",    TokenType::CLOSE},
    {"MINIMIZE", TokenType::MINIMIZE},
    {"MAXIMIZE", TokenType::MAXIMIZE},
    {"RESTORE",  TokenType::RESTORE},
    {"RESIZE",   TokenType::RESIZE},
    {"EXISTS",   TokenType::EXISTS},
    {"INTERVAL", TokenType::INTERVAL},
    {"RUN",      TokenType::RUN},
    {"AT",       TokenType::AT},
    {"FROM",     TokenType::FROM},
    {"TO",       TokenType::TO},
    {"SPEED",    TokenType::SPEED},
    {"LEFT",     TokenType::LEFT},
    {"RIGHT",    TokenType::RIGHT},
    {"MIDDLE",   TokenType::MIDDLE},
    {"UP",       TokenType::UP},
    {"DOWN",     TokenType::DOWN},
    {"BUTTON",   TokenType::BUTTON},
};

// ── Helpers ────────────────────────────────────────────────────────────────
Lexer::Lexer(const std::string& source)
    : source(source), pos(0), line(1), column(1) {
    cur = source.empty() ? '\0' : source[0];
}

void Lexer::advance() {
    if (cur == '\n') { ++line; column = 0; }
    ++pos;
    cur    = (pos < source.size()) ? source[pos] : '\0';
    ++column;
}

char Lexer::peek(int offset) const {
    size_t p = pos + offset;
    return (p < source.size()) ? source[p] : '\0';
}

void Lexer::skipWhitespace() {
    while (cur != '\0' && std::isspace(static_cast<unsigned char>(cur)))
        advance();
}

void Lexer::skipComment() {
    while (cur != '\0' && cur != '\n') advance();
}

// ── Scanners ────────────────────────────────────────────────────────────────

Token Lexer::scanNumber() {
    int    startCol = column;
    int    startLine = line;
    std::string raw;

    while (cur != '\0' && (std::isdigit(static_cast<unsigned char>(cur)) || cur == '.')) {
        raw += cur;
        advance();
    }

    // Check for time suffix: ms, s, m, h  (must be directly adjacent)
    std::string suffix;
    if (cur == 'm' && peek() == 's') {
        suffix = "ms"; advance(); advance();
    } else if (cur == 's') {
        suffix = "s"; advance();
    } else if (cur == 'm') {
        suffix = "m"; advance();
    } else if (cur == 'h') {
        suffix = "h"; advance();
    }

    if (!suffix.empty())
        return Token(TokenType::TIME_LIT, raw + suffix, startLine, startCol);

    return Token(TokenType::NUMBER, raw, startLine, startCol);
}

Token Lexer::scanString() {
    int  startCol  = column;
    int  startLine = line;
    char quote     = cur;
    advance();  // skip opening quote

    std::string result;
    while (cur != '\0' && cur != quote) {
        if (cur == '\\') {
            advance();
            switch (cur) {
                case 'n':  result += '\n'; break;
                case 't':  result += '\t'; break;
                case '\\': result += '\\'; break;
                case '"':  result += '"';  break;
                case '\'': result += '\''; break;
                default:   result += '\\'; result += cur; break;
            }
        } else {
            result += cur;
        }
        advance();
    }
    if (cur == quote) advance();
    else throw LexerError("Unterminated string literal", startLine, startCol);

    return Token(TokenType::STRING, result, startLine, startCol);
}

Token Lexer::scanIdentifierOrKeyword() {
    int startCol  = column;
    int startLine = line;
    std::string word;

    while (cur != '\0' && (std::isalnum(static_cast<unsigned char>(cur)) || cur == '_')) {
        word += cur;
        advance();
    }

    // Case-insensitive keyword check
    std::string upperWord = word;
    for (auto& c : upperWord) c = std::toupper(static_cast<unsigned char>(c));

    auto it = KEYWORDS.find(upperWord);
    TokenType tt = (it != KEYWORDS.end()) ? it->second : TokenType::IDENTIFIER;
    return Token(tt, word, startLine, startCol);
}

Token Lexer::scanOperatorOrPunct() {
    int startCol  = column;
    int startLine = line;
    char c = cur;
    advance();

    // Two/three-character operators first
    switch (c) {
        case '+':
            if (cur == '=') { advance(); return {TokenType::PLUS_EQ,     "+=",  startLine, startCol}; }
            return {TokenType::PLUS,     "+",  startLine, startCol};
        case '-':
            if (cur == '=') { advance(); return {TokenType::MINUS_EQ,    "-=",  startLine, startCol}; }
            return {TokenType::MINUS,    "-",  startLine, startCol};
        case '*':
            if (cur == '*') {
                advance();
                if (cur == '=') { advance(); return {TokenType::POWER_EQ, "**=", startLine, startCol}; }
                return {TokenType::POWER, "**", startLine, startCol};
            }
            if (cur == '=') { advance(); return {TokenType::STAR_EQ,  "*=",  startLine, startCol}; }
            return {TokenType::STAR,     "*",  startLine, startCol};
        case '/':
            if (cur == '/') {
                advance();
                if (cur == '=') { advance(); return {TokenType::FLOORDIV_EQ, "//=", startLine, startCol}; }
                return {TokenType::FLOORDIV, "//", startLine, startCol};
            }
            if (cur == '=') { advance(); return {TokenType::SLASH_EQ, "/=", startLine, startCol}; }
            return {TokenType::SLASH,    "/",  startLine, startCol};
        case '%':
            if (cur == '=') { advance(); return {TokenType::PERCENT_EQ, "%=", startLine, startCol}; }
            return {TokenType::PERCENT,  "%",  startLine, startCol};
        case '=':
            if (cur == '=') {
                advance();
                if (cur == '=') { advance(); return {TokenType::STRICT_EQ, "===", startLine, startCol}; }
                return {TokenType::EQEQ, "==", startLine, startCol};
            }
            return {TokenType::EQUALS,   "=",  startLine, startCol};
        case '!':
            if (cur == '=') { advance(); return {TokenType::NEQ,  "!=", startLine, startCol}; }
            break;
        case '<':
            if (cur == '<') {
                advance();
                if (cur == '=') { advance(); return {TokenType::LSHIFT_EQ, "<<=", startLine, startCol}; }
                return {TokenType::LSHIFT, "<<", startLine, startCol};
            }
            if (cur == '=') { advance(); return {TokenType::LEQ, "<=", startLine, startCol}; }
            return {TokenType::LT,       "<",  startLine, startCol};
        case '>':
            if (cur == '>') {
                advance();
                if (cur == '=') { advance(); return {TokenType::RSHIFT_EQ, ">>=", startLine, startCol}; }
                return {TokenType::RSHIFT, ">>", startLine, startCol};
            }
            if (cur == '=') { advance(); return {TokenType::GEQ, ">=", startLine, startCol}; }
            return {TokenType::GT,       ">",  startLine, startCol};
        case '&':
            if (cur == '=') { advance(); return {TokenType::AMP_EQ,   "&=", startLine, startCol}; }
            return {TokenType::AMP,      "&",  startLine, startCol};
        case '|':
            if (cur == '=') { advance(); return {TokenType::PIPE_EQ,  "|=", startLine, startCol}; }
            return {TokenType::PIPE,     "|",  startLine, startCol};
        case '^':
            if (cur == '=') { advance(); return {TokenType::CARET_EQ, "^=", startLine, startCol}; }
            return {TokenType::CARET,    "^",  startLine, startCol};
        case '~': return {TokenType::TILDE,    "~",  startLine, startCol};
        case '(': return {TokenType::LPAREN,   "(",  startLine, startCol};
        case ')': return {TokenType::RPAREN,   ")",  startLine, startCol};
        case '{': return {TokenType::LBRACE,   "{",  startLine, startCol};
        case '}': return {TokenType::RBRACE,   "}",  startLine, startCol};
        case '[': return {TokenType::LBRACKET, "[",  startLine, startCol};
        case ']': return {TokenType::RBRACKET, "]",  startLine, startCol};
        case ';': return {TokenType::SEMICOLON,";",  startLine, startCol};
        case ',': return {TokenType::COMMA,    ",",  startLine, startCol};
        case ':':
            if (cur == '=') { advance(); return {TokenType::WALRUS, ":=", startLine, startCol}; }
            return {TokenType::COLON,    ":",  startLine, startCol};
        case '.': return {TokenType::DOT,      ".",  startLine, startCol};
        default: break;
    }

    throw LexerError(
        std::string("Unexpected character '") + c + "'",
        startLine, startCol
    );
}

// ── Main tokenize loop ──────────────────────────────────────────────────────
std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skipWhitespace();

        if (cur == '\0') {
            tokens.emplace_back(TokenType::END_OF_FILE, "", line, column);
            break;
        }

        if (cur == '#') { skipComment(); continue; }

        if (std::isalpha(static_cast<unsigned char>(cur)) || cur == '_')
            tokens.push_back(scanIdentifierOrKeyword());
        else if (std::isdigit(static_cast<unsigned char>(cur)))
            tokens.push_back(scanNumber());
        else if (cur == '"' || cur == '\'')
            tokens.push_back(scanString());
        else
            tokens.push_back(scanOperatorOrPunct());
    }

    return tokens;
}

// ── Token name helper ───────────────────────────────────────────────────────
const char* tokenTypeName(TokenType t) {
    switch (t) {
#define CASE(x) case TokenType::x: return #x
        CASE(IDENTIFIER); CASE(NUMBER); CASE(STRING); CASE(TIME_LIT);
        CASE(TRUE_LIT); CASE(FALSE_LIT); CASE(NULL_LIT);
        CASE(LET); CASE(FN); CASE(RETURN); CASE(IF); CASE(ELSE);
        CASE(REPEAT); CASE(TIMES); CASE(LOOP); CASE(WHILE);
        CASE(PRINT); CASE(PRINTLN); CASE(ASK); CASE(INTO); CASE(AS);
        CASE(WAIT); CASE(TRY); CASE(CATCH);
        CASE(AND); CASE(OR); CASE(NOT); CASE(IN); CASE(IS);
        CASE(MOUSE); CASE(KEY); CASE(WINDOW); CASE(APP); CASE(SCREEN);
        CASE(PLUS); CASE(MINUS); CASE(STAR); CASE(SLASH); CASE(PERCENT);
        CASE(POWER); CASE(FLOORDIV); CASE(EQEQ); CASE(NEQ);
        CASE(LT); CASE(GT); CASE(LEQ); CASE(GEQ); CASE(STRICT_EQ);
        CASE(EQUALS); CASE(LPAREN); CASE(RPAREN); CASE(LBRACE); CASE(RBRACE);
        CASE(LBRACKET); CASE(RBRACKET);
        CASE(SEMICOLON); CASE(COMMA); CASE(COLON); CASE(DOT);
        CASE(END_OF_FILE); CASE(UNKNOWN);
#undef CASE
        default: return "?";
    }
}

} // namespace Synapse
