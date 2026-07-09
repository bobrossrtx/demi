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
#include <deque>
#include <csignal>
#include <termios.h>
#include <unistd.h>

// === Raw Terminal Line Editor ===

static struct termios orig_termios;

static void term_raw() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

static void term_restore() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

// Ctrl+C flag
static volatile sig_atomic_t sigint_received = 0;
static void sigint_handler(int) {
    sigint_received = 1;
}

// Check if string has unmatched braces (for multi-line continuation)
static bool is_incomplete(const std::string& line) {
    int brace = 0, paren = 0, bracket = 0;
    for (char c : line) {
        switch (c) {
            case '{': brace++; break;
            case '}': brace--; break;
            case '(': paren++; break;
            case ')': paren--; break;
            case '[': bracket++; break;
            case ']': bracket--; break;
        }
    }
    return brace > 0 || paren > 0 || bracket > 0;
}

// Read a line with history, arrow keys, Ctrl+C handling.
// Returns the line, or empty string on Ctrl+D.
static std::string readline_repl(std::deque<std::string>& history, size_t& hist_idx) {
    std::string line;
    size_t cursor = 0;

    // Show prompt
    std::cout << ">>> " << std::flush;

    while (true) {
        if (sigint_received) {
            sigint_received = 0;
            line.clear();
            cursor = 0;
            std::cout << "\n>>> " << std::flush;
            continue;
        }

        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) break;

        if (c == '\n' || c == '\r') {
            std::cout << std::endl;
            if (!line.empty()) {
                // Remove from history if duplicate of last entry
                if (history.empty() || history.back() != line) {
                    history.push_back(line);
                    if (history.size() > 1000) history.pop_front();
                }
                hist_idx = history.size();
            }
            return line;
        }
        if (c == 4) { // Ctrl+D
            std::cout << std::endl;
            return "";
        }
        if (c == 127 || c == 8) { // Backspace
            if (cursor > 0) {
                line.erase(--cursor, 1);
                std::cout << "\b \b" << std::flush;
            }
            continue;
        }
        if (c == 27) { // ESC sequence
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) break;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) break;
            if (seq[0] == '[') {
                switch (seq[1]) {
                    case 'A': // Up arrow
                        if (history.empty()) break;
                        if (hist_idx > 0) hist_idx--;
                        // Clear line and redraw with history
                        std::cout << "\r\033[K>>> " << history[hist_idx] << std::flush;
                        line = history[hist_idx];
                        cursor = line.size();
                        break;
                    case 'B': // Down arrow
                        if (hist_idx < history.size()) hist_idx++;
                        if (hist_idx >= history.size()) {
                            std::cout << "\r\033[K>>> " << std::flush;
                            line.clear();
                            cursor = 0;
                        } else {
                            std::cout << "\r\033[K>>> " << history[hist_idx] << std::flush;
                            line = history[hist_idx];
                            cursor = line.size();
                        }
                        break;
                }
            }
            continue;
        }
        if (c >= 32 && c < 127) {
            line.insert(cursor++, 1, c);
            std::cout << c << std::flush;
        }
    }
    return line;
}

// === REPL ===

void run_repl() {
    using namespace DemiLanguage;

    // Set up signal handler for Ctrl+C
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);

    // Set terminal to raw mode
    term_raw();
    atexit(term_restore);

    std::cout << "Demi REPL  v2.0  —  type .exit to quit, .help for commands" << std::endl
              << "  Ctrl+C: warning   Ctrl+D: exit   ↑↓: history" << std::endl;

    std::deque<std::string> history;
    size_t hist_idx = 0;
    std::string prog;      // accumulated program
    std::string continuum;  // multi-line buffer

    while (true) {
        std::string input = readline_repl(history, hist_idx);

        // Ctrl+D
        if (input.empty() && continuum.empty()) {
            std::cout << "Exiting REPL." << std::endl;
            break;
        }

        // Multi-line continuation
        if (!continuum.empty()) {
            continuum += "\n" + input;
            if (is_incomplete(continuum)) continue; // wait for more
            input = continuum;
            continuum.clear();
        } else if (is_incomplete(input)) {
            continuum = input;
            continue; // wait for more
        }

        if (input.empty()) continue;

        if (input == ".exit" || input == ".quit") break;
        if (input == ".help") {
            std::cout << "  <stmt>           Execute statement (persistent state)\n"
                         "  fn ...           Define a function (persistent)\n"
                         "  { ... }          Multi-line: add lines until braces close\n"
                         "  .clear           Reset REPL state\n"
                         "  .show            Show accumulated program\n"
                         "  .exit/.quit      Exit REPL\n"
                         "  .help            Show this help\n"
                         "  ↑↓ arrows        Command history\n"
                         "  Ctrl+C           Warning (use .exit to quit)\n"
                         "  Ctrl+D           Exit REPL" << std::endl;
            continue;
        }
        if (input == ".clear") {
            prog.clear();
            std::cout << "REPL state cleared." << std::endl;
            continue;
        }
        if (input == ".show") {
            std::cout << "--- program ---\n" << prog << "\n--- end ---" << std::endl;
            continue;
        }

        if (!prog.empty()) prog += "\n";
        prog += input + ";";

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

        CPU cpu;
        cpu.reset();
        initialize_devices(&cpu);
        try {
            cpu.execute(result.bytecode, 0, 10000);
        } catch (const std::exception& e) {
            std::cerr << "Runtime: " << e.what() << std::endl;
        }
        std::cout << std::endl;
    }

    term_restore();
}
