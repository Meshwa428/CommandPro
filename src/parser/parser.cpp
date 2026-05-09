#include "parser/parser.h"
#include <sstream>

namespace Synapse {

// ──────────────────────────────────────────────────────────────────────────
//  Construction
// ──────────────────────────────────────────────────────────────────────────
Parser::Parser(std::vector<Token> toks)
    : tokens(std::move(toks)), idx(0) {}

// ──────────────────────────────────────────────────────────────────────────
//  Token helpers
// ──────────────────────────────────────────────────────────────────────────
const Token& Parser::cur() const {
    return tokens[idx < tokens.size() ? idx : tokens.size() - 1];
}
const Token& Parser::peek(int offset) const {
    size_t i = idx + offset;
    return tokens[i < tokens.size() ? i : tokens.size() - 1];
}
bool Parser::check(TokenType t) const { return cur().type == t; }
bool Parser::match(TokenType t) {
    if (check(t)) { ++idx; return true; }
    return false;
}
Token Parser::eat(TokenType expected) {
    if (!check(expected)) {
        std::ostringstream msg;
        msg << "Expected " << tokenTypeName(expected)
            << " but got '" << cur().value << "' ("
            << tokenTypeName(cur().type) << ")";
        throw ParseError(msg.str(), cur().line, cur().column);
    }
    return tokens[idx++];
}

const Token& Parser::prev() const {
    return tokens[idx > 0 ? idx - 1 : 0];
}

ParseError Parser::error(const Token& tok, const std::string& msg) {
    return ParseError(msg, tok.line, tok.column);
}

// ──────────────────────────────────────────────────────────────────────────
//  Parse entry
// ──────────────────────────────────────────────────────────────────────────
std::unique_ptr<ProgramNode> Parser::parse() {
    auto prog = std::make_unique<ProgramNode>();
    while (!check(TokenType::END_OF_FILE))
        prog->statements.push_back(parseStatement());
    return prog;
}

// ──────────────────────────────────────────────────────────────────────────
//  Block  { stmt* }
// ──────────────────────────────────────────────────────────────────────────
NodePtr Parser::parseBlock() {
    eat(TokenType::LBRACE);
    auto node = std::make_unique<BlockNode>();
    node->needsScope = false; // Assume no decls initially
    while (!check(TokenType::RBRACE) && !check(TokenType::END_OF_FILE)) {
        TokenType t = cur().type;
        if (t == TokenType::LET || t == TokenType::FN || 
            t == TokenType::INT_TYPE || t == TokenType::FLOAT_TYPE ||
            t == TokenType::STR_TYPE || t == TokenType::BOOL_TYPE ||
            t == TokenType::TUPLE_TYPE || t == TokenType::LIST_TYPE ||
            t == TokenType::MAP_TYPE || t == TokenType::TIME_TYPE) {
            node->needsScope = true;
        }
        node->statements.push_back(parseStatement());
    }
    eat(TokenType::RBRACE);
    return node;
}

// ──────────────────────────────────────────────────────────────────────────
//  Statement dispatcher
// ──────────────────────────────────────────────────────────────────────────
NodePtr Parser::parseStatement() {
    switch (cur().type) {
        case TokenType::LET:        return parseVarDecl();
        // Typed declarations: int x = 1; float y = 3.14;
        case TokenType::INT_TYPE:   return parseTypedVarDecl("int");
        case TokenType::FLOAT_TYPE: return parseTypedVarDecl("float");
        case TokenType::STR_TYPE:   return parseTypedVarDecl("str");
        case TokenType::BOOL_TYPE:  return parseTypedVarDecl("bool");
        case TokenType::TUPLE_TYPE: return parseTypedVarDecl("tuple");
        case TokenType::LIST_TYPE:  return parseTypedVarDecl("list");
        case TokenType::MAP_TYPE:   return parseTypedVarDecl("map");
        case TokenType::TIME_TYPE:  return parseTypedVarDecl("time");
        case TokenType::FN:         return parseFuncDecl();
        case TokenType::IF:         return parseIf();
        case TokenType::REPEAT:     return parseRepeat();
        case TokenType::LOOP:       return parseWhile();
        case TokenType::TRY:        return parseTryCatch();
        case TokenType::PRINT:      return parsePrint(false);
        case TokenType::PRINTLN:    return parsePrint(true);
        case TokenType::ASK:        return parseAsk();
        case TokenType::WAIT:       return parseWait();
        case TokenType::RETURN:     return parseReturn();
        case TokenType::MOUSE:      return parseMouseCommand();
        case TokenType::KEY:        return parseKeyCommand();
        case TokenType::APP:        return parseAppCommand();
        case TokenType::LBRACE:     return parseBlock();
        default: {
            int ln = cur().line, col = cur().column;
            NodePtr expr = parseExpression();
            if (check(TokenType::SEMICOLON)) eat(TokenType::SEMICOLON);
            auto node = std::make_unique<ExpressionStmtNode>(std::move(expr));
            node->line = ln; node->column = col;
            return node;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────
//  Variable declaration:  let name = expr ;
// ──────────────────────────────────────────────────────────────────────────
NodePtr Parser::parseVarDecl() {
    int ln = cur().line, col = cur().column;
    eat(TokenType::LET);
    std::string name = eat(TokenType::IDENTIFIER).value;
    eat(TokenType::EQUALS);
    NodePtr val = parseExpression();
    eat(TokenType::SEMICOLON);
    auto node = std::make_unique<VarDeclNode>(name, std::move(val));
    node->line = ln; node->column = col;
    return node;
}

// ──────────────────────────────────────────────────────────────────────────
//  Typed Variable declaration:  int name = expr ;
// ──────────────────────────────────────────────────────────────────────────
NodePtr Parser::parseTypedVarDecl(const std::string& typeName) {
    int ln = cur().line, col = cur().column;
    ++idx; // Eat the type keyword (e.g. INT_TYPE)
    std::string name = eat(TokenType::IDENTIFIER).value;
    eat(TokenType::EQUALS);
    NodePtr val = parseExpression();
    eat(TokenType::SEMICOLON);
    auto node = std::make_unique<TypedVarDeclNode>(name, typeName, std::move(val));
    node->line = ln; node->column = col;
    return node;
}

// ──────────────────────────────────────────────────────────────────────────
//  Function declaration:  fn name(p1, p2) { block }
// ──────────────────────────────────────────────────────────────────────────
NodePtr Parser::parseFuncDecl() {
    int ln = cur().line, col = cur().column;
    eat(TokenType::FN);
    std::string name = eat(TokenType::IDENTIFIER).value;
    eat(TokenType::LPAREN);
    std::vector<std::string> params;
    while (!check(TokenType::RPAREN)) {
        params.push_back(eat(TokenType::IDENTIFIER).value);
        if (!check(TokenType::RPAREN)) eat(TokenType::COMMA);
    }
    eat(TokenType::RPAREN);
    NodePtr body = parseBlock();
    auto node = std::make_unique<FuncDeclNode>(name, std::move(params), std::move(body));
    node->line = ln; node->column = col;
    return node;
}

// ──────────────────────────────────────────────────────────────────────────
//  If:  if (cond) { } [else { }]
// ──────────────────────────────────────────────────────────────────────────
NodePtr Parser::parseIf() {
    int ln = cur().line, col = cur().column;
    eat(TokenType::IF);
    eat(TokenType::LPAREN);
    NodePtr cond = parseExpression();
    eat(TokenType::RPAREN);
    NodePtr thenBlk = parseBlock();
    NodePtr elseBlk = nullptr;
    if (check(TokenType::ELSE)) {
        eat(TokenType::ELSE);
        elseBlk = parseBlock();
    }
    auto node = std::make_unique<IfNode>(std::move(cond), std::move(thenBlk), std::move(elseBlk));
    node->line = ln; node->column = col;
    return node;
}

// ──────────────────────────────────────────────────────────────────────────
//  Repeat:  repeat N times { }
// ──────────────────────────────────────────────────────────────────────────
NodePtr Parser::parseRepeat() {
    int ln = cur().line, col = cur().column;
    eat(TokenType::REPEAT);
    NodePtr count = parseExpression();
    eat(TokenType::TIMES);
    NodePtr body = parseBlock();
    auto node = std::make_unique<RepeatNode>(std::move(count), std::move(body));
    node->line = ln; node->column = col;
    return node;
}

// ──────────────────────────────────────────────────────────────────────────
//  While:  loop while (cond) { }
// ──────────────────────────────────────────────────────────────────────────
NodePtr Parser::parseWhile() {
    int ln = cur().line, col = cur().column;
    eat(TokenType::LOOP);
    eat(TokenType::WHILE);
    eat(TokenType::LPAREN);
    NodePtr cond = parseExpression();
    eat(TokenType::RPAREN);
    NodePtr body = parseBlock();
    auto node = std::make_unique<WhileNode>(std::move(cond), std::move(body));
    node->line = ln; node->column = col;

    // Analyze for simple numeric loop optimization: loop while (i < N)
    if (auto* bExpr = dynamic_cast<BinaryExprNode*>(node->condition.get())) {
        if (auto* ident = dynamic_cast<IdentifierNode*>(bExpr->left.get())) {
            if (auto* lit = dynamic_cast<IntLiteralNode*>(bExpr->right.get())) {
                if (bExpr->op == "<") {
                    node->isSimpleNumericLoop = true;
                    node->counterVar = ident->name;
                    node->limit = lit->value;
                    node->op = bExpr->op;
                }
            }
        }
    }

    return node;
}

// ──────────────────────────────────────────────────────────────────────────
//  Try/Catch
// ──────────────────────────────────────────────────────────────────────────
NodePtr Parser::parseTryCatch() {
    int ln = cur().line, col = cur().column;
    eat(TokenType::TRY);
    NodePtr tryBlk = parseBlock();
    eat(TokenType::CATCH);
    eat(TokenType::LPAREN);
    std::string errVar = eat(TokenType::IDENTIFIER).value;
    eat(TokenType::RPAREN);
    NodePtr catchBlk = parseBlock();
    auto node = std::make_unique<TryCatchNode>(std::move(tryBlk), errVar, std::move(catchBlk));
    node->line = ln; node->column = col;
    return node;
}

// ──────────────────────────────────────────────────────────────────────────
//  Print / Println:  print expr;   println expr;
// ──────────────────────────────────────────────────────────────────────────
NodePtr Parser::parsePrint(bool newline) {
    int ln = cur().line, col = cur().column;
    ++idx; // eat PRINT or PRINTLN
    NodePtr val = parseExpression();
    eat(TokenType::SEMICOLON);
    auto node = std::make_unique<PrintNode>(std::move(val), newline);
    node->line = ln; node->column = col;
    return node;
}

// ──────────────────────────────────────────────────────────────────────────
//  Ask:  ASK "prompt" INTO varName [AS TYPE];
// ──────────────────────────────────────────────────────────────────────────
NodePtr Parser::parseAsk() {
    int ln = cur().line, col = cur().column;
    eat(TokenType::ASK);
    std::string prompt = eat(TokenType::STRING).value;
    eat(TokenType::INTO);
    std::string varName = eat(TokenType::IDENTIFIER).value;
    std::string typeCast;
    if (check(TokenType::AS)) {
        eat(TokenType::AS);
        typeCast = eat(TokenType::IDENTIFIER).value;
    }
    eat(TokenType::SEMICOLON);
    auto node = std::make_unique<AskNode>(prompt, varName, typeCast);
    node->line = ln; node->column = col;
    return node;
}

// ──────────────────────────────────────────────────────────────────────────
//  Wait:  WAIT duration;
// ──────────────────────────────────────────────────────────────────────────
NodePtr Parser::parseWait() {
    int ln = cur().line, col = cur().column;
    eat(TokenType::WAIT);
    NodePtr dur = parseExpression();
    eat(TokenType::SEMICOLON);
    auto node = std::make_unique<WaitNode>(std::move(dur));
    node->line = ln; node->column = col;
    return node;
}

// ──────────────────────────────────────────────────────────────────────────
//  Return:  return [expr];
// ──────────────────────────────────────────────────────────────────────────
NodePtr Parser::parseReturn() {
    int ln = cur().line, col = cur().column;
    eat(TokenType::RETURN);
    NodePtr val = nullptr;
    if (!check(TokenType::SEMICOLON)) val = parseExpression();
    eat(TokenType::SEMICOLON);
    auto node = std::make_unique<ReturnNode>(std::move(val));
    node->line = ln; node->column = col;
    return node;
}

// ──────────────────────────────────────────────────────────────────────────
//  Expression hierarchy (operator precedence)
// ──────────────────────────────────────────────────────────────────────────
NodePtr Parser::parseExpression() { return parseAssignment(); }

NodePtr Parser::parseAssignment() {
    NodePtr left = parseOr();

    if (match(TokenType::EQUALS)) {
        int ln = prev().line, col = prev().column;
        NodePtr value = parseAssignment();
        
        if (auto* id = dynamic_cast<IdentifierNode*>(left.get())) {
            auto node = std::make_unique<AssignNode>(id->name, std::move(value));
            node->line = ln; node->column = col;
            return node;
        } else if (auto* idx = dynamic_cast<IndexAccessNode*>(left.get())) {
            auto node = std::make_unique<IndexSetNode>(std::move(idx->object), std::move(idx->index), std::move(value));
            node->line = ln; node->column = col;
            return node;
        }
        throw error(prev(), "Invalid assignment target");
    }

    // Compound assignment
    static const std::pair<TokenType,std::string> compounds[] = {
        {TokenType::PLUS_EQ,     "+"}, {TokenType::MINUS_EQ,    "-"},
        {TokenType::STAR_EQ,     "*"}, {TokenType::SLASH_EQ,    "/"},
        {TokenType::PERCENT_EQ,  "%"}, {TokenType::FLOORDIV_EQ,"//"},
        {TokenType::POWER_EQ,   "**"}, {TokenType::AMP_EQ,      "&"},
        {TokenType::PIPE_EQ,    "|"}, {TokenType::CARET_EQ,    "^"},
        {TokenType::LSHIFT_EQ, "<<"}, {TokenType::RSHIFT_EQ,  ">>"},
    };

    for (auto& [tt, op] : compounds) {
        if (match(tt)) {
            int ln = prev().line, col = prev().column;
            NodePtr value = parseAssignment();
            if (auto* id = dynamic_cast<IdentifierNode*>(left.get())) {
                auto node = std::make_unique<CompoundAssignNode>(id->name, op, std::move(value));
                node->line = ln; node->column = col;
                return node;
            }
            throw error(prev(), "Invalid compound assignment target");
        }
    }

    return left;
}

NodePtr Parser::parseOr() {
    NodePtr left = parseAnd();
    while (check(TokenType::OR)) {
        std::string op = cur().value; ++idx;
        left = std::make_unique<BinaryExprNode>(op, std::move(left), parseAnd());
    }
    return left;
}

NodePtr Parser::parseAnd() {
    NodePtr left = parseNot();
    while (check(TokenType::AND)) {
        std::string op = cur().value; ++idx;
        left = std::make_unique<BinaryExprNode>(op, std::move(left), parseNot());
    }
    return left;
}

NodePtr Parser::parseNot() {
    if (check(TokenType::NOT)) {
        int ln = cur().line, col = cur().column;
        ++idx;
        auto node = std::make_unique<UnaryExprNode>("NOT", parseNot());
        node->line = ln; node->column = col;
        return node;
    }
    return parseComparison();
}

NodePtr Parser::parseComparison() {
    NodePtr left = parseBitOr();
    static const TokenType ops[] = {
        TokenType::EQEQ,   TokenType::NEQ,  TokenType::LT,
        TokenType::GT,     TokenType::LEQ,  TokenType::GEQ,
        TokenType::STRICT_EQ, TokenType::IS, TokenType::IN,
    };
    for (auto tt : ops) {
        if (check(tt)) {
            std::string op = cur().value; ++idx;
            left = std::make_unique<BinaryExprNode>(op, std::move(left), parseBitOr());
            break;
        }
    }
    // Handle NOT IN / IS NOT
    if (check(TokenType::NOT)) {
        if (peek().type == TokenType::IN || peek().type == TokenType::IS) {
            std::string op = "NOT " + tokens[idx+1].value;
            idx += 2;
            left = std::make_unique<BinaryExprNode>(op, std::move(left), parseBitOr());
        }
    }
    return left;
}

NodePtr Parser::parseBitOr() {
    NodePtr left = parseBitXor();
    while (check(TokenType::PIPE)) {
        ++idx;
        left = std::make_unique<BinaryExprNode>("|", std::move(left), parseBitXor());
    }
    return left;
}

NodePtr Parser::parseBitXor() {
    NodePtr left = parseBitAnd();
    while (check(TokenType::CARET)) {
        ++idx;
        left = std::make_unique<BinaryExprNode>("^", std::move(left), parseBitAnd());
    }
    return left;
}

NodePtr Parser::parseBitAnd() {
    NodePtr left = parseShift();
    while (check(TokenType::AMP)) {
        ++idx;
        left = std::make_unique<BinaryExprNode>("&", std::move(left), parseShift());
    }
    return left;
}

NodePtr Parser::parseShift() {
    NodePtr left = parseAddSub();
    while (check(TokenType::LSHIFT) || check(TokenType::RSHIFT)) {
        std::string op = cur().value; ++idx;
        left = std::make_unique<BinaryExprNode>(op, std::move(left), parseAddSub());
    }
    return left;
}

NodePtr Parser::parseAddSub() {
    NodePtr left = parseMulDiv();
    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        std::string op = cur().value; ++idx;
        left = std::make_unique<BinaryExprNode>(op, std::move(left), parseMulDiv());
    }
    return left;
}

NodePtr Parser::parseMulDiv() {
    NodePtr left = parsePower();
    while (check(TokenType::STAR) || check(TokenType::SLASH) ||
           check(TokenType::PERCENT) || check(TokenType::FLOORDIV)) {
        std::string op = cur().value; ++idx;
        left = std::make_unique<BinaryExprNode>(op, std::move(left), parsePower());
    }
    return left;
}

NodePtr Parser::parsePower() {
    NodePtr left = parseUnary();
    if (check(TokenType::POWER)) {
        ++idx;
        // right-associative
        NodePtr right = parsePower();
        return std::make_unique<BinaryExprNode>("**", std::move(left), std::move(right));
    }
    return left;
}

NodePtr Parser::parseUnary() {
    if (match(TokenType::NOT) || match(TokenType::MINUS) || match(TokenType::TILDE)) {
        std::string op = tokens[idx-1].value;
        return std::make_unique<UnaryExprNode>(op, parseUnary());
    }
    return parsePostfix();
}

NodePtr Parser::parsePostfix() {
    NodePtr node = parsePrimary();
    while (true) {
        if (check(TokenType::LBRACKET)) {
            int ln = cur().line, col = cur().column;
            eat(TokenType::LBRACKET);
            NodePtr index = parseExpression();
            eat(TokenType::RBRACKET);
            auto next = std::make_unique<IndexAccessNode>(std::move(node), std::move(index));
            next->line = ln; next->column = col;
            node = std::move(next);
        } else {
            break;
        }
    }
    return node;
}

NodePtr Parser::parsePrimary() {
    const Token& tok = cur();

    // Grouped expression OR Tuple literal
    // Disambiguation rules (matching Python semantics):
    //   ()          → empty Tuple
    //   (expr,)     → single-element Tuple
    //   (a, b, ...) → multi-element Tuple
    //   (expr)      → grouping (returns expr, not a Tuple)
    if (tok.type == TokenType::LPAREN) {
        int ln = tok.line, col = tok.column;
        ++idx; // eat (

        // Empty tuple: ()
        if (check(TokenType::RPAREN)) {
            ++idx; // eat )
            auto node = std::make_unique<TupleLiteralNode>(std::vector<NodePtr>{});
            node->line = ln; node->column = col;
            return node;
        }

        NodePtr first = parseExpression();

        if (check(TokenType::RPAREN)) {
            // Plain grouping: (expr)
            ++idx; // eat )
            return first;
        }

        // Has a comma — it's a Tuple
        eat(TokenType::COMMA);
        std::vector<NodePtr> elements;
        elements.push_back(std::move(first));

        // Trailing comma after first element: (val,) → single-element tuple
        if (check(TokenType::RPAREN)) {
            ++idx; // eat )
            auto node = std::make_unique<TupleLiteralNode>(std::move(elements));
            node->line = ln; node->column = col;
            return node;
        }

        // Remaining elements
        while (!check(TokenType::RPAREN) && !check(TokenType::END_OF_FILE)) {
            elements.push_back(parseExpression());
            if (!check(TokenType::RPAREN)) eat(TokenType::COMMA);
        }
        eat(TokenType::RPAREN);
        auto node = std::make_unique<TupleLiteralNode>(std::move(elements));
        node->line = ln; node->column = col;
        return node;
    }

    // Literals
    if (tok.type == TokenType::NUMBER) {
        ++idx;
        std::string v = tok.value;
        try {
            if (v.find('.') != std::string::npos)
                return std::make_unique<FloatLiteralNode>(std::stod(v));
            return std::make_unique<IntLiteralNode>(std::stoll(v));
        } catch (...) {
            error(tok, "Numeric literal overflow: " + v);
        }
    }
    if (tok.type == TokenType::STRING) {
        ++idx;
        return std::make_unique<StringLiteralNode>(tok.value);
    }
    if (tok.type == TokenType::TIME_LIT) {
        ++idx;
        return parseTimeLiteral(tok);
    }
    if (tok.type == TokenType::TRUE_LIT)  { ++idx; return std::make_unique<BoolLiteralNode>(true); }
    if (tok.type == TokenType::FALSE_LIT) { ++idx; return std::make_unique<BoolLiteralNode>(false); }
    if (tok.type == TokenType::NULL_LIT)  { ++idx; return std::make_unique<NullLiteralNode>(); }

    if (tok.type == TokenType::LBRACKET) return parseListLiteral();
    if (tok.type == TokenType::LBRACE)   return parseMapLiteral();

    if (tok.type == TokenType::APP) {
        size_t saved = idx;
        eat(TokenType::APP);
        if (match(TokenType::LIST_TYPE)) {
            auto node = std::make_unique<AppListNode>();
            node->line = tok.line; node->column = tok.column;
            return node;
        }
        idx = saved; // Rollback if not APP LIST, let parseStatement handle it
    }

    // Identifier or function call — only parses value, never eats trailing ;
    if (tok.type == TokenType::IDENTIFIER) {
        ++idx;
        if (check(TokenType::LPAREN)) {
            ++idx; // eat (
            std::vector<NodePtr> args = parseArgList();
            eat(TokenType::RPAREN);
            auto node = std::make_unique<FuncCallNode>(tok.value, std::move(args));
            node->line = tok.line; node->column = tok.column;
            return node;
        }
        auto node = std::make_unique<IdentifierNode>(tok.value);
        node->line = tok.line; node->column = tok.column;
        return node;
    }

    throw ParseError(
        std::string("Unexpected token '") + tok.value + "' in expression",
        tok.line, tok.column
    );
}

// ──────────────────────────────────────────────────────────────────────────
//  Collections
// ──────────────────────────────────────────────────────────────────────────
NodePtr Parser::parseListLiteral() {
    int ln = cur().line, col = cur().column;
    eat(TokenType::LBRACKET);
    std::vector<NodePtr> elements;
    while (!check(TokenType::RBRACKET) && !check(TokenType::END_OF_FILE)) {
        elements.push_back(parseExpression());
        if (!check(TokenType::RBRACKET)) eat(TokenType::COMMA);
    }
    eat(TokenType::RBRACKET);
    auto node = std::make_unique<ListLiteralNode>(std::move(elements));
    node->line = ln; node->column = col;
    return node;
}

NodePtr Parser::parseMapLiteral() {
    int ln = cur().line, col = cur().column;
    eat(TokenType::LBRACE);
    std::vector<std::pair<NodePtr, NodePtr>> items;
    while (!check(TokenType::RBRACE) && !check(TokenType::END_OF_FILE)) {
        NodePtr key = parseExpression();
        eat(TokenType::COLON);
        NodePtr val = parseExpression();
        items.push_back({std::move(key), std::move(val)});
        if (!check(TokenType::RBRACE)) eat(TokenType::COMMA);
    }
    eat(TokenType::RBRACE);
    auto node = std::make_unique<MapLiteralNode>(std::move(items));
    node->line = ln; node->column = col;
    return node;
}

// ──────────────────────────────────────────────────────────────────────────
//  Helpers
// ──────────────────────────────────────────────────────────────────────────
NodePtr Parser::parseTimeLiteral(const Token& tok) {
    std::string raw = tok.value;
    double amount = 0.0;
    std::string unit = "ms";

    if (raw.size() >= 2 && raw.substr(raw.size()-2) == "ms") {
        amount = std::stod(raw.substr(0, raw.size()-2));
        unit   = "ms";
    } else if (raw.back() == 's') {
        amount = std::stod(raw.substr(0, raw.size()-1));
        unit   = "s";
    } else if (raw.back() == 'm') {
        amount = std::stod(raw.substr(0, raw.size()-1));
        unit   = "m";
    } else if (raw.back() == 'h') {
        amount = std::stod(raw.substr(0, raw.size()-1));
        unit   = "h";
    }

    auto node = std::make_unique<TimeLiteralNode>(amount, unit);
    node->line = tok.line; node->column = tok.column;
    return node;
}

std::vector<NodePtr> Parser::parseArgList() {
    std::vector<NodePtr> args;
    while (!check(TokenType::RPAREN) && !check(TokenType::END_OF_FILE)) {
        args.push_back(parseExpression());
        if (!check(TokenType::RPAREN)) eat(TokenType::COMMA);
    }
    return args;
}

// ──────────────────────────────────────────────────────────────────────────
//  Automation: MOUSE commands
//  MOUSE MOVE TO (x, y);
//  MOUSE CLICK LEFT [AT (x, y)];
//  MOUSE CLICK RIGHT [AT (x, y)];
//  MOUSE CLICK MIDDLE [AT (x, y)];
// ──────────────────────────────────────────────────────────────────────────
NodePtr Parser::parseMouseCommand() {
    int ln = cur().line, col = cur().column;
    eat(TokenType::MOUSE);

    if (match(TokenType::MOVE)) {
        if (check(TokenType::TO)) eat(TokenType::TO);
        NodePtr point = parseExpression();
        eat(TokenType::SEMICOLON);
        auto node = std::make_unique<MouseMoveNode>(std::move(point));
        node->line = ln; node->column = col;
        return node;
    }

    if (match(TokenType::CLICK)) {
        MouseButton btn = MouseButton::LEFT;
        if      (match(TokenType::LEFT))   btn = MouseButton::LEFT;
        else if (match(TokenType::RIGHT))  btn = MouseButton::RIGHT;
        else if (match(TokenType::MIDDLE)) btn = MouseButton::MIDDLE;

        NodePtr pt = nullptr;
        if (match(TokenType::AT))
            pt = parseExpression();

        eat(TokenType::SEMICOLON);
        auto node = std::make_unique<MouseClickNode>(btn, std::move(pt));
        node->line = ln; node->column = col;
        return node;
    }

    throw ParseError("Expected MOVE or CLICK after MOUSE", ln, col);
}

// ──────────────────────────────────────────────────────────────────────────
//  Automation: KEY commands
//  KEY PRESS "keysym";
//  KEY TYPE "text";
//  KEY TYPE varName;
// ──────────────────────────────────────────────────────────────────────────
NodePtr Parser::parseKeyCommand() {
    int ln = cur().line, col = cur().column;
    eat(TokenType::KEY);

    if (match(TokenType::PRESS)) {
        // Accept a string literal OR an identifier
        std::string key;
        if (check(TokenType::STRING))
            key = eat(TokenType::STRING).value;
        else
            key = eat(TokenType::IDENTIFIER).value;
        eat(TokenType::SEMICOLON);
        auto node = std::make_unique<KeyPressNode>(key);
        node->line = ln; node->column = col;
        return node;
    }

    if (match(TokenType::TYPE_KW)) {
        NodePtr expr = parseExpression();
        eat(TokenType::SEMICOLON);
        auto node = std::make_unique<KeyTypeNode>(std::move(expr));
        node->line = ln; node->column = col;
        return node;
    }

    throw ParseError("Expected PRESS or TYPE after KEY", ln, col);
}

// ──────────────────────────────────────────────────────────────────────────
//  Automation: APP commands
//  APP OPEN "name";
//  APP LIST; (as statement or expression)
// ──────────────────────────────────────────────────────────────────────────
NodePtr Parser::parseAppCommand() {
    int ln = cur().line, col = cur().column;
    eat(TokenType::APP);

    if (match(TokenType::OPEN)) {
        NodePtr name = parseExpression();
        eat(TokenType::SEMICOLON);
        auto node = std::make_unique<AppOpenNode>(std::move(name));
        node->line = ln; node->column = col;
        return node;
    }

    if (match(TokenType::LIST_TYPE)) {
        // If followed by ;, it's a statement. Otherwise it was handled by parsePrimary as expr.
        // But here we are in parseStatement context if called from there.
        auto node = std::make_unique<AppListNode>();
        node->line = ln; node->column = col;
        if (check(TokenType::SEMICOLON)) eat(TokenType::SEMICOLON);
        return node;
    }

    throw ParseError("Expected OPEN or LIST after APP", ln, col);
}

} // namespace Synapse
