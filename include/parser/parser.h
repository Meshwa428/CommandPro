#pragma once
#include "lexer/token.h"
#include "parser/ast.h"
#include <vector>
#include <memory>
#include <stdexcept>

namespace Synapse {

class ParseError : public std::runtime_error {
public:
    int line, column;
    ParseError(const std::string& msg, int ln, int col)
        : std::runtime_error(msg), line(ln), column(col) {}
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    std::unique_ptr<ProgramNode> parse();

private:
    // ── Token navigation ─────────────────────────────────────────────────
    const Token& cur()  const;
    const Token& prev() const;
    const Token& peek(int offset = 1) const;
    Token        eat(TokenType expected);
    bool         check(TokenType t) const;
    bool         match(TokenType t);

    // ── Top-level ─────────────────────────────────────────────────────────
    NodePtr parseStatement();
    NodePtr parseBlock();

    // ── Declarations ──────────────────────────────────────────────────────
    NodePtr parseVarDecl();
    NodePtr parseFuncDecl();

    // ── Control flow ──────────────────────────────────────────────────────
    NodePtr parseIf();
    NodePtr parseRepeat();
    NodePtr parseWhile();
    NodePtr parseTryCatch();

    // ── Simple statements ─────────────────────────────────────────────────
    NodePtr parsePrint(bool newline);
    NodePtr parseAsk();
    NodePtr parseWait();
    NodePtr parseReturn();

    // ── Automation commands (Phase 2) ──────────────────────────────────────
    NodePtr parseMouseCommand();
    NodePtr parseKeyCommand();
    NodePtr parseAppCommand();

    // ── Expressions (operator precedence climbing) ─────────────────────────
    NodePtr parseExpression();
    NodePtr parseAssignment();
    NodePtr parseOr();
    NodePtr parseAnd();
    NodePtr parseNot();
    NodePtr parseComparison();
    NodePtr parseBitOr();
    NodePtr parseBitXor();
    NodePtr parseBitAnd();
    NodePtr parseShift();
    NodePtr parseAddSub();
    NodePtr parseMulDiv();
    NodePtr parsePower();
    NodePtr parseUnary();
    NodePtr parsePostfix();
    NodePtr parsePrimary();
    NodePtr parseListLiteral();
    NodePtr parseMapLiteral();

    // ── Helpers ───────────────────────────────────────────────────────────
    NodePtr parseTimeLiteral(const Token& tok);
    NodePtr parsePoint();          // (x, y)
    NodeList parseArgList();       // comma-separated expressions

    // ── State ─────────────────────────────────────────────────────────────
    std::vector<Token> tokens;
    size_t             idx;
    ParseError error(const Token& tok, const std::string& msg);
};

} // namespace Synapse
