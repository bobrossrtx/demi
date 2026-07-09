#include "parser.hpp"
#include <functional>
#include <stdexcept>
#include <sstream>

namespace DemiLanguage {

Parser::Parser(std::vector<Token> tokens)
    : tokens_(std::move(tokens)), pos_(0) {}

// === Token navigation ===
const Token& Parser::peek() const { return tokens_[pos_]; }
const Token& Parser::peek_next() const { return tokens_[pos_ + 1]; }
const Token& Parser::advance() { return tokens_[pos_++]; }
const Token& Parser::previous() const { return tokens_[pos_ - 1]; }
bool Parser::is_at_end() const { return peek().type == TokenType::END_OF_FILE; }
bool Parser::check(TokenType type) const { return peek().type == type; }
bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}
bool Parser::match_any(std::initializer_list<TokenType> types) {
    for (auto t : types) if (check(t)) { advance(); return true; }
    return false;
}

Token Parser::consume(TokenType type, const std::string& error_msg) {
    if (check(type)) return advance();
    std::stringstream ss;
    ss << error_msg << " at line " << peek().line << ":" << peek().column
       << " (got '" << peek().lexeme << "')";
    errors_.push_back(ss.str());
    return peek(); // return current token so parsing can continue
}

void Parser::synchronize() {
    advance();
    while (!is_at_end()) {
        if (previous().type == TokenType::OP_SEMI) return;
        switch (peek().type) {
            case TokenType::KW_FN:
            case TokenType::KW_LET:
            case TokenType::KW_IF:
            case TokenType::KW_WHILE:
            case TokenType::KW_FOR:
            case TokenType::KW_RETURN:
            case TokenType::KW_STRUCT:
            case TokenType::KW_ENUM:
            case TokenType::KW_IMPORT:
            case TokenType::KW_PUB:
                return;
            default: break;
        }
        advance();
    }
}

// === Module ===

std::unique_ptr<DemiModule> Parser::parse() {
    auto mod = std::make_unique<DemiModule>();
    while (!is_at_end()) {
        try {
            if (match(TokenType::KW_IMPORT)) {
                parse_import(*mod);
            } else if (match(TokenType::KW_PUB)) {
                if (check(TokenType::KW_FN))
                    mod->functions.push_back(parse_function(true));
                else if (check(TokenType::KW_STRUCT))
                    mod->structs.push_back(parse_struct(true));
                else if (check(TokenType::KW_ENUM))
                    mod->enums.push_back(parse_enum(true));
                else {
                    errors_.push_back("Expected fn/struct/enum after pub");
                    synchronize();
                }
            } else if (check(TokenType::KW_FN)) {
                mod->functions.push_back(parse_function(false));
            } else if (check(TokenType::KW_STRUCT)) {
                mod->structs.push_back(parse_struct(false));
            } else if (check(TokenType::KW_ENUM)) {
                mod->enums.push_back(parse_enum(false));
            } else {
                errors_.push_back("Unexpected token: " + std::string(token_type_name(peek().type)));
                advance();
            }
        } catch (const std::runtime_error&) {
            synchronize();
        }
    }
    return mod;
}

void Parser::parse_import(DemiModule& mod) {
    Token str = consume(TokenType::LIT_STRING, "Expected string after import");
    mod.imports.push_back(str.string_value);
    consume(TokenType::OP_SEMI, "Expected ';' after import");
}

// === Functions ===

std::unique_ptr<DemiFunction> Parser::parse_function(bool is_pub) {
    consume(TokenType::KW_FN, "Expected 'fn'");
    Token name = consume(TokenType::IDENTIFIER, "Expected function name");
    consume(TokenType::DELIM_LPAREN, "Expected '(' after function name");
    
    auto func = std::make_unique<DemiFunction>();
    func->name = name.lexeme;
    func->is_public = is_pub;
    
    // Parameters
    if (!check(TokenType::DELIM_RPAREN)) {
        do {
            func->params.push_back(parse_param());
        } while (match(TokenType::OP_COMMA));
    }
    consume(TokenType::DELIM_RPAREN, "Expected ')' after parameters");
    
    // Return type
    if (match(TokenType::OP_ARROW)) {
        func->return_type = parse_type();
    } else {
        func->return_type = std::make_unique<DemiType>(TypeKind::Void);
    }
    
    func->body.push_back(parse_block());
    return func;
}

std::pair<std::string, std::unique_ptr<DemiType>> Parser::parse_param() {
    Token name = consume(TokenType::IDENTIFIER, "Expected parameter name");
    consume(TokenType::OP_COLON, "Expected ':' after parameter name");
    return {name.lexeme, parse_type()};
}

// === Types ===

std::unique_ptr<DemiType> Parser::parse_type() {
    std::string name;
    TypeKind kind = TypeKind::Primitive;
    
    if (match_any({TokenType::TYPE_I8, TokenType::TYPE_I16, TokenType::TYPE_I32, TokenType::TYPE_I64,
                   TokenType::TYPE_U8, TokenType::TYPE_U16, TokenType::TYPE_U32, TokenType::TYPE_U64,
                   TokenType::TYPE_F32, TokenType::TYPE_F64, TokenType::TYPE_BOOL, TokenType::TYPE_VOID,
                   TokenType::TYPE_STRING})) {
        name = previous().lexeme;
        if (name == "void") kind = TypeKind::Void;
        else if (name == "string") kind = TypeKind::Slice;
    } else {
        Token id = consume(TokenType::IDENTIFIER, "Expected type name");
        name = id.lexeme;
        kind = TypeKind::Struct; // Assume struct type — semantic pass resolves
    }
    
    auto type = std::make_unique<DemiType>(kind, name);
    return parse_type_suffix(std::move(type));
}

std::unique_ptr<DemiType> Parser::parse_type_suffix(std::unique_ptr<DemiType> base) {
    while (match(TokenType::OP_STAR)) {
        auto ptr = std::make_unique<DemiType>(TypeKind::Pointer);
        ptr->inner = std::move(base);
        base = std::move(ptr);
    }
    while (match(TokenType::DELIM_LBRACKET)) {
        if (match(TokenType::DELIM_RBRACKET)) {
            auto sl = std::make_unique<DemiType>(TypeKind::Slice);
            sl->inner = std::move(base);
            base = std::move(sl);
        } else {
            // Fixed-size array: T[N] — treat as pointer for now
            parse_expression(); // eat the size expression
            consume(TokenType::DELIM_RBRACKET, "Expected ']'");
            auto arr = std::make_unique<DemiType>(TypeKind::Array);
            arr->inner = std::move(base);
            base = std::move(arr);
        }
    }
    return base;
}

// === Struct ===

std::unique_ptr<DemiStruct> Parser::parse_struct(bool is_pub) {
    consume(TokenType::KW_STRUCT, "Expected 'struct'");
    Token name = consume(TokenType::IDENTIFIER, "Expected struct name");
    consume(TokenType::DELIM_LBRACE, "Expected '{' after struct name");
    
    auto st = std::make_unique<DemiStruct>();
    st->name = name.lexeme;
    st->is_public = is_pub;
    
    while (!check(TokenType::DELIM_RBRACE) && !is_at_end()) {
        Token field_name = consume(TokenType::IDENTIFIER, "Expected field name");
        consume(TokenType::OP_COLON, "Expected ':' after field name");
        auto field_type = parse_type();
        consume(TokenType::OP_SEMI, "Expected ';' after field type");
        st->fields.emplace_back(field_name.lexeme, std::move(field_type));
    }
    consume(TokenType::DELIM_RBRACE, "Expected '}' after struct body");
    return st;
}

// === Enum ===

std::unique_ptr<DemiEnum> Parser::parse_enum(bool is_pub) {
    consume(TokenType::KW_ENUM, "Expected 'enum'");
    Token name = consume(TokenType::IDENTIFIER, "Expected enum name");
    consume(TokenType::DELIM_LBRACE, "Expected '{' after enum name");
    
    auto en = std::make_unique<DemiEnum>();
    en->name = name.lexeme;
    en->is_public = is_pub;
    
    while (!check(TokenType::DELIM_RBRACE) && !is_at_end()) {
        Token var_name = consume(TokenType::IDENTIFIER, "Expected variant name");
        std::unique_ptr<DemiType> payload;
        if (match(TokenType::DELIM_LPAREN)) {
            payload = parse_type();
            consume(TokenType::DELIM_RPAREN, "Expected ')'");
        }
        en->variants.emplace_back(var_name.lexeme, std::move(payload));
        if (!check(TokenType::DELIM_RBRACE)) consume(TokenType::OP_COMMA, "Expected ',' between variants");
    }
    consume(TokenType::DELIM_RBRACE, "Expected '}' after enum body");
    return en;
}

// === Statements ===

std::unique_ptr<DemiStmt> Parser::parse_block() {
    consume(TokenType::DELIM_LBRACE, "Expected '{'");
    std::vector<std::unique_ptr<DemiStmt>> stmts;
    while (!check(TokenType::DELIM_RBRACE) && !is_at_end()) {
        stmts.push_back(parse_statement());
    }
    consume(TokenType::DELIM_RBRACE, "Expected '}'");
    return DemiStmt::make_block(std::move(stmts));
}

std::unique_ptr<DemiStmt> Parser::parse_statement() {
    if (match(TokenType::KW_LET)) return parse_let();
    if (match(TokenType::KW_IF)) return parse_if();
    if (match(TokenType::KW_WHILE)) return parse_while();
    if (match(TokenType::KW_FOR)) return parse_for();
    if (match(TokenType::KW_RETURN)) return parse_return();
    if (match(TokenType::KW_ASM)) return parse_asm_block();
    if (match(TokenType::KW_REGISTER)) return parse_register_write();
    return parse_expr_stmt();
}

std::unique_ptr<DemiStmt> Parser::parse_let() {
    bool is_mut = match(TokenType::KW_MUT);
    Token name = consume(TokenType::IDENTIFIER, "Expected variable name");
    std::unique_ptr<DemiType> type_annot;
    if (match(TokenType::OP_COLON)) type_annot = parse_type();
    consume(TokenType::OP_ASSIGN, "Expected '=' in variable declaration");
    auto init = parse_expression();
    consume(TokenType::OP_SEMI, "Expected ';'");
    return DemiStmt::make_let(name.lexeme, std::move(type_annot), std::move(init), is_mut);
}

std::unique_ptr<DemiStmt> Parser::parse_if() {
    auto cond = parse_expression();
    auto then_body = parse_block();
    
    auto stmt = std::make_unique<DemiStmt>();
    stmt->kind = StmtKind::If;
    stmt->for_cond = std::move(cond); // reuse for_cond for condition
    stmt->body.push_back(std::move(then_body));
    
    // else-if chain
    while (match(TokenType::KW_ELSE)) {
        if (match(TokenType::KW_IF)) {
            auto elif_cond = parse_expression();
            auto elif_body = parse_block();
            auto elif_stmt = std::make_unique<DemiStmt>();
            elif_stmt->kind = StmtKind::If;
            elif_stmt->for_cond = std::move(elif_cond);
            elif_stmt->body.push_back(std::move(elif_body));
            stmt->body.push_back(std::move(elif_stmt));
        } else {
            stmt->body.push_back(parse_block()); // else body, stored as extra block in body
            break;
        }
    }
    return stmt;
}

std::unique_ptr<DemiStmt> Parser::parse_while() {
    auto cond = parse_expression();
    auto body = parse_block();
    auto stmt = std::make_unique<DemiStmt>();
    stmt->kind = StmtKind::While;
    stmt->for_cond = std::move(cond);
    stmt->body.push_back(std::move(body));
    return stmt;
}

std::unique_ptr<DemiStmt> Parser::parse_for() {
    // for (let i=0; i<10; i=i+1) { ... } — C-style
    // for let item in iterable { ... } — for-in style
    
    if (match(TokenType::DELIM_LPAREN)) {
        // C-style for
        auto stmt = std::make_unique<DemiStmt>();
        stmt->kind = StmtKind::For;
        
        if (!check(TokenType::OP_SEMI)) stmt->for_init = parse_let();
        else consume(TokenType::OP_SEMI, "Expected ';'");
        
        if (!check(TokenType::OP_SEMI)) stmt->for_cond = parse_expression();
        consume(TokenType::OP_SEMI, "Expected ';' after for condition");
        
        if (!check(TokenType::DELIM_RPAREN)) {
            // step is an assignment or expression
            auto step = parse_expression();
            auto step_stmt = DemiStmt::make_expr(std::move(step));
            stmt->for_step = std::move(step_stmt);
        }
        consume(TokenType::DELIM_RPAREN, "Expected ')'");
        stmt->body.push_back(parse_block());
        return stmt;
    } else {
        // for-in: for let item in expr { ... }
        consume(TokenType::KW_LET, "Expected 'let' after 'for'");
        Token var = consume(TokenType::IDENTIFIER, "Expected variable name");
        consume(TokenType::KW_IN, "Expected 'in' in for-in loop");
        auto iterable = parse_expression();
        
        auto stmt = std::make_unique<DemiStmt>();
        stmt->kind = StmtKind::ForIn;
        stmt->forin_var = var.lexeme;
        stmt->for_cond = std::move(iterable);
        stmt->body.push_back(parse_block());
        return stmt;
    }
}

std::unique_ptr<DemiStmt> Parser::parse_return() {
    std::unique_ptr<DemiExpr> expr;
    if (!check(TokenType::OP_SEMI)) expr = parse_expression();
    consume(TokenType::OP_SEMI, "Expected ';' after return");
    return DemiStmt::make_return(std::move(expr));
}

std::unique_ptr<DemiStmt> Parser::parse_asm_block() {
    consume(TokenType::DELIM_LBRACE, "Expected '{' after 'asm'");
    // Collect raw tokens between { and }
    std::string code;
    int depth = 1;
    while (!is_at_end() && depth > 0) {
        if (check(TokenType::DELIM_LBRACE)) depth++;
        if (check(TokenType::DELIM_RBRACE)) { depth--; if (depth == 0) break; }
        code += advance().lexeme + " ";
    }
    consume(TokenType::DELIM_RBRACE, "Expected '}' after asm block");
    auto stmt = std::make_unique<DemiStmt>();
    stmt->kind = StmtKind::AsmBlock;
    stmt->asm_code = code;
    return stmt;
}

std::unique_ptr<DemiStmt> Parser::parse_register_write() {
    Token reg = consume(TokenType::IDENTIFIER, "Expected register name");
    consume(TokenType::OP_ASSIGN, "Expected '=' for register write");
    auto value = parse_expression();
    consume(TokenType::OP_SEMI, "Expected ';' after register write");
    auto stmt = std::make_unique<DemiStmt>();
    stmt->kind = StmtKind::RegisterWrite;
    stmt->var_name = reg.lexeme;
    stmt->var_init = std::move(value);
    return stmt;
}

std::unique_ptr<DemiStmt> Parser::parse_expr_stmt() {
    auto expr = parse_expression();
    
    // Check for assignment
    if (match(TokenType::OP_ASSIGN)) {
        auto value = parse_expression();
        auto stmt = std::make_unique<DemiStmt>();
        stmt->kind = StmtKind::Assign;
        stmt->assign_target = std::move(expr);
        stmt->assign_value = std::move(value);
        consume(TokenType::OP_SEMI, "Expected ';'");
        return stmt;
    }
    
    consume(TokenType::OP_SEMI, "Expected ';' after expression");
    return DemiStmt::make_expr(std::move(expr));
}

// === Expressions (Pratt parser) ===

std::unique_ptr<DemiExpr> Parser::parse_expression() {
    return parse_precedence(PREC_ASSIGNMENT);
}

std::unique_ptr<DemiExpr> Parser::parse_precedence(int min_prec) {
    auto token = peek();  // don't advance — prefix function handles it
    auto prefix_fn = get_rule(token.type).prefix;
    if (!prefix_fn) {
        errors_.push_back("Expected expression at line " + std::to_string(token.line));
        advance(); // skip the unexpected token
        return DemiExpr::make_literal_int(0, "0");
    }
    
    auto left = prefix_fn();
    
    
    
    while (min_prec <= get_rule(peek().type).precedence) {
        token = advance();
        auto infix_fn = get_rule(token.type).infix;
        if (infix_fn) left = infix_fn(std::move(left));
    }
    
    return left;
}

std::unique_ptr<DemiExpr> Parser::parse_primary() {
    Token token = advance();
        switch (token.type) {
        case TokenType::LIT_INTEGER: return DemiExpr::make_literal_int(token.int_value, token.lexeme);
        case TokenType::LIT_FLOAT:   return DemiExpr::make_literal_float(token.float_value, token.lexeme);
        case TokenType::LIT_STRING:  return DemiExpr::make_literal_string(token.string_value);
        case TokenType::LIT_CHAR:    return DemiExpr::make_literal_char(token.char_value);
        case TokenType::KW_TRUE:     return DemiExpr::make_literal_bool(true);
        case TokenType::KW_FALSE:    return DemiExpr::make_literal_bool(false);
        case TokenType::KW_NULL:     return DemiExpr::make_literal_int(0, "null");
        case TokenType::IDENTIFIER: {
            // Check for struct literal: Name { field: val, ... }
            if (check(TokenType::DELIM_LBRACE)) {
                return parse_struct_literal(token.lexeme);
            }
            return DemiExpr::make_identifier(token.lexeme);
        }
        case TokenType::DELIM_LPAREN: {
            auto expr = parse_expression();
            consume(TokenType::DELIM_RPAREN, "Expected ')'");
            return expr;
        }
        case TokenType::OP_AT: {
            // Intrinsic: @cpuid, @rdtsc, etc.
            Token name = consume(TokenType::IDENTIFIER, "Expected intrinsic name");
            auto expr = DemiExpr::make_identifier("@" + name.lexeme);
            expr->kind = DemiExpr::Kind::Call;
            return expr;
        }
        case TokenType::KW_REGISTER: {
            Token reg = consume(TokenType::IDENTIFIER, "Expected register name");
            auto expr = std::make_unique<DemiExpr>();
            expr->kind = DemiExpr::Kind::RegisterRead;
            expr->literal = reg.lexeme;
            return expr;
        }
        default:
            errors_.push_back("Unexpected token in expression: " + std::string(token_type_name(token.type)));
            return DemiExpr::make_literal_int(0, "0");
    }
}

std::unique_ptr<DemiExpr> Parser::parse_struct_literal(const std::string& name) {
    auto expr = std::make_unique<DemiExpr>();
    expr->kind = DemiExpr::Kind::StructLiteral;
    expr->struct_name = name;
    
    consume(TokenType::DELIM_LBRACE, "Expected '{'");
    while (!check(TokenType::DELIM_RBRACE) && !is_at_end()) {
        Token field = consume(TokenType::IDENTIFIER, "Expected field name");
        consume(TokenType::OP_COLON, "Expected ':' after field name");
        auto value = parse_expression();
        expr->fields.emplace_back(field.lexeme, std::move(value));
        if (!check(TokenType::DELIM_RBRACE)) consume(TokenType::OP_COMMA, "Expected ',' between fields");
    }
    consume(TokenType::DELIM_RBRACE, "Expected '}'");
    return expr;
}

// === Infix parsers ===

static BinaryOp token_to_binary_op(TokenType type) {
    switch (type) {
        case TokenType::OP_PLUS: return BinaryOp::Add;
        case TokenType::OP_MINUS: return BinaryOp::Sub;
        case TokenType::OP_STAR: return BinaryOp::Mul;
        case TokenType::OP_SLASH: return BinaryOp::Div;
        case TokenType::OP_PERCENT: return BinaryOp::Mod;
        case TokenType::OP_EQ: return BinaryOp::Eq;
        case TokenType::OP_NE: return BinaryOp::Ne;
        case TokenType::OP_LT: return BinaryOp::Lt;
        case TokenType::OP_GT: return BinaryOp::Gt;
        case TokenType::OP_LE: return BinaryOp::Le;
        case TokenType::OP_GE: return BinaryOp::Ge;
        case TokenType::OP_AND: return BinaryOp::And;
        case TokenType::OP_OR: return BinaryOp::Or;
        case TokenType::OP_BIT_AND: return BinaryOp::BitAnd;
        case TokenType::OP_BIT_OR: return BinaryOp::BitOr;
        case TokenType::OP_BIT_XOR: return BinaryOp::BitXor;
        case TokenType::OP_SHL: return BinaryOp::Shl;
        case TokenType::OP_SHR: return BinaryOp::Shr;
        default: return BinaryOp::Add;
    }
}

static int binary_precedence(TokenType type) {
    switch (type) {
        case TokenType::OP_ASSIGN: return Parser::PREC_ASSIGNMENT;
        case TokenType::OP_OR: return Parser::PREC_OR;
        case TokenType::OP_AND: return Parser::PREC_AND;
        case TokenType::OP_EQ: case TokenType::OP_NE: return Parser::PREC_EQUALITY;
        case TokenType::OP_LT: case TokenType::OP_GT:
        case TokenType::OP_LE: case TokenType::OP_GE: return Parser::PREC_COMPARISON;
        case TokenType::OP_BIT_OR: return Parser::PREC_BIT_OR;
        case TokenType::OP_BIT_XOR: return Parser::PREC_BIT_XOR;
        case TokenType::OP_BIT_AND: return Parser::PREC_BIT_AND;
        case TokenType::OP_SHL: case TokenType::OP_SHR: return Parser::PREC_SHIFT;
        case TokenType::OP_PLUS: case TokenType::OP_MINUS: return Parser::PREC_TERM;
        case TokenType::OP_STAR: case TokenType::OP_SLASH: case TokenType::OP_PERCENT: return Parser::PREC_FACTOR;
        default: return Parser::PREC_NONE;
    }
}

Parser::ParseRule Parser::get_rule(TokenType type) {
    ParseRule rule{nullptr, nullptr, PREC_NONE};
    
    // === Prefix rules ===
    switch (type) {
        case TokenType::LIT_INTEGER: case TokenType::LIT_FLOAT:
        case TokenType::LIT_STRING: case TokenType::LIT_CHAR:
        case TokenType::KW_TRUE: case TokenType::KW_FALSE: case TokenType::KW_NULL:
        case TokenType::IDENTIFIER: case TokenType::DELIM_LPAREN:
        case TokenType::OP_AT: case TokenType::KW_REGISTER:
            rule.prefix = [this]() { return parse_primary(); };
            break;
        case TokenType::OP_MINUS:
            rule.prefix = [this]() {
                auto e = std::make_unique<DemiExpr>();
                e->kind = DemiExpr::Kind::Unary; e->unary_op = UnaryOp::Neg;
                e->operand = parse_precedence(PREC_UNARY);
                return e;
            };
            break;
        case TokenType::OP_NOT:
            rule.prefix = [this]() {
                auto e = std::make_unique<DemiExpr>();
                e->kind = DemiExpr::Kind::Unary; e->unary_op = UnaryOp::Not;
                e->operand = parse_precedence(PREC_UNARY);
                return e;
            };
            break;
        case TokenType::OP_BIT_AND:
            rule.prefix = [this]() {
                auto e = std::make_unique<DemiExpr>();
                e->kind = DemiExpr::Kind::Unary; e->unary_op = UnaryOp::Ref;
                e->operand = parse_precedence(PREC_UNARY);
                return e;
            };
            break;
        case TokenType::OP_STAR:
            rule.prefix = [this]() {
                auto e = std::make_unique<DemiExpr>();
                e->kind = DemiExpr::Kind::Unary; e->unary_op = UnaryOp::Deref;
                e->operand = parse_precedence(PREC_UNARY);
                return e;
            };
            break;
        default: break;
    }
    
    // === Infix rules ===
    switch (type) {
        case TokenType::OP_PLUS: case TokenType::OP_MINUS:
        case TokenType::OP_STAR: case TokenType::OP_SLASH: case TokenType::OP_PERCENT:
        case TokenType::OP_EQ: case TokenType::OP_NE:
        case TokenType::OP_LT: case TokenType::OP_GT:
        case TokenType::OP_LE: case TokenType::OP_GE:
        case TokenType::OP_AND: case TokenType::OP_OR:
        case TokenType::OP_BIT_AND: case TokenType::OP_BIT_OR: case TokenType::OP_BIT_XOR:
        case TokenType::OP_SHL: case TokenType::OP_SHR:
            rule.infix = [this, type](std::unique_ptr<DemiExpr> left) {
                auto right = parse_precedence(binary_precedence(type) + 1);
                return DemiExpr::make_binary(token_to_binary_op(type), std::move(left), std::move(right));
            };
            rule.precedence = binary_precedence(type);
            break;
        case TokenType::DELIM_LPAREN:
            rule.infix = [this](std::unique_ptr<DemiExpr> callee) {
                std::vector<std::unique_ptr<DemiExpr>> args;
                if (!check(TokenType::DELIM_RPAREN)) {
                    do { args.push_back(parse_expression()); }
                    while (match(TokenType::OP_COMMA));
                }
                consume(TokenType::DELIM_RPAREN, "Expected ')'");
                return DemiExpr::make_call(std::move(callee), std::move(args));
            };
            rule.precedence = PREC_POSTFIX;
            break;
        case TokenType::DELIM_LBRACKET:
            rule.infix = [this](std::unique_ptr<DemiExpr> arr) {
                auto index = parse_expression();
                consume(TokenType::DELIM_RBRACKET, "Expected ']'");
                auto e = std::make_unique<DemiExpr>();
                e->kind = DemiExpr::Kind::Index;
                e->left = std::move(arr); e->right = std::move(index);
                return e;
            };
            rule.precedence = PREC_POSTFIX;
            break;
        case TokenType::OP_DOT:
            rule.infix = [this](std::unique_ptr<DemiExpr> obj) {
                // Allow keywords as member names (e.g., sys.alloc, sys.free)
                Token member = peek();
                bool is_id = (member.type == TokenType::IDENTIFIER);
                bool is_kw = (static_cast<int>(member.type) >= static_cast<int>(TokenType::KW_FN) &&
                              static_cast<int>(member.type) <= static_cast<int>(TokenType::KW_AS));
                if (is_id || is_kw) {
                    advance();
                } else {
                    errors_.push_back("Expected member name after '.'");
                    return obj;
                }
                auto e = std::make_unique<DemiExpr>();
                e->kind = DemiExpr::Kind::Member;
                e->left = std::move(obj); e->literal = member.lexeme;
                return e;
            };
            rule.precedence = PREC_POSTFIX;
            break;
        default: break;
    }
    
    return rule;
}

} // namespace DemiLanguage
