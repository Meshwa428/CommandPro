#include <catch2/catch_test_macros.hpp>
#include "synapse/frontend/lexer.h"
#include "synapse/frontend/parser.h"

static syn::Program parse(const std::string& src)
{
    syn::Source source("test", src);
    syn::Lexer  lexer(source);
    auto tokens = lexer.tokenize();
    syn::Parser parser(std::move(tokens), source);
    return parser.parse();
}

// Helper: cast and assert node type
template<typename T>
static const T* as(const syn::ExprNode* n)
{
    return dynamic_cast<const T*>(n);
}
template<typename T>
static const T* as(const syn::StmtNode* n)
{
    return dynamic_cast<const T*>(n);
}

TEST_CASE("Parser: integer literal", "[parser]")
{
    auto prog = parse("42");
    REQUIRE(!prog.has_errors);
    REQUIRE(prog.stmts.size() == 1);
    auto* es = as<syn::ExprStmt>(prog.stmts[0].get());
    REQUIRE(es != nullptr);
    auto* lit = as<syn::IntLitExpr>(es->expr.get());
    REQUIRE(lit != nullptr);
    REQUIRE(lit->value == 42);
}

TEST_CASE("Parser: let statement", "[parser]")
{
    auto prog = parse("let x = 10");
    REQUIRE(!prog.has_errors);
    REQUIRE(prog.stmts.size() == 1);
    auto* ls = as<syn::LetStmt>(prog.stmts[0].get());
    REQUIRE(ls != nullptr);
    REQUIRE(ls->bind_kind == syn::LetBindKind::Simple);
    REQUIRE(ls->names.size() == 1);
    REQUIRE(ls->names[0] == "x");
    auto* val = as<syn::IntLitExpr>(ls->values[0].get());
    REQUIRE(val != nullptr);
    REQUIRE(val->value == 10);
}

TEST_CASE("Parser: binary expression", "[parser]")
{
    auto prog = parse("1 + 2 * 3");
    REQUIRE(!prog.has_errors);
    auto* es = as<syn::ExprStmt>(prog.stmts[0].get());
    // 1 + (2 * 3): top-level is Add
    auto* add = as<syn::BinaryExpr>(es->expr.get());
    REQUIRE(add != nullptr);
    REQUIRE(add->op == syn::TokenKind::Plus);
    auto* mul = as<syn::BinaryExpr>(add->right.get());
    REQUIRE(mul != nullptr);
    REQUIRE(mul->op == syn::TokenKind::Star);
}

TEST_CASE("Parser: if statement", "[parser]")
{
    auto prog = parse("if x > 0 {\nlet y = 1\n}");
    REQUIRE(!prog.has_errors);
    auto* stmt = as<syn::IfStmt>(prog.stmts[0].get());
    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->branches.size() == 1);
    REQUIRE(stmt->branches[0].cond != nullptr);
    REQUIRE(stmt->branches[0].body.size() == 1);
}

TEST_CASE("Parser: function declaration", "[parser]")
{
    auto prog = parse("fn add(x, y) {\nreturn x + y\n}");
    REQUIRE(!prog.has_errors);
    auto* fn = as<syn::FnDeclStmt>(prog.stmts[0].get());
    REQUIRE(fn != nullptr);
    REQUIRE(fn->name == "add");
    REQUIRE(fn->params.size() == 2);
    REQUIRE(fn->params[0].name == "x");
    REQUIRE(fn->params[1].name == "y");
}

TEST_CASE("Parser: ternary expression", "[parser]")
{
    auto prog = parse("let label = \"big\" if score > 90 else \"small\"");
    REQUIRE(!prog.has_errors);
    auto* ls = as<syn::LetStmt>(prog.stmts[0].get());
    REQUIRE(ls != nullptr);
    auto* tern = as<syn::TernaryExpr>(ls->values[0].get());
    REQUIRE(tern != nullptr);
    REQUIRE(tern->value != nullptr);
    REQUIRE(tern->cond  != nullptr);
    REQUIRE(tern->else_val != nullptr);
}

TEST_CASE("Parser: list literal", "[parser]")
{
    auto prog = parse("let xs = [1, 2, 3]");
    REQUIRE(!prog.has_errors);
    auto* ls = as<syn::LetStmt>(prog.stmts[0].get());
    REQUIRE(ls != nullptr);
    auto* list = as<syn::ListExpr>(ls->values[0].get());
    REQUIRE(list != nullptr);
    REQUIRE(list->elements.size() == 3);
}

TEST_CASE("Parser: map literal", "[parser]")
{
    auto prog = parse("let m = {name: \"mesh\", age: 25}");
    REQUIRE(!prog.has_errors);
    auto* ls = as<syn::LetStmt>(prog.stmts[0].get());
    REQUIRE(ls != nullptr);
    auto* map = as<syn::MapExpr>(ls->values[0].get());
    REQUIRE(map != nullptr);
    REQUIRE(map->pairs.size() == 2);
}

TEST_CASE("Parser: for range statement", "[parser]")
{
    auto prog = parse("for i in 0 to 9 {\n}");
    REQUIRE(!prog.has_errors);
    auto* fr = as<syn::ForStmt>(prog.stmts[0].get());
    REQUIRE(fr != nullptr);
    REQUIRE(fr->iter1 == "i");
    REQUIRE(fr->range_end != nullptr);
    REQUIRE(fr->range_step == nullptr);
}

TEST_CASE("Parser: command desugaring", "[parser]")
{
    auto prog = parse("mouse 300, 400");
    REQUIRE(!prog.has_errors);
    auto* es = as<syn::ExprStmt>(prog.stmts[0].get());
    REQUIRE(es != nullptr);
    auto* call = as<syn::CallExpr>(es->expr.get());
    REQUIRE(call != nullptr);
    auto* field = as<syn::FieldExpr>(call->callee.get());
    REQUIRE(field != nullptr);
    REQUIRE(field->field == "move");
    auto* obj = as<syn::IdentExpr>(field->object.get());
    REQUIRE(obj != nullptr);
    REQUIRE(obj->name == "mouse");
    REQUIRE(call->args.size() == 2);
}

TEST_CASE("Parser: while statement", "[parser]")
{
    auto prog = parse("while x > 0 {\nx = x - 1\n}");
    REQUIRE(!prog.has_errors);
    auto* ws = as<syn::WhileStmt>(prog.stmts[0].get());
    REQUIRE(ws != nullptr);
    REQUIRE(ws->cond != nullptr);
    REQUIRE(ws->body.size() == 1);
}

TEST_CASE("Parser: call expression with named arg", "[parser]")
{
    auto prog = parse("connect(\"host\", port: 3000)");
    REQUIRE(!prog.has_errors);
    auto* es = as<syn::ExprStmt>(prog.stmts[0].get());
    auto* call = as<syn::CallExpr>(es->expr.get());
    REQUIRE(call != nullptr);
    REQUIRE(call->args.size() == 2);
    REQUIRE(call->args[0].name.empty()); // positional
    REQUIRE(call->args[1].name == "port");
}

TEST_CASE("Parser: multi-statement program", "[parser]")
{
    auto prog = parse("let x = 1\nlet y = 2\nlet z = x + y");
    REQUIRE(!prog.has_errors);
    REQUIRE(prog.stmts.size() == 3);
}
