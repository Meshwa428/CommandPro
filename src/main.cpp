#include "lexer/lexer.h"
#include "parser/parser.h"
#include "interpreter/interpreter.h"
#include "vm/resolver.h"
#include "vm/compiler.h"
#include "vm/vm.h"
#include "platform/platform.h"
#include <iostream>
#include <fstream>
#include <sstream>

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) throw std::runtime_error("Cannot open file: " + path);
    std::stringstream buf;
    buf << f.rdbuf();
    return buf.str();
}

namespace Synapse {
    class IPlatform;
    std::shared_ptr<IPlatform> createMockPlatform();
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Synapse v0.2.0\n"
                     "Usage:\n"
                     "  synapse run <script.syn>     Execute a script\n"
                     "  synapse check <script.syn>   Validate syntax only\n"
                     "Options:\n"
                     "  --vm                        Use Bytecode Virtual Machine\n"
                     "  --mock                      Use mock platform for tests\n";
        return 1;
    }

    std::string command = argv[1];
    std::string filepath = argv[2];

    std::string source;
    try {
        source = readFile(filepath);
    } catch (const std::exception& e) {
        std::cerr << "[Error] " << e.what() << "\n";
        return 1;
    }

    try {
        Synapse::Lexer  lexer(source);
        auto            tokens = lexer.tokenize();

        Synapse::Parser parser(std::move(tokens));
        auto            ast = parser.parse();

        bool useMock = false;
        bool useVM   = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--mock") useMock = true;
            if (arg == "--vm")   useVM   = true;
        }

        if (command == "check") {
            std::cout << "[OK] Syntax valid: " << filepath << "\n";
            return 0;
        }

        if (command == "run") {
            std::shared_ptr<Synapse::IPlatform> platform;
            if (useMock) {
                namespace Syn = Synapse;
                platform = Syn::createMockPlatform();
            } else {
                platform = Synapse::IPlatform::create();
            }

            if (useVM) {
                Synapse::Resolver resolver;
                resolver.resolve(*ast);
                
                Synapse::Compiler compiler;
                auto* chunk = compiler.compile(*ast);
                
                Synapse::VM vm(platform);
                vm.setGlobals(compiler.getGlobalNames());
                if (vm.interpret(chunk) == Synapse::InterpretResult::RUNTIME_ERROR) {
                    // Redundant - VM prints its own error
                    decref(chunk);
                    return 1;
                }
                decref(chunk);
            } else {
                Synapse::Interpreter interp(platform);
                interp.interpret(*ast);
            }
            return 0;
        }

        std::cerr << "[Error] Unknown command: " << command << "\n";
        return 1;

    } catch (const Synapse::LexerError& e) {
        std::cerr << "[LexerError] " << filepath
                  << ":" << e.line << ":" << e.column << ": " << e.what() << "\n";
    } catch (const Synapse::ParseError& e) {
        std::cerr << "[ParseError] " << filepath
                  << ":" << e.line << ":" << e.column << ": " << e.what() << "\n";
    } catch (const Synapse::RuntimeError& e) {
        std::cerr << "[RuntimeError] " << filepath
                  << ":" << e.line << ":" << e.column << ": " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[Error] " << e.what() << "\n";
    }
    return 1;
}
