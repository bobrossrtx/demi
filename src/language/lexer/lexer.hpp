#pragma once
#include "../token.hpp"
#include <vector>
#include <string>
#include <unordered_map>

namespace DemiLanguage {

// C-like lexer for the Demi language.
// Tokenizes a source string into a vector of Tokens.
class Lexer {
public:
    explicit Lexer(std::string source);
    
    std::vector<Token> tokenize();
    const std::vector<std::string>& get_errors() const { return errors_; }
    bool has_errors() const { return !errors_.empty(); }

private:
    std::string source_;
    size_t pos_;
    size_t line_;
    size_t column_;
    std::vector<std::string> errors_;
    std::unordered_map<std::string, TokenType> keywords_;

    void init_keywords();

    // Character access
    char peek(size_t ahead = 0) const;
    char advance();
    bool is_at_end() const;
    bool match(char expected);
    void skip_whitespace_and_comments();
    
    // Tokenizers
    Token scan_token();
    Token scan_identifier_or_keyword();
    Token scan_number();
    Token scan_char_literal();
    Token scan_string_literal();
    Token scan_operator();
    Token make_token(TokenType type);
    Token make_error(const std::string& message);
    
    // Helpers
    static bool is_digit(char c);
    static bool is_hex_digit(char c);
    static bool is_alpha(char c);
    static bool is_alphanumeric(char c);
};

} // namespace DemiLanguage
