#include "config.hpp"
#include "engine/cpu.hpp"
#include "language/lexer/lexer.hpp"
#include "language/parser/parser.hpp"
#include "language/semantic/semantic.hpp"
#include "language/codegen/ir_generator.hpp"
#include <iostream>
#include <string>

void run_repl() {
    using namespace DemiLanguage;

    CPU cpu;
    cpu.reset();
    initialize_devices(&cpu);

    std::cout << "Demi REPL  v1.0  —  type .exit to quit, .help for commands" << std::endl;

    std::string input;

    while (true) {
        std::cout << ">>> " << std::flush;
        if (!std::getline(std::cin, input)) break;
        if (input.empty()) continue;

        if (input == ".exit" || input == ".quit") break;
        if (input == ".help") {
            std::cout << "  <stmt>           Execute a statement (let, if, while, etc.)" << std::endl;
            std::cout << "  fn ...           Define a function" << std::endl;
            std::cout << "  .exit/.quit      Exit REPL" << std::endl;
            std::cout << "  .help            Show this help" << std::endl;
            continue;
        }

        // Wrap input as the entry-point function 'main'
        std::string wrapped = "fn main() { " + input + "; }";

        Lexer lexer(wrapped);
        auto tokens = lexer.tokenize();
        if (lexer.has_errors()) {
            for (auto& e : lexer.get_errors()) std::cerr << "Lexer: " << e << std::endl;
            continue;
        }
        Parser parser(std::move(tokens));
        auto mod = parser.parse();
        if (parser.has_errors()) {
            for (auto& e : parser.get_errors()) std::cerr << "Parse: " << e << std::endl;
            continue;
        }
        SemanticAnalyzer sema;
        sema.analyze(*mod);
        if (sema.has_errors()) {
            for (auto& e : sema.get_errors()) std::cerr << "Semantic: " << e.message << std::endl;
            continue;
        }
        IRGenerator codegen;
        auto result = codegen.generate(*mod);
        if (!codegen.get_errors().empty()) {
            for (auto& e : codegen.get_errors()) std::cerr << "Codegen: " << e << std::endl;
            continue;
        }

        // Debug: show bytecode
        if (Config::debug) {
            std::cout << "Bytecode (" << result.bytecode.size() << " bytes): ";
            for (auto b : result.bytecode) std::cout << std::hex << (int)b << " ";
            std::cout << std::dec << std::endl;
        }

        // Execute on VM
        cpu.reset();
        initialize_devices(&cpu);
        try {
            cpu.execute(result.bytecode, 0, 10000);
        } catch (const std::exception& e) {
            std::cerr << "Runtime: " << e.what() << std::endl;
        }
    }
}
