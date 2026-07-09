#pragma once
#include "../token.hpp"
#include "../ast/demi_ast.hpp"
#include <functional>
#include <vector>
#include <string>
#include <memory>

namespace DemiLanguage {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    
    std::unique_ptr<DemiModule> parse();
    const std::vector<std::string>& get_errors() const { return errors_; }
    bool has_errors() const { return !errors_.empty(); }

private:
    std::vector<Token> tokens_;
    size_t pos_;
    std::vector<std::string> errors_;

    // Token navigation
    const Token& peek() const;
    const Token& peek_next() const;
    const Token& advance();
    const Token& previous() const;
    bool is_at_end() const;
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool match_any(std::initializer_list<TokenType> types);
    Token consume(TokenType type, const std::string& error_msg);
    void synchronize();  // panic mode recovery

    // Declarations
    void parse_import(DemiModule& mod);
    std::unique_ptr<DemiFunction> parse_function(bool is_pub);
    std::unique_ptr<DemiStruct> parse_struct(bool is_pub);
    std::unique_ptr<DemiEnum> parse_enum(bool is_pub);
    std::pair<std::string, std::unique_ptr<DemiType>> parse_param();
    std::unique_ptr<DemiType> parse_type();
    std::unique_ptr<DemiType> parse_type_suffix(std::unique_ptr<DemiType> base);

    // Statements
    std::unique_ptr<DemiStmt> parse_statement();
    std::unique_ptr<DemiStmt> parse_block();
    std::unique_ptr<DemiStmt> parse_let();
    std::unique_ptr<DemiStmt> parse_if();
    std::unique_ptr<DemiStmt> parse_while();
    std::unique_ptr<DemiStmt> parse_for();
    std::unique_ptr<DemiStmt> parse_return();
    std::unique_ptr<DemiStmt> parse_asm_block();
    std::unique_ptr<DemiStmt> parse_register_write();
    std::unique_ptr<DemiStmt> parse_expr_stmt();

    // Expressions (Pratt parser)
    std::unique_ptr<DemiExpr> parse_expression();
    std::unique_ptr<DemiExpr> parse_precedence(int min_precedence);
    std::unique_ptr<DemiExpr> parse_primary();
    std::unique_ptr<DemiExpr> parse_struct_literal(const std::string& name);

    // Pratt parser helpers
    struct ParseRule {
        std::function<std::unique_ptr<DemiExpr>()> prefix;
        std::function<std::unique_ptr<DemiExpr>(std::unique_ptr<DemiExpr>)> infix;
        int precedence;
    };
    ParseRule get_rule(TokenType type);
    
    // Precedence levels
    public:
    enum Precedence {
        PREC_NONE = 0,
        PREC_ASSIGNMENT = 1,
        PREC_OR = 2,
        PREC_AND = 3,
        PREC_EQUALITY = 4,
        PREC_COMPARISON = 5,
        PREC_BIT_OR = 6,
        PREC_BIT_XOR = 7,
        PREC_BIT_AND = 8,
        PREC_SHIFT = 9,
        PREC_TERM = 10,
        PREC_FACTOR = 11,
        PREC_UNARY = 12,
        PREC_POSTFIX = 13,
        PREC_PRIMARY = 14
    };
    private:
};

} // namespace DemiLanguage
