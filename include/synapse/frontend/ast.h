#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "synapse/common/source.h"
#include "synapse/frontend/token.h"

namespace syn {

struct ExprNode;
struct StmtNode;
using Expr  = std::unique_ptr<ExprNode>;
using Stmt  = std::unique_ptr<StmtNode>;
using Block = std::vector<Stmt>;

// ── Param ─────────────────────────────────────────────────────────────────────
struct Param {
    std::string name;
    Expr        default_val; // null if none
    bool        is_variadic = false;
    Span        span;
};

// ── Arg ───────────────────────────────────────────────────────────────────────
struct Arg {
    std::string name;  // empty = positional
    Expr        value;
    Span        span;
};

// ── Pattern ───────────────────────────────────────────────────────────────────
enum class PatternKind { Literal, TypeCheck, Tuple, Capture, Wildcard };

struct Pattern {
    PatternKind              kind   = PatternKind::Wildcard;
    Span                     span;
    // Literal fields
    TokenKind                lit_kind = TokenKind::Error; // Int/Float/String/True/False/None
    int64_t                  int_val   = 0;
    double                   float_val = 0.0;
    std::string              str_val;
    // TypeCheck
    TokenKind                type_kw = TokenKind::Error;
    // Tuple / nested
    std::vector<Pattern>     children;
    // Capture
    std::string              name;
};

// ── MatchArm ──────────────────────────────────────────────────────────────────
struct MatchArm {
    Pattern pattern;
    Expr    guard; // null if no 'if' guard
    Block   body;
    Span    span;
};

// ── MapDestructEntry ──────────────────────────────────────────────────────────
struct MapDestructEntry {
    std::string key;
    std::string alias; // same as key if no alias
    Span        span;
};

// ── UseEntry ──────────────────────────────────────────────────────────────────
struct UseEntry {
    std::string              module_str;  // if string-literal module ref
    std::vector<std::string> module_path; // if dotted-ident module ref
    bool                     is_str_ref = false;
    std::string              alias;       // after 'as'
    std::vector<std::string> selects;    // after '{ a, b }'
    Span                     span;
};

// ── CatchClause ───────────────────────────────────────────────────────────────
struct CatchClause {
    std::string name;
    Expr        guard;  // null if no 'if' guard
    Block       body;
    Span        span;
};

// ── CompFor ───────────────────────────────────────────────────────────────────
struct CompFor {
    std::string iter1;
    std::string iter2; // empty if single-bind
    Expr        source;
    Span        span;
};

// ─────────────────────────────────────────────────────────────────────────────
// Expressions
// ─────────────────────────────────────────────────────────────────────────────

struct ExprNode { Span span; virtual ~ExprNode() = default; };

struct IntLitExpr     : ExprNode { int64_t value = 0; };
struct FloatLitExpr   : ExprNode { double  value = 0.0; };
struct BoolLitExpr    : ExprNode { bool    value = false; };
struct NoneLitExpr    : ExprNode {};
struct DurationLitExpr: ExprNode { int64_t ns = 0; };
// raw source text including quotes; compiler handles unescaping
struct StringLitExpr  : ExprNode { std::string raw; };

// "hello {name}!" → parts alternating text/expr
struct InterpPart {
    bool        is_str = true;
    std::string text;  // valid when is_str
    Expr        expr;  // valid when !is_str
};
struct InterpStringExpr : ExprNode { std::vector<InterpPart> parts; };

struct IdentExpr : ExprNode { std::string name; };

struct BinaryExpr : ExprNode {
    TokenKind op = TokenKind::Error;
    Expr      left;
    Expr      right;
};

struct UnaryExpr : ExprNode {
    TokenKind op = TokenKind::Error;
    Expr      operand;
};

// value if cond else else_val
struct TernaryExpr : ExprNode { Expr value; Expr cond; Expr else_val; };

// callee(args)
struct CallExpr : ExprNode {
    Expr             callee;
    std::vector<Arg> args;
};

struct IndexExpr : ExprNode { Expr object; Expr index; };

struct SliceExpr : ExprNode {
    Expr object;
    Expr start;  // null if omitted
    Expr end_;   // null if omitted
};

struct FieldExpr : ExprNode {
    Expr        object;
    std::string field;
    bool        is_optional = false; // true for ?.
};

// chained comparisons: 0 < x < 100
// operands[i] ops[i] operands[i+1] for all i
struct ChainedCmpExpr : ExprNode {
    std::vector<Expr>      operands;
    std::vector<TokenKind> ops;
};

struct TupleExpr : ExprNode { std::vector<Expr> elements; };
struct ListExpr  : ExprNode { std::vector<Expr> elements; };

struct MapPair { Expr key; Expr value; };
struct MapExpr  : ExprNode { std::vector<MapPair> pairs; };

struct ListCompExpr : ExprNode {
    Expr                 body;
    std::vector<CompFor> comp_fors;
    Expr                 filter; // null if none
};

struct MapCompExpr : ExprNode {
    Expr    key;
    Expr    value;
    CompFor comp_for;
    Expr    filter; // null if none
};

struct FnExpr : ExprNode {
    std::vector<Param> params;
    Block              body;
};

// used both as expression and statement
struct MatchExpr : ExprNode {
    Expr                  subject;
    std::vector<MatchArm> arms;
    Block                 else_body; // empty if no else arm
};

// ─────────────────────────────────────────────────────────────────────────────
// Statements
// ─────────────────────────────────────────────────────────────────────────────

struct StmtNode { Span span; virtual ~StmtNode() = default; };

enum class LetBindKind { Simple, TupleDestruct, ListDestruct, MapDestruct };

struct LetStmt : StmtNode {
    LetBindKind                  bind_kind = LetBindKind::Simple;
    std::vector<std::string>     names;       // Simple/TupleDestruct/ListDestruct lhs names
    std::string                  rest_name;   // *rest name, empty if none
    std::vector<MapDestructEntry> map_entries; // MapDestruct lhs
    std::vector<Expr>            values;      // rhs
};

struct ConstStmt : StmtNode {
    std::string name;
    Expr        value;
};

struct AssignStmt : StmtNode {
    std::vector<Expr> lvalues;
    std::vector<Expr> values;
};

struct AugAssignStmt : StmtNode {
    Expr      lvalue;
    TokenKind op = TokenKind::Error;
    Expr      value;
};

struct IfBranch {
    Expr  cond;  // null for the final else branch
    Block body;
    Span  span;
};

struct IfStmt : StmtNode {
    std::vector<IfBranch> branches;
};

struct WhileStmt : StmtNode { Expr cond; Block body; };

struct RepeatStmt : StmtNode {
    Expr        count;
    std::string index_name; // empty if no 'as'
    Block       body;
};

struct ForStmt : StmtNode {
    std::string iter1;
    std::string iter2;      // empty if single-bind
    Expr        source;     // iterable expression
    Expr        range_end;  // non-null if range syntax (source to range_end)
    Expr        range_step; // non-null if 'by' step
    Block       body;
};

struct TryStmt : StmtNode {
    Block                    body;
    std::vector<CatchClause> catches;
    Block                    else_body;
    Block                    finally_body;
};

struct FnDeclStmt : StmtNode {
    std::string        name;
    std::vector<Param> params;
    Block              body;
};

struct ReturnStmt  : StmtNode { std::vector<Expr> values; };
struct ThrowStmt   : StmtNode { Expr value; };
struct BreakStmt   : StmtNode {};
struct ContinueStmt: StmtNode {};

struct UseStmt : StmtNode { std::vector<UseEntry> entries; };

struct InScopeStmt : StmtNode { std::string window_name; Block body; };

struct ExprStmt : StmtNode { Expr expr; };

// ── Program ───────────────────────────────────────────────────────────────────
struct Program {
    Block stmts;
    bool  has_errors = false;
};

} // namespace syn
