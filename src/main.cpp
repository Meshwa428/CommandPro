#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "synapse/frontend/lexer.h"

static void run_file(const std::string& path)
{
    std::ifstream f(path);
    if (!f) {
        std::cerr << "syn: cannot open '" << path << "'\n";
        std::exit(1);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string src = ss.str();

    syn::Source source(path, src);
    syn::Lexer  lexer(source);
    auto tokens = lexer.tokenize();

    if (lexer.has_errors()) {
        lexer.diag().print_all(source);
        std::exit(1);
    }

    // Phase 1: just dump tokens for now
    for (auto& tok : tokens) {
        std::cout << tok.to_string() << '\n';
    }
}

static void repl()
{
    std::cout << "Synapse v0.2.0 — type 'exit' to quit\n";
    std::string line;
    while (true) {
        std::cout << ">>> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "exit" || line == "quit") break;
        if (line.empty()) continue;

        syn::Source source("<repl>", line);
        syn::Lexer  lexer(source);
        auto tokens = lexer.tokenize();
        
        if (lexer.has_errors()) {
            lexer.diag().print_all(source);
            continue;
        }

        for (auto& tok : tokens) {
            std::cout << tok.to_string() << '\n';
        }
    }
}

int main(int argc, char* argv[])
{
    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        run_file(argv[1]);
    } else {
        std::cerr << "Usage: syn [script.syn]\n";
        return 1;
    }
    return 0;
}
