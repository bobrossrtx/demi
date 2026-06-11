#include "parser.hpp"
#include "../debug/error_handler.hpp"
#include <iostream>
#include <algorithm>
#include <cctype>

namespace Assembler {

Parser::Parser(const std::vector<Token>& tokens)
    : tokens(tokens), pos(0) {}

std::unique_ptr<Program> Parser::parse() {
    auto program = std::make_unique<Program>();
    
    skip_newlines();
    
    while (!is_at_end()) {
        auto stmt = parse_statement();
        if (stmt) {
            // Check if this is a test case using dynamic_cast
            if (dynamic_cast<TestCase*>(stmt.get())) {
                // Safely cast and move the unique_ptr
                std::unique_ptr<TestCase> test_case(static_cast<TestCase*>(stmt.release()));
                program->add_test_case(std::move(test_case));
            } else {
                program->add_statement(std::move(stmt));
            }
        }
        skip_newlines();
    }
    
    return program;
}

const Token& Parser::current_token() const {
    if (pos >= tokens.size()) {
        static Token eof_token(TokenType::END_OF_FILE, "");
        return eof_token;
    }
    return tokens[pos];
}

const Token& Parser::peek_token(size_t offset) const {
    size_t peek_pos = pos + offset;
    if (peek_pos >= tokens.size()) {
        static Token eof_token(TokenType::END_OF_FILE, "");
        return eof_token;
    }
    return tokens[peek_pos];
}

void Parser::advance() {
    if (pos < tokens.size()) {
        pos++;
    }
}

bool Parser::is_at_end() const {
    return pos >= tokens.size() || current_token().type == TokenType::END_OF_FILE;
}

bool Parser::match(TokenType type) {
    if (current_token().type == type) {
        advance();
        return true;
    }
    return false;
}

bool Parser::consume(TokenType type, const std::string& error_message) {
    if (current_token().type == type) {
        advance();
        return true;
    }
    add_error(error_message, current_token());
    return false;
}

void Parser::skip_newlines() {
    while (current_token().type == TokenType::NEWLINE) {
        advance();
    }
}

std::unique_ptr<Statement> Parser::parse_statement() {
    const Token& token = current_token();
    
    switch (token.type) {
        case TokenType::MNEMONIC: {
            std::string mnemonic = token.text;
            size_t line = token.line;
            size_t col = token.column;
            advance();
            return parse_instruction(mnemonic, line, col);
        }
        
        case TokenType::DIRECTIVE: {
            std::string directive = token.text;
            size_t line = token.line;
            size_t col = token.column;
            advance();
            return parse_directive(directive, line, col);
        }

        case TokenType::PRINT:
        case TokenType::BREAK:
        case TokenType::DUMP:
        case TokenType::MEMDUMP:
        case TokenType::TRACE:
        case TokenType::ASSERT:
        case TokenType::DUMPSTACK:
        case TokenType::WATCH:
        case TokenType::UNWATCH:
        case TokenType::CHECKPOINT:
        case TokenType::LOG:
        case TokenType::DUMPREG:
        case TokenType::MEMSET:
        case TokenType::STEP: {
            return parse_debug_directive();
        }

        case TokenType::SECTION:
        case TokenType::GLOBAL:
        case TokenType::EXTERN: {
            std::string directive = token.text;
            size_t line = token.line;
            size_t col = token.column;
            advance();
            return parse_directive(directive, line, col);
        }
        
        case TokenType::TEST_DIRECTIVE: {
            size_t line = token.line;
            size_t col = token.column;
            advance();
            return parse_test_case(line, col);
        }
        
        case TokenType::ASSERT_MEM: {
            size_t line = token.line;
            size_t col = token.column;
            advance();
            return parse_test_assertion(TestAssertionType::ASSERT_MEM, line, col);
        }
        
        case TokenType::ASSERT_REG: {
            size_t line = token.line;
            size_t col = token.column;
            advance();
            return parse_test_assertion(TestAssertionType::ASSERT_REG, line, col);
        }
        
        case TokenType::ASSERT_FPU: {
            size_t line = token.line;
            size_t col = token.column;
            advance();
            return parse_test_assertion(TestAssertionType::ASSERT_FPU, line, col);
        }
        
        case TokenType::ASSERT_OUTPUT: {
            size_t line = token.line;
            size_t col = token.column;
            advance();
            return parse_test_assertion(TestAssertionType::ASSERT_OUTPUT, line, col);
        }
        
        case TokenType::EXPECT_ERROR: {
            size_t line = token.line;
            size_t col = token.column;
            advance();
            return parse_test_assertion(TestAssertionType::EXPECT_ERROR, line, col);
        }
        
        // Handle metadata and preprocessor directives
        case TokenType::DESCRIPTION:
        case TokenType::AUTHOR:
        case TokenType::CATEGORY:
        case TokenType::TAG:
        case TokenType::MAXSTEPS:
        case TokenType::MAXCALLDEPTH:
        case TokenType::TIMEOUT:
        case TokenType::SKIP:
        case TokenType::BENCHMARK:
        case TokenType::WARMUP:
        case TokenType::ITERATIONS:
        case TokenType::MEASURE:
        case TokenType::INCLUDE:
        case TokenType::IFDEF:
        case TokenType::IFNDEF:
        case TokenType::ENDIF:
        case TokenType::ELSE: {
            std::string directive = token.text;
            size_t line = token.line;
            size_t col = token.column;
            advance();
            return parse_directive(directive, line, col);
        }
        
        case TokenType::DEFINE: {
            std::string directive = token.text;
            size_t line = token.line;
            size_t col = token.column;
            advance();
            return parse_define_directive(directive, line, col);
        }
        
        case TokenType::IDENTIFIER: {
            // Could be a label if followed by colon
            if (peek_token().type == TokenType::COLON) {
                std::string label_name = token.text;
                size_t line = token.line;
                size_t col = token.column;
                advance(); // identifier
                advance(); // colon
                return parse_label(label_name, line, col);
            } else if (peek_token().type == TokenType::MNEMONIC) {
                // Check if next token is "equ" (case insensitive)
                std::string next_text = peek_token().text;
                std::transform(next_text.begin(), next_text.end(), next_text.begin(), ::toupper);
                
                if (next_text == "EQU" || next_text == ".EQU") {
                    // Handle: identifier EQU value
                    std::string const_name = token.text;
                    size_t line = token.line;
                    size_t col = token.column;
                    advance(); // identifier
                    advance(); // equ
                    
                    // Parse the value expression
                    auto value_expr = parse_expression();
                    if (!value_expr) {
                        add_error("Expected value expression after EQU", current_token());
                        return nullptr;
                    }
                    
                    // Create a directive node for .equ
                    auto directive = std::make_unique<Directive>(".equ", line, col);
                    
                    // First argument is the constant name
                    directive->arguments.push_back(std::make_unique<IdentifierExpression>(const_name));
                    // Second argument is the value
                    directive->arguments.push_back(std::move(value_expr));
                    
                    return directive;
                } else {
                    add_error("Unexpected identifier", token);
                    advance();
                    return nullptr;
                }
            } else {
                add_error("Unexpected identifier", token);
                advance();
                return nullptr;
            }
        }
        
        case TokenType::NEWLINE:
            advance();
            return nullptr;
            
        case TokenType::END_OF_FILE:
            return nullptr;
            
        default:
            add_error("Unexpected token", token);
            advance();
            return nullptr;
    }
}

std::unique_ptr<Instruction> Parser::parse_instruction(const std::string& mnemonic, size_t line, size_t col) {
    auto instruction = std::make_unique<Instruction>(mnemonic, line, col);
    
    // Parse operands (comma-separated)
    if (current_token().type != TokenType::NEWLINE && 
        current_token().type != TokenType::END_OF_FILE) {
        
        do {
            auto operand = parse_expression();
            if (operand) {
                instruction->add_operand(std::move(operand));
            }
        } while (match(TokenType::COMMA));
    }
    
    return instruction;
}

std::unique_ptr<Directive> Parser::parse_directive(const std::string& directive_name, size_t line, size_t col) {
    auto directive = std::make_unique<Directive>(directive_name, line, col);
    
    // Parse arguments
    if (current_token().type != TokenType::NEWLINE && 
        current_token().type != TokenType::END_OF_FILE) {
        
        do {
            auto arg = parse_expression();
            if (arg) {
                directive->add_argument(std::move(arg));
            }
        } while (match(TokenType::COMMA));
    }
    
    return directive;
}

std::unique_ptr<Directive> Parser::parse_define_directive(const std::string& directive_name, size_t line, size_t col) {
    auto directive = std::make_unique<Directive>(directive_name, line, col);
    
    // Parse .define IDENTIFIER VALUE
    // First argument should be an identifier
    if (current_token().type == TokenType::IDENTIFIER) {
        std::string name = current_token().text;
        advance();
        auto name_expr = std::make_unique<IdentifierExpression>(name, current_token().line, current_token().column);
        directive->add_argument(std::move(name_expr));
        
        // Second argument should be a value (number or string)
        if (current_token().type == TokenType::NUMBER) {
            if (current_token().is_float()) {
                double value = current_token().as_float();
                advance();
                auto value_expr = std::make_unique<FloatExpression>(value, current_token().line, current_token().column);
                directive->add_argument(std::move(value_expr));
            } else {
                int64_t value = current_token().as_int();
                advance();
                auto value_expr = std::make_unique<ImmediateExpression>(value, current_token().line, current_token().column);
                directive->add_argument(std::move(value_expr));
            }
        } else if (current_token().type == TokenType::STRING) {
            std::string value = current_token().as_string();
            advance();
            auto value_expr = std::make_unique<StringLiteralExpression>(value, current_token().line, current_token().column);
            directive->add_argument(std::move(value_expr));
        } else {
            add_error("Expected value after identifier in .define directive", current_token());
        }
    } else {
        add_error("Expected identifier after .define", current_token());
    }
    
    return directive;
}

std::unique_ptr<Label> Parser::parse_label(const std::string& label_name, size_t line, size_t col) {
    return std::make_unique<Label>(label_name, line, col);
}

std::unique_ptr<Expression> Parser::parse_expression() {
    if (current_token().type == TokenType::SIZE_SPECIFIER) {
        const Token size_token = current_token();
        const uint8_t size_hint_bits = parse_size_hint_bits(size_token);
        advance();

        if (current_token().type != TokenType::LBRACKET) {
            add_error("Expected '[' after memory size specifier", current_token());
            return nullptr;
        }

        return parse_memory_reference(size_hint_bits);
    }

    // Handle memory references [expression]
    if (current_token().type == TokenType::LBRACKET) {
        return parse_memory_reference();
    }
    
    return parse_primary_expression();
}

std::unique_ptr<Expression> Parser::parse_primary_expression() {
    const Token& token = current_token();
    
    switch (token.type) {
        case TokenType::REGISTER: {
            std::string reg_name = token.text;
            advance();
            return std::make_unique<RegisterExpression>(reg_name, token.line, token.column);
        }
        
        case TokenType::NUMBER: {
            if (token.is_float()) {
                double value = token.as_float();
                advance();
                return std::make_unique<FloatExpression>(value, token.line, token.column);
            } else {
                int64_t value = token.as_int();
                advance();
                return std::make_unique<ImmediateExpression>(value, token.line, token.column);
            }
        }
        
        case TokenType::IDENTIFIER: {
            std::string name = token.text;
            
            // Check for ST(i) syntax
            if (name == "ST" || name == "st") {
                advance(); // consume "ST"
                
                // Expect opening parenthesis
                if (current_token().type != TokenType::LPAREN) {
                    add_error("Expected '(' after ST", current_token());
                    return std::make_unique<IdentifierExpression>(name, token.line, token.column);
                }
                advance(); // consume '('
                
                // Expect number (ST index)
                if (current_token().type != TokenType::NUMBER) {
                    add_error("Expected number in ST(...)", current_token());
                    return std::make_unique<IdentifierExpression>(name, token.line, token.column);
                }
                
                int64_t st_index = current_token().as_int();
                if (st_index < 0 || st_index > 7) {
                    add_error("ST index must be 0-7", current_token());
                    return std::make_unique<IdentifierExpression>(name, token.line, token.column);
                }
                advance(); // consume number
                
                // Expect closing parenthesis
                if (current_token().type != TokenType::RPAREN) {
                    add_error("Expected ')' after ST index", current_token());
                    return std::make_unique<IdentifierExpression>(name, token.line, token.column);
                }
                advance(); // consume ')'
                
                return std::make_unique<STRegisterExpression>(static_cast<uint8_t>(st_index), token.line, token.column);
            }
            
            // Regular identifier
            advance();
            return std::make_unique<IdentifierExpression>(name, token.line, token.column);
        }
        
        case TokenType::STRING: {
            std::string value = token.as_string();
            advance();
            return std::make_unique<StringLiteralExpression>(value, token.line, token.column);
        }

        case TokenType::DIRECTIVE:
        case TokenType::SECTION:
        case TokenType::GLOBAL:
        case TokenType::EXTERN: {
            // Allow directives to be used as identifiers (e.g. section names like .text)
            std::string name = token.text;
            advance();
            return std::make_unique<IdentifierExpression>(name, token.line, token.column);
        }
        
        default:
            add_error("Expected expression", token);
            advance();
            return nullptr;
    }
}

std::unique_ptr<Expression> Parser::parse_memory_term(bool allow_negative_immediate) {
    if (allow_negative_immediate && current_token().type == TokenType::MINUS) {
        const Token minus_token = current_token();
        advance();

        if (current_token().type != TokenType::NUMBER || current_token().is_float()) {
            add_error("Expected integer literal after '-' in memory reference", current_token());
            return nullptr;
        }

        const int64_t value = -current_token().as_int();
        const size_t line = minus_token.line;
        const size_t column = minus_token.column;
        advance();
        return std::make_unique<ImmediateExpression>(value, line, column);
    }

    return parse_primary_expression();
}

uint8_t Parser::parse_size_hint_bits(const Token& token) const {
    if (token.text == "byte") {
        return 8;
    }
    if (token.text == "word") {
        return 16;
    }
    if (token.text == "dword") {
        return 32;
    }
    if (token.text == "qword") {
        return 64;
    }
    return 0;
}

std::unique_ptr<Expression> Parser::parse_memory_reference(uint8_t size_hint_bits) {
    if (!consume(TokenType::LBRACKET, "Expected '['")) {
        return nullptr;
    }

    const Token bracket_token = current_token();
    std::unique_ptr<Expression> base = nullptr;
    std::unique_ptr<Expression> simple_offset = nullptr;
    std::unique_ptr<Expression> index = nullptr;
    std::unique_ptr<Expression> displacement = nullptr;
    std::unique_ptr<IdentifierExpression> symbol = nullptr;

    uint8_t scale = 1;
    int64_t displacement_value = 0;
    bool has_displacement = false;
    bool saw_complex_form = false;
    int sign = 1;

    while (current_token().type != TokenType::RBRACKET && current_token().type != TokenType::END_OF_FILE) {
        auto term = parse_memory_term(sign < 0);
        if (!term) {
            return nullptr;
        }

        int term_sign = sign;
        sign = 1;

        if (current_token().type == TokenType::ASTERISK) {
            saw_complex_form = true;

            auto* register_term = dynamic_cast<RegisterExpression*>(term.get());
            if (!register_term) {
                add_error("Scaled memory index must start with a register", current_token());
                return nullptr;
            }

            advance();

            if (current_token().type != TokenType::NUMBER || current_token().is_float()) {
                add_error("Expected integer scale after '*' in memory reference", current_token());
                return nullptr;
            }

            const int64_t scale_value = current_token().as_int();
            if (scale_value != 1 && scale_value != 2 && scale_value != 4 && scale_value != 8) {
                add_error("Memory index scale must be 1, 2, 4, or 8", current_token());
                return nullptr;
            }

            advance();

            if (index) {
                add_error("Memory reference can only contain one scaled index", current_token());
                return nullptr;
            }

            index = std::move(term);
            scale = static_cast<uint8_t>(scale_value);

            if (current_token().type == TokenType::PLUS || current_token().type == TokenType::MINUS) {
                sign = current_token().type == TokenType::MINUS ? -1 : 1;
                advance();
                continue;
            }

            break;
        }

        if (dynamic_cast<RegisterExpression*>(term.get())) {
            if (!base) {
                base = std::move(term);
            } else if (!index) {
                index = std::move(term);
                saw_complex_form = true;
            } else {
                add_error("Memory reference can contain at most two registers", current_token());
                return nullptr;
            }
        } else if (auto* imm = dynamic_cast<ImmediateExpression*>(term.get())) {
            displacement_value += term_sign * imm->value;
            has_displacement = true;
        } else if (auto* ident = dynamic_cast<IdentifierExpression*>(term.get())) {
            if (!base && !symbol) {
                base = std::move(term);
                symbol = std::make_unique<IdentifierExpression>(ident->name, ident->line, ident->column);
            } else if (!symbol) {
                symbol = std::make_unique<IdentifierExpression>(ident->name, ident->line, ident->column);
                saw_complex_form = true;
            } else {
                add_error("Memory reference can contain at most one symbolic term", current_token());
                return nullptr;
            }
        } else {
            add_error("Unsupported term inside memory reference", current_token());
            return nullptr;
        }

        if (current_token().type == TokenType::PLUS || current_token().type == TokenType::MINUS) {
            sign = current_token().type == TokenType::MINUS ? -1 : 1;
            advance();
            continue;
        }

        break;
    }

    if (!consume(TokenType::RBRACKET, "Expected ']'")) {
        return nullptr;
    }

    if (!base && !index && !symbol && !has_displacement) {
        add_error("Empty memory reference is not allowed", bracket_token);
        return nullptr;
    }

    if (has_displacement) {
        displacement = std::make_unique<ImmediateExpression>(displacement_value, bracket_token.line, bracket_token.column);
        if (!saw_complex_form && base && !index && !symbol) {
            simple_offset = std::make_unique<ImmediateExpression>(displacement_value, bracket_token.line, bracket_token.column);
        }
    }

    if (!base && symbol) {
        base = std::make_unique<IdentifierExpression>(symbol->name, symbol->line, symbol->column);
    }

    if (!base && has_displacement) {
        base = std::make_unique<ImmediateExpression>(displacement_value, bracket_token.line, bracket_token.column);
        displacement.reset();
        simple_offset.reset();
    }

    auto memory = std::make_unique<MemoryReferenceExpression>(std::move(base), std::move(simple_offset), bracket_token.line, bracket_token.column);
    memory->index = std::move(index);
    memory->scale = scale;
    memory->size_hint_bits = size_hint_bits;
    memory->symbol = std::move(symbol);

    if (displacement) {
        memory->displacement = std::move(displacement);
    }

    return memory;
}

std::unique_ptr<TestCase> Parser::parse_test_case(size_t line, size_t col) {
    // Expect: .test "test name" { ... }
    
    // Parse test name (should be a STRING token)
    if (current_token().type != TokenType::STRING) {
        add_error("Expected string literal for test name", current_token());
        return nullptr;
    }
    
    std::string test_name = current_token().as_string();
    advance();
    
    // Expect opening brace
    if (!consume(TokenType::LBRACE, "Expected '{' to start test body")) {
        return nullptr;
    }
    
    // Create test case
    auto test_case = std::make_unique<TestCase>(test_name, line, col);
    
    // Parse test body statements and metadata
    while (current_token().type != TokenType::RBRACE && 
           current_token().type != TokenType::END_OF_FILE) {
        
        // Handle metadata directives
        if (current_token().type == TokenType::DESCRIPTION) {
            advance();
            if (current_token().type == TokenType::STRING) {
                if (!test_case->set_description(current_token().as_string())) {
                    add_error(".description directive can only be used once per test", current_token());
                }
                advance();
                skip_newlines();
            } else {
                add_error("Expected string after .description", current_token());
            }
            continue;
        } else if (current_token().type == TokenType::AUTHOR) {
            advance();
            if (current_token().type == TokenType::STRING) {
                if (!test_case->set_author(current_token().as_string())) {
                    add_error(".author directive can only be used once per test", current_token());
                }
                advance();
                skip_newlines();
            } else {
                add_error("Expected string after .author", current_token());
            }
            continue;
        } else if (current_token().type == TokenType::CATEGORY) {
            advance();
            if (current_token().type == TokenType::STRING) {
                if (!test_case->set_category(current_token().as_string())) {
                    add_error(".category directive can only be used once per test", current_token());
                }
                advance();
                skip_newlines();
            } else {
                add_error("Expected string after .category", current_token());
            }
            continue;
        } else if (current_token().type == TokenType::TAG) {
            advance();
            if (current_token().type == TokenType::STRING) {
                test_case->add_tag(current_token().as_string());
                advance();
                skip_newlines();
            } else {
                add_error("Expected string after .tag", current_token());
            }
            continue;
        } else if (current_token().type == TokenType::ENTRY_POINT) {
            advance();
            if (current_token().type == TokenType::STRING || current_token().type == TokenType::MNEMONIC || current_token().type == TokenType::NUMBER || current_token().type == TokenType::IDENTIFIER) {
                std::string ep_value;
                if (current_token().type == TokenType::NUMBER) {
                    ep_value = std::to_string(current_token().as_uint());
                } else {
                    ep_value = current_token().as_string();
                }
                if (!test_case->set_entry_point(ep_value)) {
                    add_error(".entry_point directive can only be used once per test", current_token());
                }
                advance();
            } else {
                add_error("Expected label or address after .entry_point", current_token());
            }
            continue;
        } else if (current_token().type == TokenType::MAXSTEPS) {
            advance();
            if (current_token().type == TokenType::NUMBER) {
                if (!test_case->set_max_steps(static_cast<size_t>(current_token().as_uint()))) {
                    add_error(".maxsteps directive can only be used once per test", current_token());
                }
                advance();
                skip_newlines();
            } else {
                add_error("Expected number after .maxsteps", current_token());
            }
            continue;
        } else if (current_token().type == TokenType::MAXCALLDEPTH) {
            advance();
            if (current_token().type == TokenType::NUMBER) {
                if (!test_case->set_max_call_depth(static_cast<size_t>(current_token().as_uint()))) {
                    add_error(".maxcalldepth directive can only be used once per test", current_token());
                }
                advance();
                skip_newlines();  // Skip any newlines after the number
            } else {
                add_error("Expected number after .maxcalldepth", current_token());
            }
            continue;
        } else if (current_token().type == TokenType::TIMEOUT) {
            advance();
            if (current_token().type == TokenType::NUMBER) {
                if (!test_case->set_timeout(static_cast<size_t>(current_token().as_uint()))) {
                    add_error(".timeout directive can only be used once per test", current_token());
                }
                advance();
                skip_newlines();  // Skip any newlines after the number
            } else {
                add_error("Expected number after .timeout", current_token());
            }
            continue;
        } else if (current_token().type == TokenType::SKIP) {
            advance();
            test_case->set_skip(true);
            continue;
        } else if (current_token().type == TokenType::BENCHMARK) {
            advance();
            test_case->set_benchmark(true);
            continue;
        } else if (current_token().type == TokenType::WARMUP) {
            advance();
            if (current_token().type == TokenType::NUMBER) {
                test_case->set_warmup(static_cast<size_t>(current_token().as_uint()));
                advance();
            } else {
                add_error("Expected number after .warmup", current_token());
            }
            continue;
        } else if (current_token().type == TokenType::ITERATIONS) {
            advance();
            if (current_token().type == TokenType::NUMBER) {
                test_case->set_iterations(static_cast<size_t>(current_token().as_uint()));
                advance();
            } else {
                add_error("Expected number after .iterations", current_token());
            }
            continue;
        } else if (current_token().type == TokenType::MEASURE) {
            advance();
            if (current_token().type == TokenType::STRING) {
                test_case->set_measure_type(current_token().as_string());
                advance();
            } else {
                add_error("Expected string after .measure", current_token());
            }
            continue;
        }
        
        // Parse regular statements
        auto stmt = parse_statement();
        if (stmt) {
            test_case->add_statement(std::move(stmt));
        }
    }
    
    // Expect closing brace
    if (!consume(TokenType::RBRACE, "Expected '}' to close test body")) {
        return nullptr;
    }
    
    return test_case;
}

std::unique_ptr<TestAssertion> Parser::parse_test_assertion(TestAssertionType type, size_t line, size_t col) {
    // Parse assertion arguments - all assertions expect comma-separated expressions
    std::vector<std::unique_ptr<Expression>> args;
    
    // Different assertions expect different numbers of arguments:
    // ASSERT_MEM: address, expected_value
    // ASSERT_REG: register_name, expected_value  
    // ASSERT_OUTPUT: expected_string
    // EXPECT_ERROR: error_message (optional)
    
    if (current_token().type == TokenType::NEWLINE || 
        current_token().type == TokenType::END_OF_FILE) {
        // No arguments (only valid for EXPECT_ERROR)
        if (type != TestAssertionType::EXPECT_ERROR) {
            add_error("Expected arguments for assertion", current_token());
            return nullptr;
        }
        return std::make_unique<TestAssertion>(type, std::move(args), line, col);
    }
    
    // Parse comma-separated argument list
    do {
        auto arg = parse_expression();
        if (!arg) {
            add_error("Expected expression in assertion", current_token());
            return nullptr;
        }
        args.push_back(std::move(arg));
        
        if (current_token().type == TokenType::COMMA) {
            advance();
        } else {
            break;
        }
    } while (true);
    
    // Validate argument counts
    switch (type) {
        case TestAssertionType::ASSERT_MEM:
        case TestAssertionType::ASSERT_REG:
        case TestAssertionType::ASSERT_FPU:
            if (args.size() != 2) {
                std::string assertion_name;
                switch(type) {
                    case TestAssertionType::ASSERT_MEM: assertion_name = "#assert_mem"; break;
                    case TestAssertionType::ASSERT_REG: assertion_name = "#assert_reg"; break;
                    case TestAssertionType::ASSERT_FPU: assertion_name = "#assert_fpu"; break;
                    default: assertion_name = "#unknown"; break;
                }
                add_error("Expected 2 arguments for " + assertion_name);
                return nullptr;
            }
            break;
        case TestAssertionType::ASSERT_OUTPUT:
            if (args.size() != 1) {
                add_error("Expected 1 argument for #assert_output");
                return nullptr;
            }
            break;
        case TestAssertionType::EXPECT_ERROR:
            if (args.size() > 1) {
                add_error("Expected 0 or 1 arguments for #expect_error");
                return nullptr;
            }
            break;
    }
    
    return std::make_unique<TestAssertion>(type, std::move(args), line, col);
}

std::unique_ptr<Instruction> Parser::parse_debug_directive() {
    const Token& token = current_token();
    size_t line = token.line;
    size_t col = token.column;
    TokenType type = token.type;
    advance(); // Consume the directive token

    auto instruction = std::make_unique<Instruction>("DEBUG", line, col);
    
    // Add sub-opcode based on directive type
    uint8_t sub_opcode = 0;
    switch (type) {
        case TokenType::PRINT:      sub_opcode = 0; break;
        case TokenType::BREAK:      sub_opcode = 1; break;
        case TokenType::DUMP:       sub_opcode = 2; break;
        case TokenType::MEMDUMP:    sub_opcode = 3; break;
        case TokenType::TRACE:      sub_opcode = 4; break;
        case TokenType::ASSERT:     sub_opcode = 5; break;
        case TokenType::DUMPSTACK:  sub_opcode = 6; break;
        case TokenType::WATCH:      sub_opcode = 7; break;
        case TokenType::UNWATCH:    sub_opcode = 8; break;
        case TokenType::CHECKPOINT: sub_opcode = 9; break;
        case TokenType::LOG:        sub_opcode = 10; break;
        case TokenType::DUMPREG:    sub_opcode = 11; break;
        case TokenType::MEMSET:     sub_opcode = 12; break;
        case TokenType::STEP:       sub_opcode = 13; break;
        default: break;
    }
    
    instruction->add_operand(std::make_unique<ImmediateExpression>(sub_opcode, line, col));
    
    // Parse additional operands
    if (type == TokenType::PRINT) {
        // .print "string" or .print reg
        auto expr = parse_expression();
        if (!expr) {
            add_error("Expected string or register for .print", token);
            return nullptr;
        }
        instruction->add_operand(std::move(expr));
    } else if (type == TokenType::MEMDUMP) {
        // .memdump start, length
        auto start = parse_expression();
        if (!start) {
            add_error("Expected start address for .memdump", token);
            return nullptr;
        }
        instruction->add_operand(std::move(start));
        
        if (match(TokenType::COMMA)) {
            auto len = parse_expression();
            if (!len) {
                add_error("Expected length for .memdump", token);
                return nullptr;
            }
            instruction->add_operand(std::move(len));
        } else {
            add_error("Expected length for .memdump", token);
            return nullptr;
        }
    } else if (type == TokenType::TRACE) {
        // .trace [on/off/1/0] - optional
        if (current_token().type != TokenType::NEWLINE && current_token().type != TokenType::END_OF_FILE) {
             auto expr = parse_expression();
             if (expr) {
                 instruction->add_operand(std::move(expr));
             }
        }
    } else if (type == TokenType::ASSERT) {
        // .assert reg, value  or  .assert [addr], value
        auto first = parse_expression();
        if (!first) {
            add_error("Expected register or memory address for .assert", token);
            return nullptr;
        }
        instruction->add_operand(std::move(first));
        
        if (match(TokenType::COMMA)) {
            auto value = parse_expression();
            if (!value) {
                add_error("Expected value for .assert", token);
                return nullptr;
            }
            instruction->add_operand(std::move(value));
        } else {
            add_error("Expected value for .assert", token);
            return nullptr;
        }
    } else if (type == TokenType::DUMPSTACK) {
        // .dumpstack [depth] - optional
        if (current_token().type != TokenType::NEWLINE && current_token().type != TokenType::END_OF_FILE) {
            auto depth = parse_expression();
            if (depth) {
                instruction->add_operand(std::move(depth));
            }
        }
    } else if (type == TokenType::WATCH) {
        // .watch addr, length
        auto addr = parse_expression();
        if (!addr) {
            add_error("Expected address for .watch", token);
            return nullptr;
        }
        instruction->add_operand(std::move(addr));
        
        if (match(TokenType::COMMA)) {
            auto len = parse_expression();
            if (!len) {
                add_error("Expected length for .watch", token);
                return nullptr;
            }
            instruction->add_operand(std::move(len));
        } else {
            add_error("Expected length for .watch", token);
            return nullptr;
        }
    } else if (type == TokenType::UNWATCH) {
        // .unwatch addr
        auto addr = parse_expression();
        if (!addr) {
            add_error("Expected address for .unwatch", token);
            return nullptr;
        }
        instruction->add_operand(std::move(addr));
    } else if (type == TokenType::CHECKPOINT) {
        // .checkpoint "label"
        auto label = parse_expression();
        if (!label) {
            add_error("Expected label for .checkpoint", token);
            return nullptr;
        }
        instruction->add_operand(std::move(label));
    } else if (type == TokenType::LOG) {
        // .log level, "message"
        auto level = parse_expression();
        if (!level) {
            add_error("Expected level for .log", token);
            return nullptr;
        }
        instruction->add_operand(std::move(level));
        
        if (match(TokenType::COMMA)) {
            auto msg = parse_expression();
            if (!msg) {
                add_error("Expected message for .log", token);
                return nullptr;
            }
            instruction->add_operand(std::move(msg));
        } else {
            add_error("Expected message for .log", token);
            return nullptr;
        }
    } else if (type == TokenType::DUMPREG) {
        // .dumpreg register
        auto reg = parse_expression();
        if (!reg) {
            add_error("Expected register for .dumpreg", token);
            return nullptr;
        }
        instruction->add_operand(std::move(reg));
    } else if (type == TokenType::MEMSET) {
        // .memset addr, length, value
        auto addr = parse_expression();
        if (!addr) {
            add_error("Expected address for .memset", token);
            return nullptr;
        }
        instruction->add_operand(std::move(addr));
        
        if (!match(TokenType::COMMA)) {
            add_error("Expected comma after address in .memset", token);
            return nullptr;
        }
        
        auto len = parse_expression();
        if (!len) {
            add_error("Expected length for .memset", token);
            return nullptr;
        }
        instruction->add_operand(std::move(len));
        
        if (!match(TokenType::COMMA)) {
            add_error("Expected comma after length in .memset", token);
            return nullptr;
        }
        
        auto value = parse_expression();
        if (!value) {
            add_error("Expected value for .memset", token);
            return nullptr;
        }
        instruction->add_operand(std::move(value));
    } else if (type == TokenType::STEP) {
        // .step [count] - optional
        if (current_token().type != TokenType::NEWLINE && current_token().type != TokenType::END_OF_FILE) {
            auto count = parse_expression();
            if (count) {
                instruction->add_operand(std::move(count));
            }
        }
    }
    
    return instruction;
}

void Parser::add_error(const std::string& message) {
    errors.push_back("Parse error: " + message);
    Logging::ErrorHandler::instance().report_parse(Logging::ErrorCode::PARSE_GENERIC, message);
}

void Parser::add_error(const std::string& message, const Token& token) {
    std::string detailed_message = message + " (got '" + token.text + "')";
    std::string error_str = "Line " + std::to_string(token.line) + ", Column " + std::to_string(token.column) + 
                    ": " + detailed_message;
    errors.push_back(error_str);
    Logging::ErrorHandler::instance().report_parse(Logging::ErrorCode::PARSE_UNEXPECTED_TOKEN, detailed_message, "", token.line, token.column);
}

} // namespace Assembler
