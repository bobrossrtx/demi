#include "lexer.hpp"
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace DemiLanguage {

Lexer::Lexer(std::string source)
    : source_(std::move(source)), pos_(0), line_(1), column_(1) {
    init_keywords();
}

void Lexer::init_keywords() {
    keywords_["fn"]       = TokenType::KW_FN;
    keywords_["let"]      = TokenType::KW_LET;
    keywords_["mut"]      = TokenType::KW_MUT;
    keywords_["if"]       = TokenType::KW_IF;
    keywords_["else"]     = TokenType::KW_ELSE;
    keywords_["while"]    = TokenType::KW_WHILE;
    keywords_["for"]      = TokenType::KW_FOR;
    keywords_["in"]       = TokenType::KW_IN;
    keywords_["return"]   = TokenType::KW_RETURN;
    keywords_["struct"]   = TokenType::KW_STRUCT;
    keywords_["enum"]     = TokenType::KW_ENUM;
    keywords_["match"]    = TokenType::KW_MATCH;
    keywords_["import"]   = TokenType::KW_IMPORT;
    keywords_["pub"]      = TokenType::KW_PUB;
    keywords_["asm"]      = TokenType::KW_ASM;
    keywords_["register"] = TokenType::KW_REGISTER;
    keywords_["extern"]   = TokenType::KW_EXTERN;
    keywords_["true"]     = TokenType::KW_TRUE;
    keywords_["false"]    = TokenType::KW_FALSE;
    keywords_["sizeof"]   = TokenType::KW_SIZEOF;
    keywords_["alloc"]    = TokenType::KW_ALLOC;
    keywords_["free"]     = TokenType::KW_FREE;
    keywords_["null"]     = TokenType::KW_NULL;
    keywords_["as"]       = TokenType::KW_AS;

    keywords_["i8"]       = TokenType::TYPE_I8;
    keywords_["i16"]      = TokenType::TYPE_I16;
    keywords_["i32"]      = TokenType::TYPE_I32;
    keywords_["i64"]      = TokenType::TYPE_I64;
    keywords_["u8"]       = TokenType::TYPE_U8;
    keywords_["u16"]      = TokenType::TYPE_U16;
    keywords_["u32"]      = TokenType::TYPE_U32;
    keywords_["u64"]      = TokenType::TYPE_U64;
    keywords_["f32"]      = TokenType::TYPE_F32;
    keywords_["f64"]      = TokenType::TYPE_F64;
    keywords_["bool"]     = TokenType::TYPE_BOOL;
    keywords_["void"]     = TokenType::TYPE_VOID;
    keywords_["string"]   = TokenType::TYPE_STRING;
}

// === Character navigation ===

char Lexer::peek(size_t ahead) const {
    if (pos_ + ahead >= source_.size()) return '\0';
    return source_[pos_ + ahead];
}

char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') { line_++; column_ = 1; }
    else { column_++; }
    return c;
}

bool Lexer::is_at_end() const {
    return pos_ >= source_.size();
}

bool Lexer::match(char expected) {
    if (is_at_end() || source_[pos_] != expected) return false;
    pos_++;
    column_++;
    return true;
}

void Lexer::skip_whitespace_and_comments() {
    while (!is_at_end()) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                advance();
                break;
            case '/':
                if (peek(1) == '/') {
                    // Line comment: skip until newline
                    while (!is_at_end() && peek() != '\n') advance();
                } else if (peek(1) == '*') {
                    // Block comment: skip until */
                    advance(); advance(); // skip /*
                    while (!is_at_end()) {
                        if (peek() == '*' && peek(1) == '/') {
                            advance(); advance(); // skip */
                            break;
                        }
                        advance();
                    }
                } else {
                    return; // division operator, not a comment
                }
                break;
            default:
                return;
        }
    }
}

// === Main tokenizer loop ===

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (!is_at_end()) {
        skip_whitespace_and_comments();
        if (is_at_end()) break;

        char c = peek();
        Token token = (is_alpha(c) || c == '_') ? scan_identifier_or_keyword()
                    : is_digit(c)                 ? scan_number()
                    : c == '\''                   ? scan_char_literal()
                    : c == '"'                    ? scan_string_literal()
                    : c == '@'                    ? (advance(), make_token(TokenType::OP_AT))
                    :                               scan_operator();

        if (token.type == TokenType::ERROR) {
            errors_.push_back(token.lexeme);
        }
        tokens.push_back(std::move(token));
    }
    tokens.emplace_back(TokenType::END_OF_FILE, "", line_, column_);
    return tokens;
}

// === Individual token scanners ===

Token Lexer::scan_identifier_or_keyword() {
    size_t start_col = column_;
    std::string lexeme;
    while (!is_at_end() && (is_alphanumeric(peek()) || peek() == '_')) {
        lexeme += advance();
    }

    auto it = keywords_.find(lexeme);
    if (it != keywords_.end()) {
        return Token(it->second, lexeme, line_, start_col);
    }
    return Token(TokenType::IDENTIFIER, lexeme, line_, start_col);
}

Token Lexer::scan_number() {
    size_t start_col = column_;
    std::string lexeme;
    bool is_float = false;
    int base = 10;

    // Check for hex/octal/binary prefix
    if (peek() == '0') {
        lexeme += advance();
        if (match('x') || match('X')) {
            lexeme += 'x'; base = 16;
        } else if (match('o') || match('O')) {
            lexeme += 'o'; base = 8;
        } else if (match('b') || match('B')) {
            lexeme += 'b'; base = 2;
        }
    }

    while (!is_at_end()) {
        char c = peek();
        if (base == 16 && is_hex_digit(c)) { lexeme += advance(); }
        else if (base == 10 && is_digit(c)) { lexeme += advance(); }
        else if (base == 8 && c >= '0' && c <= '7') { lexeme += advance(); }
        else if (base == 2 && (c == '0' || c == '1')) { lexeme += advance(); }
        else break;
    }

    // Fractional part
    if (peek() == '.' && base == 10 && is_digit(peek(1))) {
        is_float = true;
        lexeme += advance(); // '.'
        while (!is_at_end() && is_digit(peek())) lexeme += advance();
    }

    // Exponent
    if ((peek() == 'e' || peek() == 'E') && base == 10) {
        is_float = true;
        lexeme += advance();
        if (peek() == '+' || peek() == '-') lexeme += advance();
        while (!is_at_end() && is_digit(peek())) lexeme += advance();
    }

    // Type suffix (i32, u64, f64)
    std::string suffix;
    while (!is_at_end() && is_alpha(peek())) suffix += advance();
    lexeme += suffix;  // include suffix in lexeme for now

    if (is_float || !suffix.empty()) {
        double fv = std::strtod(lexeme.c_str(), nullptr);
        return Token(TokenType::LIT_FLOAT, lexeme, line_, start_col, fv);
    }

    // Parse integer value — handle non-standard prefixes
    const char* num_str = lexeme.c_str();
    int num_base = base;
    if (num_base == 8)  { num_str += 2; }  // skip "0o"
    if (num_base == 2)  { num_str += 2; }  // skip "0b"
    
    int64_t iv = std::strtoll(num_str, nullptr, num_base);
    return Token(TokenType::LIT_INTEGER, lexeme, line_, start_col, iv);
}

Token Lexer::scan_char_literal() {
    size_t start_col = column_;
    advance(); // opening '
    char value;
    if (peek() == '\\') {
        advance(); // backslash
        switch (peek()) {
            case 'n': value = '\n'; break;
            case 't': value = '\t'; break;
            case 'r': value = '\r'; break;
            case '\\': value = '\\'; break;
            case '\'': value = '\''; break;
            case '0': value = '\0'; break;
            default: value = peek(); break;
        }
        advance();
    } else {
        value = peek();
        advance();
    }
    if (!match('\'')) {
        return make_error("Unterminated character literal");
    }
    return Token(TokenType::LIT_CHAR, std::string(1, value), line_, start_col, value);
}

Token Lexer::scan_string_literal() {
    size_t start_col = column_;
    advance(); // opening "
    std::string value;
    while (!is_at_end() && peek() != '"') {
        if (peek() == '\\') {
            advance();
            switch (peek()) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                case '0': value += '\0'; break;
                default: value += peek(); break;
            }
            advance();
        } else {
            value += advance();
        }
    }
    if (!match('"')) {
        return make_error("Unterminated string literal");
    }
    Token tok(TokenType::LIT_STRING, value, line_, start_col);
    tok.string_value = value;
    return tok;
}

Token Lexer::scan_operator() {
    size_t start_col = column_;
    std::string lexeme;
    lexeme += advance();

    TokenType type;
    switch (lexeme[0]) {
        case '+': type = match('=') ? (lexeme+="=", TokenType::OP_PLUS_ASSIGN) : TokenType::OP_PLUS; break;
        case '-': type = match('>') ? (lexeme+=">", TokenType::OP_ARROW)
                       : match('=') ? (lexeme+="=", TokenType::OP_MINUS_ASSIGN) : TokenType::OP_MINUS; break;
        case '*': type = match('=') ? (lexeme+="=", TokenType::OP_STAR_ASSIGN) : TokenType::OP_STAR; break;
        case '/': type = match('=') ? (lexeme+="=", TokenType::OP_SLASH_ASSIGN) : TokenType::OP_SLASH; break;
        case '%': type = match('=') ? (lexeme+="=", TokenType::OP_PERCENT_ASSIGN) : TokenType::OP_PERCENT; break;
        case '=': type = match('=') ? (lexeme+="=", TokenType::OP_EQ) : TokenType::OP_ASSIGN; break;
        case '!': type = match('=') ? (lexeme+="=", TokenType::OP_NE) : TokenType::OP_NOT; break;
        case '<': type = match('<') ? (lexeme+="<", TokenType::OP_SHL)
                       : match('=') ? (lexeme+="=", TokenType::OP_LE) : TokenType::OP_LT; break;
        case '>': type = match('>') ? (lexeme+=">", TokenType::OP_SHR)
                       : match('=') ? (lexeme+="=", TokenType::OP_GE) : TokenType::OP_GT; break;
        case '&': type = match('&') ? (lexeme+="&", TokenType::OP_AND) : TokenType::OP_BIT_AND; break;
        case '|': type = match('|') ? (lexeme+="|", TokenType::OP_OR) : TokenType::OP_BIT_OR; break;
        case '^': type = TokenType::OP_BIT_XOR; break;
        case '~': type = TokenType::OP_BIT_NOT; break;
        case '.': type = match('.') ? (lexeme+=".", TokenType::OP_DOTDOT) : TokenType::OP_DOT; break;
        case ':': type = TokenType::OP_COLON; break;
        case ';': type = TokenType::OP_SEMI; break;
        case ',': type = TokenType::OP_COMMA; break;
        case '(': type = TokenType::DELIM_LPAREN; break;
        case ')': type = TokenType::DELIM_RPAREN; break;
        case '{': type = TokenType::DELIM_LBRACE; break;
        case '}': type = TokenType::DELIM_RBRACE; break;
        case '[': type = TokenType::DELIM_LBRACKET; break;
        case ']': type = TokenType::DELIM_RBRACKET; break;
        default:
            return make_error("Unexpected character: '" + lexeme + "'");
    }
    return Token(type, lexeme, line_, start_col);
}

Token Lexer::make_token(TokenType type) {
    return Token(type, "", line_, column_);
}

Token Lexer::make_error(const std::string& message) {
    return Token(TokenType::ERROR, message, line_, column_);
}

// === Helpers ===

bool Lexer::is_digit(char c) { return c >= '0' && c <= '9'; }
bool Lexer::is_hex_digit(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
bool Lexer::is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
bool Lexer::is_alphanumeric(char c) {
    return is_alpha(c) || is_digit(c);
}

const char* token_type_name(TokenType type) {
    switch (type) {
        case TokenType::KW_FN: return "fn";
        case TokenType::KW_LET: return "let";
        case TokenType::KW_MUT: return "mut";
        case TokenType::KW_IF: return "if";
        case TokenType::KW_ELSE: return "else";
        case TokenType::KW_WHILE: return "while";
        case TokenType::KW_FOR: return "for";
        case TokenType::KW_IN: return "in";
        case TokenType::KW_RETURN: return "return";
        case TokenType::KW_STRUCT: return "struct";
        case TokenType::KW_ENUM: return "enum";
        case TokenType::KW_MATCH: return "match";
        case TokenType::KW_IMPORT: return "import";
        case TokenType::KW_PUB: return "pub";
        case TokenType::KW_ASM: return "asm";
        case TokenType::KW_REGISTER: return "register";
        case TokenType::KW_EXTERN: return "extern";
        case TokenType::KW_TRUE: return "true";
        case TokenType::KW_FALSE: return "false";
        case TokenType::KW_SIZEOF: return "sizeof";
        case TokenType::KW_ALLOC: return "alloc";
        case TokenType::KW_FREE: return "free";
        case TokenType::KW_NULL: return "null";
        case TokenType::KW_AS: return "as";
        case TokenType::TYPE_I8: return "i8";
        case TokenType::TYPE_I16: return "i16";
        case TokenType::TYPE_I32: return "i32";
        case TokenType::TYPE_I64: return "i64";
        case TokenType::TYPE_U8: return "u8";
        case TokenType::TYPE_U16: return "u16";
        case TokenType::TYPE_U32: return "u32";
        case TokenType::TYPE_U64: return "u64";
        case TokenType::TYPE_F32: return "f32";
        case TokenType::TYPE_F64: return "f64";
        case TokenType::TYPE_BOOL: return "bool";
        case TokenType::TYPE_VOID: return "void";
        case TokenType::TYPE_STRING: return "string";
        case TokenType::LIT_INTEGER: return "integer";
        case TokenType::LIT_FLOAT: return "float";
        case TokenType::LIT_CHAR: return "char";
        case TokenType::LIT_STRING: return "string";
        case TokenType::IDENTIFIER: return "identifier";
        case TokenType::OP_PLUS: return "+";
        case TokenType::OP_MINUS: return "-";
        case TokenType::OP_STAR: return "*";
        case TokenType::OP_SLASH: return "/";
        case TokenType::OP_EQ: return "==";
        case TokenType::OP_NE: return "!=";
        case TokenType::OP_LT: return "<";
        case TokenType::OP_GT: return ">";
        case TokenType::OP_LE: return "<=";
        case TokenType::OP_GE: return ">=";
        case TokenType::OP_AND: return "&&";
        case TokenType::OP_OR: return "||";
        case TokenType::OP_NOT: return "!";
        case TokenType::OP_ARROW: return "->";
        case TokenType::OP_DOT: return ".";
        case TokenType::OP_DOTDOT: return "..";
        case TokenType::OP_COLON: return ":";
        case TokenType::OP_SEMI: return ";";
        case TokenType::OP_COMMA: return ",";
        case TokenType::OP_AT: return "@";
        case TokenType::DELIM_LBRACE: return "{";
        case TokenType::DELIM_RBRACE: return "}";
        case TokenType::DELIM_LPAREN: return "(";
        case TokenType::DELIM_RPAREN: return ")";
        case TokenType::DELIM_LBRACKET: return "[";
        case TokenType::DELIM_RBRACKET: return "]";
        case TokenType::END_OF_FILE: return "EOF";
        case TokenType::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

} // namespace DemiLanguage
