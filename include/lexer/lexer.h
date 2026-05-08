#pragma once
#include "token.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

namespace Synapse {

class LexerError : public std::runtime_error {
public:
    int line, column;
    LexerError(const std::string& msg, int ln, int col)
        : std::runtime_error(msg), line(ln), column(col) {}
};

class Lexer {
public:
    explicit Lexer(const std::string& source);
    std::vector<Token> tokenize();

private:
    void        advance();
    char        peek(int offset = 1) const;
    void        skipWhitespace();
    void        skipComment();
    Token       scanNumber();
    Token       scanString();
    Token       scanIdentifierOrKeyword();
    Token       scanOperatorOrPunct();

    std::string         source;
    size_t              pos;
    int                 line;
    int                 column;
    char                cur;

    static const std::unordered_map<std::string, TokenType> KEYWORDS;
};

} // namespace Synapse
