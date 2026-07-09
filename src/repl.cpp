#include "config.hpp"
#include "engine/cpu.hpp"
#include "language/lexer/lexer.hpp"
#include "language/parser/parser.hpp"
#include "language/semantic/semantic.hpp"
#include "language/codegen/ir_generator.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>

void run_repl() {
    using namespace DemiLanguage;

    std::cout << "Demi REPL  v1.1  —  type .exit to quit, .help for commands" << std::endl;

    // Accumulate all statements into one growing program
    std::string prog;
    std::string input;

    while (true) {
        std::cout << ">>> " << std::flush;
        if (!std::getline(std::cin, input)) break;
        if (input.empty()) continue;

        if (input == ".exit" || input == ".quit") break;
        if (input == ".help") {
            std::cout << "  <stmt>           Execute statement (persistent state)" << std::endl;
            std::cout << "  fn ...           Define a function (persistent)" << std::endl;
            std::cout << "  .clear           Reset REPL state" << std::endl;
            std::cout << "  .show            Show accumulated program" << std::endl;
            std::cout << "  .exit/.quit      Exit REPL" << std::endl;
            std::cout << "  .help            Show this help" << std::endl;
            continue;
        }
        if (input == ".clear") {
            prog.clear();
            std::cout << "REPL state cleared." << std::endl;
            continue;
        }
        if (input == ".show") {
            std::cout << "--- program ---" << std::endl;
            std::cout << prog << std::endl;
            std::cout << "--- end ---" << std::endl;
            continue;
        }

        // Append this statement to the accumulated program
        if (!prog.empty()) prog += "\n";
        prog += input + ";";

        // Wrap accumulated program into a main function with stdlib imports
        std::string wrapped =
            "import \"console\";\n"
            "import \"math\";\n"
            "import \"sys\";\n"
            "import \"mem\";\n"
            "import \"str\";\n"
            "fn main() {\n" + prog + "\n}";

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

        // Re-execute full program on fresh CPU
        CPU cpu;
        cpu.reset();
        initialize_devices(&cpu);
        try {
            cpu.execute(result.bytecode, 0, 10000);
        } catch (const std::exception& e) {
            std::cerr << "Runtime: " << e.what() << std::endl;
        }
        std::cout << std::endl;  // newline after execution output
    }
}
