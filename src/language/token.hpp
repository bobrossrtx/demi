#pragma once
#include <cstddef>
#include <string>
#include <cstdint>
#include <variant>

namespace DemiLanguage {

enum class TokenType {
    // Keywords
    KW_FN,
    KW_LET,
    KW_MUT,
    KW_IF,
    KW_ELSE,
    KW_WHILE,
    KW_FOR,
    KW_IN,
    KW_RETURN,
    KW_STRUCT,
    KW_ENUM,
    KW_MATCH,
    KW_IMPORT,
    KW_PUB,
    KW_ASM,
    KW_REGISTER,
    KW_EXTERN,
    KW_TRUE,
    KW_FALSE,
    KW_SIZEOF,
    KW_ALLOC,
    KW_FREE,
    KW_NULL,
    KW_AS,         // type cast

    // Type keywords
    TYPE_I8,
    TYPE_I16,
    TYPE_I32,
    TYPE_I64,
    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_U64,
    TYPE_F32,
    TYPE_F64,
    TYPE_BOOL,
    TYPE_VOID,
    TYPE_STRING,

    // Literals
    LIT_INTEGER,
    LIT_FLOAT,
    LIT_CHAR,
    LIT_STRING,

    // Identifier
    IDENTIFIER,

    // Operators
    OP_PLUS,          // +
    OP_MINUS,         // -
    OP_STAR,          // *
    OP_SLASH,         // /
    OP_PERCENT,       // %
    OP_EQ,            // ==
    OP_NE,            // !=
    OP_LT,            // <
    OP_GT,            // >
    OP_LE,            // <=
    OP_GE,            // >=
    OP_AND,           // &&
    OP_OR,            // ||
    OP_NOT,           // !
    OP_BIT_AND,       // &
    OP_BIT_OR,        // |
    OP_BIT_XOR,       // ^
    OP_BIT_NOT,       // ~
    OP_SHL,           // <<
    OP_SHR,           // >>
    OP_ASSIGN,        // =
    OP_PLUS_ASSIGN,   // +=
    OP_MINUS_ASSIGN,  // -=
    OP_STAR_ASSIGN,   // *=
    OP_SLASH_ASSIGN,  // /=
    OP_PERCENT_ASSIGN,// %=
    OP_ARROW,         // ->
    OP_DOT,           // .
    OP_DOTDOT,        // ..
    OP_COLON,         // :
    OP_SEMI,          // ;
    OP_COMMA,         // ,
    OP_AT,            // @

    // Delimiters
    DELIM_LBRACE,     // {
    DELIM_RBRACE,     // }
    DELIM_LPAREN,     // (
    DELIM_RPAREN,     // )
    DELIM_LBRACKET,   // [
    DELIM_RBRACKET,   // ]
    DELIM_SINGLE_QUOTE,// '

    // Special
    END_OF_FILE,
    ERROR,
};

struct Token {
    TokenType type;
    std::string lexeme;      // raw text
    size_t line;
    size_t column;

    // Literal values (use std::variant or store as string)
    union {
        int64_t int_value;
        double float_value;
        char char_value;
    };
    std::string string_value; // for LIT_STRING

    Token(TokenType t, std::string lex, size_t ln, size_t col)
        : type(t), lexeme(std::move(lex)), line(ln), column(col), int_value(0) {}

    Token(TokenType t, std::string lex, size_t ln, size_t col, int64_t iv)
        : type(t), lexeme(std::move(lex)), line(ln), column(col), int_value(iv) {}

    Token(TokenType t, std::string lex, size_t ln, size_t col, double fv)
        : type(t), lexeme(std::move(lex)), line(ln), column(col), float_value(fv) {}

    Token(TokenType t, std::string lex, size_t ln, size_t col, char cv)
        : type(t), lexeme(std::move(lex)), line(ln), column(col), char_value(cv) {}
};

// Convert token type to string for debugging
const char* token_type_name(TokenType type);

} // namespace DemiLanguage
