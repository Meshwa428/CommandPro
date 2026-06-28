#include <catch2/catch_test_macros.hpp>
#include "synapse/frontend/lexer.h"

static std::vector<syn::Token> tokenize(const std::string& src)
{
    syn::Source source("test", src);
    syn::Lexer  lexer(source);
    return lexer.tokenize();
}

TEST_CASE("Lexer basic numeric literals", "[lexer]")
{
    auto tokens = tokenize("42 0xFF 0b1010 0o77 3.14 1e10");
    
    REQUIRE(tokens.size() == 7); // 6 tokens + EOF
    
    REQUIRE(tokens[0].is(syn::TokenKind::Int));
    REQUIRE(tokens[0].int_val == 42);

    REQUIRE(tokens[1].is(syn::TokenKind::Int));
    REQUIRE(tokens[1].int_val == 0xFF);

    REQUIRE(tokens[2].is(syn::TokenKind::Int));
    REQUIRE(tokens[2].int_val == 10);

    REQUIRE(tokens[3].is(syn::TokenKind::Int));
    REQUIRE(tokens[3].int_val == 63);

    REQUIRE(tokens[4].is(syn::TokenKind::Float));
    REQUIRE(tokens[4].float_val == 3.14);

    REQUIRE(tokens[5].is(syn::TokenKind::Float));
    REQUIRE(tokens[5].float_val == 1e10);

    REQUIRE(tokens[6].is(syn::TokenKind::Eof));
}

TEST_CASE("Lexer duration literals", "[lexer]")
{
    auto tokens = tokenize("500ms 2s 1.5m 0.5h");
    
    REQUIRE(tokens.size() == 5);
    
    REQUIRE(tokens[0].is(syn::TokenKind::Duration));
    REQUIRE(tokens[0].duration_ns == 500000000); // 500ms = 500,000,000ns

    REQUIRE(tokens[1].is(syn::TokenKind::Duration));
    REQUIRE(tokens[1].duration_ns == 2000000000ULL); // 2s

    REQUIRE(tokens[2].is(syn::TokenKind::Duration));
    REQUIRE(tokens[2].duration_ns == 90000000000ULL); // 1.5m = 90s

    REQUIRE(tokens[3].is(syn::TokenKind::Duration));
    REQUIRE(tokens[3].duration_ns == 1800000000000ULL); // 0.5h = 1800s
}

TEST_CASE("Lexer string literals", "[lexer]")
{
    auto tokens = tokenize("\"hello\" r\"C:\\path\" \"\"\"multi\nline\"\"\"");
    
    REQUIRE(tokens.size() == 4);
    REQUIRE(tokens[0].is(syn::TokenKind::String));
    REQUIRE(tokens[1].is(syn::TokenKind::String));
    REQUIRE(tokens[2].is(syn::TokenKind::String));
}

TEST_CASE("Lexer keywords and command keywords", "[lexer]")
{
    auto tokens = tokenize("let const fn mouse click press");
    
    REQUIRE(tokens.size() == 7);
    REQUIRE(tokens[0].is(syn::TokenKind::Let));
    REQUIRE(tokens[1].is(syn::TokenKind::Const));
    REQUIRE(tokens[2].is(syn::TokenKind::Fn));
    REQUIRE(tokens[3].is(syn::TokenKind::Mouse));
    REQUIRE(tokens[4].is(syn::TokenKind::Click));
    REQUIRE(tokens[5].is(syn::TokenKind::Press));
}

TEST_CASE("Lexer string interpolation", "[lexer]")
{
    // "hello {name}!"
    auto tokens = tokenize("\"hello {name}!\"");
    
    REQUIRE(tokens.size() == 7); // StrPart, InterpOpen, Ident, InterpClose, StrPart, String (end), Eof
    REQUIRE(tokens[0].is(syn::TokenKind::StrPart));
    REQUIRE(tokens[1].is(syn::TokenKind::InterpOpen));
    REQUIRE(tokens[2].is(syn::TokenKind::Ident));
    REQUIRE(tokens[3].is(syn::TokenKind::InterpClose));
    REQUIRE(tokens[4].is(syn::TokenKind::StrPart));
    REQUIRE(tokens[5].is(syn::TokenKind::String));
    REQUIRE(tokens[6].is(syn::TokenKind::Eof));
}

TEST_CASE("Lexer operators and delimiters", "[lexer]")
{
    auto tokens = tokenize("+ - * / // % ** == != < <= > >= = += ?? |> ?.");
    
    REQUIRE(tokens.size() == 19);
    REQUIRE(tokens[0].is(syn::TokenKind::Plus));
    REQUIRE(tokens[1].is(syn::TokenKind::Minus));
    REQUIRE(tokens[2].is(syn::TokenKind::Star));
    REQUIRE(tokens[3].is(syn::TokenKind::Slash));
    REQUIRE(tokens[4].is(syn::TokenKind::SlashSlash));
    REQUIRE(tokens[5].is(syn::TokenKind::Percent));
    REQUIRE(tokens[6].is(syn::TokenKind::StarStar));
    REQUIRE(tokens[7].is(syn::TokenKind::EqEq));
    REQUIRE(tokens[8].is(syn::TokenKind::BangEq));
    REQUIRE(tokens[9].is(syn::TokenKind::Lt));
    REQUIRE(tokens[10].is(syn::TokenKind::LtEq));
    REQUIRE(tokens[11].is(syn::TokenKind::Gt));
    REQUIRE(tokens[12].is(syn::TokenKind::GtEq));
    REQUIRE(tokens[13].is(syn::TokenKind::Eq));
    REQUIRE(tokens[14].is(syn::TokenKind::PlusEq));
    REQUIRE(tokens[15].is(syn::TokenKind::QQ));
    REQUIRE(tokens[16].is(syn::TokenKind::Pipe));
    REQUIRE(tokens[17].is(syn::TokenKind::OptChain));
}
