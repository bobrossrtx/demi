#pragma once
#include "../token.hpp"
#include <memory>
#include <string>
#include <vector>
#include <variant>

namespace DemiLanguage {

// Forward declarations
struct DemiExpr;
struct DemiStmt;
struct DemiType;
struct DemiFunction;
struct DemiModule;

// === Types ===

enum class TypeKind { Primitive, Pointer, Array, Slice, Struct, Enum, Void };

struct DemiType {
    TypeKind kind;
    std::string name;
    std::unique_ptr<DemiType> inner;

    explicit DemiType(TypeKind k = TypeKind::Primitive, std::string n = "")
        : kind(k), name(std::move(n)) {}
    
    // Copy constructor (deep copy inner pointer)
    DemiType(const DemiType& other)
        : kind(other.kind), name(other.name),
          inner(other.inner ? std::make_unique<DemiType>(*other.inner) : nullptr) {}
    
    // Copy assignment
    DemiType& operator=(const DemiType& other) {
        if (this != &other) {
            kind = other.kind;
            name = other.name;
            inner = other.inner ? std::make_unique<DemiType>(*other.inner) : nullptr;
        }
        return *this;
    }
    
    // Move constructor and assignment (default)
    DemiType(DemiType&&) = default;
    DemiType& operator=(DemiType&&) = default;
};

// === Expressions ===

enum class BinaryOp {
    Add, Sub, Mul, Div, Mod,
    Eq, Ne, Lt, Gt, Le, Ge,
    And, Or,
    BitAnd, BitOr, BitXor,
    Shl, Shr
};

enum class UnaryOp { Neg, Not, Ref, Deref, Intrinsic };

struct DemiExpr {
    enum class Kind {
        LiteralInt, LiteralFloat, LiteralString, LiteralChar,
        LiteralBool, LiteralNull,
        Identifier, Binary, Unary, Call, Index,
        Member, StructLiteral, FieldInit, Cast,
        RegisterRead
    };

    Kind kind;
    std::unique_ptr<DemiType> expr_type; // inferred by semantic analysis

    // LiteralInt
    std::string literal;  // raw text
    int64_t int_value = 0;
    double float_value = 0.0;
    bool bool_value = false;

    // Binary
    BinaryOp bin_op;
    std::unique_ptr<DemiExpr> left;
    std::unique_ptr<DemiExpr> right;

    // Unary
    UnaryOp unary_op;
    std::unique_ptr<DemiExpr> operand;

    // Call/Index/Member
    std::vector<std::unique_ptr<DemiExpr>> args;

    // StructLiteral
    std::string struct_name;
    std::vector<std::pair<std::string, std::unique_ptr<DemiExpr>>> fields;

    DemiExpr() = default;

    static std::unique_ptr<DemiExpr> make_literal_int(int64_t v, const std::string& raw) {
        auto e = std::make_unique<DemiExpr>();
        e->kind = Kind::LiteralInt; e->int_value = v; e->literal = raw;
        return e;
    }
    static std::unique_ptr<DemiExpr> make_literal_float(double v, const std::string& raw) {
        auto e = std::make_unique<DemiExpr>();
        e->kind = Kind::LiteralFloat; e->float_value = v; e->literal = raw;
        return e;
    }
    static std::unique_ptr<DemiExpr> make_literal_string(const std::string& v) {
        auto e = std::make_unique<DemiExpr>();
        e->kind = Kind::LiteralString; e->literal = v;
        return e;
    }
    static std::unique_ptr<DemiExpr> make_literal_char(char v) {
        auto e = std::make_unique<DemiExpr>();
        e->kind = Kind::LiteralChar; e->int_value = static_cast<int64_t>(static_cast<unsigned char>(v));
        return e;
    }
    static std::unique_ptr<DemiExpr> make_literal_bool(bool v) {
        auto e = std::make_unique<DemiExpr>();
        e->kind = Kind::LiteralBool; e->bool_value = v;
        return e;
    }
    static std::unique_ptr<DemiExpr> make_identifier(const std::string& name) {
        auto e = std::make_unique<DemiExpr>();
        e->kind = Kind::Identifier; e->literal = name;
        return e;
    }
    static std::unique_ptr<DemiExpr> make_binary(BinaryOp op, std::unique_ptr<DemiExpr> l, std::unique_ptr<DemiExpr> r) {
        auto e = std::make_unique<DemiExpr>();
        e->kind = Kind::Binary; e->bin_op = op; e->left = std::move(l); e->right = std::move(r);
        return e;
    }
    static std::unique_ptr<DemiExpr> make_call(std::unique_ptr<DemiExpr> callee, std::vector<std::unique_ptr<DemiExpr>> a) {
        auto e = std::make_unique<DemiExpr>();
        e->kind = Kind::Call; e->left = std::move(callee); e->args = std::move(a);
        return e;
    }
};

// === Statements ===

enum class StmtKind {
    Let, Assign, If, While, For, ForIn, Return, Expr, Block, AsmBlock, RegisterWrite
};

struct DemiStmt {
    StmtKind kind;

    // Let: name, type_annot, initializer, is_mut
    std::string var_name;
    std::unique_ptr<DemiType> var_type;
    std::unique_ptr<DemiExpr> var_init;
    bool is_mut = false;

    // If: condition, then_body, else_ifs (condition+body pairs), else_body
    std::vector<std::unique_ptr<DemiStmt>> body; // for Block, If branches, While body, etc.

    // Return
    std::unique_ptr<DemiExpr> return_expr;

    // Assign: target, value
    std::unique_ptr<DemiExpr> assign_target;
    std::unique_ptr<DemiExpr> assign_value;

    // For: init, cond, step
    std::unique_ptr<DemiStmt> for_init;
    std::unique_ptr<DemiExpr> for_cond;
    std::unique_ptr<DemiStmt> for_step;

    // ForIn: iterator name, iterable expr
    std::string forin_var;

    // Asm: raw tokens or string
    std::string asm_code;

    DemiStmt() = default;

    static std::unique_ptr<DemiStmt> make_let(std::string name, std::unique_ptr<DemiType> ty,
                                               std::unique_ptr<DemiExpr> init, bool mut) {
        auto s = std::make_unique<DemiStmt>();
        s->kind = StmtKind::Let; s->var_name = std::move(name);
        s->var_type = std::move(ty); s->var_init = std::move(init); s->is_mut = mut;
        return s;
    }
    static std::unique_ptr<DemiStmt> make_return(std::unique_ptr<DemiExpr> expr) {
        auto s = std::make_unique<DemiStmt>();
        s->kind = StmtKind::Return; s->return_expr = std::move(expr);
        return s;
    }
    static std::unique_ptr<DemiStmt> make_block(std::vector<std::unique_ptr<DemiStmt>> stmts) {
        auto s = std::make_unique<DemiStmt>();
        s->kind = StmtKind::Block; s->body = std::move(stmts);
        return s;
    }
    static std::unique_ptr<DemiStmt> make_expr(std::unique_ptr<DemiExpr> e) {
        auto s = std::make_unique<DemiStmt>();
        s->kind = StmtKind::Expr; s->return_expr = std::move(e);
        return s;
    }
};

// === Functions ===

struct DemiFunction {
    std::string name;
    std::vector<std::pair<std::string, std::unique_ptr<DemiType>>> params;
    std::unique_ptr<DemiType> return_type;
    std::vector<std::unique_ptr<DemiStmt>> body;
    bool is_public = false;
};

// === Struct ===

struct DemiStruct {
    std::string name;
    std::vector<std::pair<std::string, std::unique_ptr<DemiType>>> fields;
    bool is_public = false;
};

// === Enum ===

struct DemiEnum {
    std::string name;
    std::vector<std::pair<std::string, std::unique_ptr<DemiType>>> variants; // name + optional payload type
    bool is_public = false;
};

// === Module ===

struct DemiModule {
    std::string name;
    std::vector<std::string> imports;
    std::vector<std::unique_ptr<DemiFunction>> functions;
    std::vector<std::unique_ptr<DemiStruct>> structs;
    std::vector<std::unique_ptr<DemiEnum>> enums;
};

} // namespace DemiLanguage
