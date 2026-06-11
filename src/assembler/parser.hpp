#pragma once
#include "token.hpp"
#include "ast.hpp"
#include <vector>
#include <memory>
#include <string>

namespace Assembler {

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);
    
    std::unique_ptr<Program> parse();
    const std::vector<std::string>& get_errors() const { return errors; }
    bool has_errors() const { return !errors.empty(); }

private:
    const std::vector<Token>& tokens;
    size_t pos;
    std::vector<std::string> errors;
    
    // Helper methods
    const Token& current_token() const;
    const Token& peek_token(size_t offset = 1) const;
    void advance();
    bool is_at_end() const;
    bool match(TokenType type);
    bool consume(TokenType type, const std::string& error_message);
    
    // Parsing methods
    std::unique_ptr<Statement> parse_statement();
    std::unique_ptr<Instruction> parse_instruction(const std::string& mnemonic, size_t line, size_t col);
    std::unique_ptr<Instruction> parse_debug_directive();
    std::unique_ptr<Directive> parse_directive(const std::string& directive_name, size_t line, size_t col);
    std::unique_ptr<Directive> parse_define_directive(const std::string& directive_name, size_t line, size_t col);
    std::unique_ptr<Label> parse_label(const std::string& label_name, size_t line, size_t col);
    
    // Test directive parsing
    std::unique_ptr<TestCase> parse_test_case(size_t line, size_t col);
    std::unique_ptr<TestAssertion> parse_test_assertion(TestAssertionType type, size_t line, size_t col);
    
    std::unique_ptr<Expression> parse_expression();
    std::unique_ptr<Expression> parse_primary_expression();
    std::unique_ptr<Expression> parse_memory_reference(uint8_t size_hint_bits = 0);
    std::unique_ptr<Expression> parse_memory_term(bool allow_negative_immediate = false);
    uint8_t parse_size_hint_bits(const Token& token) const;
    
    // Utility methods
    void skip_newlines();
    void add_error(const std::string& message);
    void add_error(const std::string& message, const Token& token);
};

} // namespace Assembler
