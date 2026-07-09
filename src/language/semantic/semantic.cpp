#include "semantic.hpp"
#include <sstream>

namespace DemiLanguage {

// === SymbolTable ===

void SymbolTable::push_scope() {
    scopes_.push_back({});
}

void SymbolTable::pop_scope() {
    if (scopes_.size() > 1) scopes_.pop_back();
}

bool SymbolTable::declare(const std::string& name, Symbol sym) {
    if (exists_in_current_scope(name)) return false;
    sym.name = name;
    scopes_.back()[name] = std::move(sym);
    return true;
}

Symbol* SymbolTable::lookup(const std::string& name) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return &found->second;
    }
    return nullptr;
}

bool SymbolTable::exists(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        if (it->find(name) != it->end()) return true;
    }
    return false;
}

bool SymbolTable::exists_in_current_scope(const std::string& name) const {
    return scopes_.back().find(name) != scopes_.back().end();
}

Symbol* SymbolTable::lookup_module_level(const std::string& name) {
    if (!scopes_.empty()) {
        auto found = scopes_[0].find(name);
        if (found != scopes_[0].end()) return &found->second;
    }
    return nullptr;
}

// === Builtins ===

void populate_builtins(SymbolTable& symbols) {
    // Primitive types as "functions" for type resolution
    auto declare_type = [&](const std::string& name, TypeKind kind) {
        Symbol s;
        s.kind = SymbolKind::Struct; // types are struct-like
        s.name = name;
        s.type = std::make_unique<DemiType>(kind, name);
        symbols.declare(name, std::move(s));
    };
    
    declare_type("i8",   TypeKind::Primitive);
    declare_type("i16",  TypeKind::Primitive);
    declare_type("i32",  TypeKind::Primitive);
    declare_type("i64",  TypeKind::Primitive);
    declare_type("u8",   TypeKind::Primitive);
    declare_type("u16",  TypeKind::Primitive);
    declare_type("u32",  TypeKind::Primitive);
    declare_type("u64",  TypeKind::Primitive);
    declare_type("f32",  TypeKind::Primitive);
    declare_type("f64",  TypeKind::Primitive);
    declare_type("bool", TypeKind::Primitive);
    declare_type("void", TypeKind::Void);
    declare_type("string", TypeKind::Slice);
}

// === SemanticAnalyzer ===

SemanticAnalyzer::SemanticAnalyzer() {
    populate_builtins(symbols_);
}

bool SemanticAnalyzer::analyze(DemiModule& module) {
    module_ = &module;
    errors_.clear();
    
    declare_module_symbols(module);
    
    for (auto& func : module.functions) {
        analyze_function(*func);
    }
    
    return !has_errors();
}

void SemanticAnalyzer::error(size_t line, size_t col, const std::string& msg) {
    std::stringstream ss;
    ss << "[" << line << ":" << col << "] " << msg;
    errors_.push_back({ss.str(), line, col});
}

// === Module-level declarations ===

void SemanticAnalyzer::declare_module_symbols(DemiModule& mod) {
    // Declare structs
    for (auto& st : mod.structs) {
        Symbol s;
        s.kind = SymbolKind::Struct;
        s.name = st->name;
        s.is_public = st->is_public;
        s.type = std::make_unique<DemiType>(TypeKind::Struct, st->name);
        if (!symbols_.declare(st->name, std::move(s))) {
            error(0, 0, "Duplicate struct: " + st->name);
        }
    }
    
    // Declare enums
    for (auto& en : mod.enums) {
        Symbol s;
        s.kind = SymbolKind::Enum;
        s.name = en->name;
        s.is_public = en->is_public;
        symbols_.declare(en->name, std::move(s));
    }
    
    // Declare function signatures (for forward references)
    for (auto& func : mod.functions) {
        Symbol s;
        s.kind = SymbolKind::Function;
        s.name = func->name;
        s.is_public = func->is_public;
        for (auto& p : func->params) {
            s.params.emplace_back(p.first, std::make_unique<DemiType>(*p.second));
        }
        if (func->return_type) {
            s.return_type = std::make_unique<DemiType>(*func->return_type);
        } else {
            s.return_type = std::make_unique<DemiType>(TypeKind::Void);
        }
        if (!symbols_.declare(func->name, std::move(s))) {
            error(0, 0, "Duplicate function: " + func->name);
        }
    }
}

// === Per-function analysis ===

void SemanticAnalyzer::analyze_function(DemiFunction& func) {
    current_function_ = &func;
    symbols_.push_scope();
    
    // Declare parameters
    for (auto& p : func.params) {
        Symbol s;
        s.kind = SymbolKind::Parameter;
        s.name = p.first;
        s.type = std::make_unique<DemiType>(*p.second);
        s.is_mutable = false;
        symbols_.declare(p.first, std::move(s));
    }
    
    // Analyze body
    analyze_block(func.body);
    
    symbols_.pop_scope();
    current_function_ = nullptr;
}

void SemanticAnalyzer::analyze_block(std::vector<std::unique_ptr<DemiStmt>>& stmts) {
    symbols_.push_scope();
    for (auto& stmt : stmts) {
        analyze_stmt(*stmt);
    }
    symbols_.pop_scope();
}

void SemanticAnalyzer::analyze_stmt(DemiStmt& stmt) {
    switch (stmt.kind) {
        case StmtKind::Let: analyze_let(stmt); break;
        case StmtKind::If: analyze_if(stmt); break;
        case StmtKind::While: analyze_while(stmt); break;
        case StmtKind::For: analyze_for(stmt); break;
        case StmtKind::Return: analyze_return(stmt); break;
        case StmtKind::Block: analyze_block(stmt.body); break;
        case StmtKind::Expr:
            if (stmt.return_expr) analyze_expr(*stmt.return_expr);
            break;
        case StmtKind::RegisterWrite: analyze_register_write(stmt); break;
        case StmtKind::Assign:
            if (stmt.assign_target) analyze_expr(*stmt.assign_target);
            if (stmt.assign_value) {
                DemiType val_type = analyze_expr(*stmt.assign_value);
                stmt.assign_target->expr_type = std::make_unique<DemiType>(val_type);
            }
            break;
        default: break;
    }
}

void SemanticAnalyzer::analyze_let(DemiStmt& stmt) {
    DemiType init_type = analyze_expr(*stmt.var_init);
    
    DemiType var_type;
    if (stmt.var_type) {
        var_type = *stmt.var_type;
        if (!types_compatible(var_type, init_type)) {
            error(0, 0, "Type mismatch in variable '" + stmt.var_name +
                  "': declared " + var_type.name + " but initialized with " + init_type.name);
        }
    } else {
        var_type = init_type;
    }
    
    Symbol s;
    s.kind = SymbolKind::Variable;
    s.name = stmt.var_name;
    s.type = std::make_unique<DemiType>(var_type);
    s.is_mutable = stmt.is_mut;
    
    if (!symbols_.declare(stmt.var_name, std::move(s))) {
        error(0, 0, "Variable '" + stmt.var_name + "' already declared in this scope");
    }
    
    stmt.var_init->expr_type = std::make_unique<DemiType>(var_type);
}

void SemanticAnalyzer::analyze_if(DemiStmt& stmt) {
    if (stmt.for_cond) analyze_expr(*stmt.for_cond);
    for (auto& b : stmt.body) analyze_stmt(*b);
}

void SemanticAnalyzer::analyze_while(DemiStmt& stmt) {
    if (stmt.for_cond) analyze_expr(*stmt.for_cond);
    for (auto& b : stmt.body) analyze_stmt(*b);
}

void SemanticAnalyzer::analyze_for(DemiStmt& stmt) {
    symbols_.push_scope();
    if (stmt.for_init) analyze_stmt(*stmt.for_init);
    if (stmt.for_cond) analyze_expr(*stmt.for_cond);
    if (stmt.for_step) analyze_stmt(*stmt.for_step);
    for (auto& b : stmt.body) analyze_stmt(*b);
    symbols_.pop_scope();
}

void SemanticAnalyzer::analyze_return(DemiStmt& stmt) {
    DemiType return_type(TypeKind::Void);
    if (stmt.return_expr) {
        return_type = analyze_expr(*stmt.return_expr);
    }
    
    if (current_function_ && current_function_->return_type) {
        DemiType expected = *current_function_->return_type;
        if (expected.kind != TypeKind::Void && !types_compatible(expected, return_type)) {
            error(0, 0, "Return type mismatch: expected " + expected.name +
                  " but got " + return_type.name);
        }
    }
}

void SemanticAnalyzer::analyze_register_write(DemiStmt& stmt) {
    if (stmt.var_init) {
        analyze_expr(*stmt.var_init);
    }
}

// === Expression analysis ===

DemiType SemanticAnalyzer::analyze_expr(DemiExpr& expr) {
    switch (expr.kind) {
        case DemiExpr::Kind::LiteralInt:
        case DemiExpr::Kind::LiteralFloat:
        case DemiExpr::Kind::LiteralString:
        case DemiExpr::Kind::LiteralChar:
        case DemiExpr::Kind::LiteralBool:
            return analyze_literal(expr);
        
        case DemiExpr::Kind::Identifier: {
            Symbol* sym = symbols_.lookup(expr.literal);
            if (!sym) {
                error(0, 0, "Undefined variable: " + expr.literal);
                return DemiType(TypeKind::Primitive, "i32");
            }
            if (sym->type) {
                expr.expr_type = std::make_unique<DemiType>(*sym->type);
                return *sym->type;
            }
            return DemiType(TypeKind::Primitive, "i32");
        }
        
        case DemiExpr::Kind::Binary:
            return analyze_binary(expr);
        
        case DemiExpr::Kind::Call:
            return analyze_call(expr);
        
        case DemiExpr::Kind::Unary:
            if (expr.operand) {
                DemiType t = analyze_expr(*expr.operand);
                if (expr.unary_op == UnaryOp::Not) return DemiType(TypeKind::Primitive, "bool");
                return t;
            }
            return DemiType(TypeKind::Primitive, "i32");
        
        case DemiExpr::Kind::RegisterRead:
            // Register reads are always i64
            return DemiType(TypeKind::Primitive, "i64");
        
        default:
            return DemiType(TypeKind::Primitive, "i32"); // fallback
    }
}

DemiType SemanticAnalyzer::analyze_literal(DemiExpr& expr) {
    DemiType t;
    switch (expr.kind) {
        case DemiExpr::Kind::LiteralInt:
            t = DemiType(TypeKind::Primitive, "i32");
            break;
        case DemiExpr::Kind::LiteralFloat:
            t = DemiType(TypeKind::Primitive, "f64");
            break;
        case DemiExpr::Kind::LiteralString:
            t = DemiType(TypeKind::Slice, "string");
            break;
        case DemiExpr::Kind::LiteralChar:
            t = DemiType(TypeKind::Primitive, "u8");
            break;
        case DemiExpr::Kind::LiteralBool:
            t = DemiType(TypeKind::Primitive, "bool");
            break;
        default:
            t = DemiType(TypeKind::Primitive, "i32");
    }
    expr.expr_type = std::make_unique<DemiType>(t);
    return t;
}

DemiType SemanticAnalyzer::analyze_binary(DemiExpr& expr) {
    DemiType left_t = analyze_expr(*expr.left);
    DemiType right_t = analyze_expr(*expr.right);
    
    DemiType result;
    switch (expr.bin_op) {
        case BinaryOp::Add: case BinaryOp::Sub:
        case BinaryOp::Mul: case BinaryOp::Div: case BinaryOp::Mod:
            result = common_type(left_t, right_t);
            break;
        case BinaryOp::Eq: case BinaryOp::Ne:
        case BinaryOp::Lt: case BinaryOp::Gt:
        case BinaryOp::Le: case BinaryOp::Ge:
            result = DemiType(TypeKind::Primitive, "bool");
            break;
        case BinaryOp::And: case BinaryOp::Or:
            result = DemiType(TypeKind::Primitive, "bool");
            break;
        case BinaryOp::BitAnd: case BinaryOp::BitOr: case BinaryOp::BitXor:
        case BinaryOp::Shl: case BinaryOp::Shr:
            result = common_type(left_t, right_t);
            break;
    }
    
    expr.expr_type = std::make_unique<DemiType>(result);
    return result;
}

DemiType SemanticAnalyzer::analyze_call(DemiExpr& expr) {
    if (!expr.left) {
        error(0, 0, "Invalid function call target");
        return DemiType(TypeKind::Primitive, "i32");
    }
    
    // Method calls (e.g. console.println) — skip type checking for now
    if (expr.left->kind == DemiExpr::Kind::Member) {
        for (auto& arg : expr.args) analyze_expr(*arg);
        return DemiType(TypeKind::Void);
    }
    
    if (expr.left->kind != DemiExpr::Kind::Identifier) {
        error(0, 0, "Invalid function call target");
        return DemiType(TypeKind::Primitive, "i32");
    }
    
    std::string func_name = expr.left->literal;
    Symbol* sym = symbols_.lookup(func_name);
    if (!sym || sym->kind != SymbolKind::Function) {
        error(0, 0, "Undefined function: " + func_name);
        return DemiType(TypeKind::Primitive, "i32");
    }
    
    // Analyze arguments
    for (auto& arg : expr.args) {
        analyze_expr(*arg);
    }
    
    if (sym->return_type) {
        expr.expr_type = std::make_unique<DemiType>(*sym->return_type);
        return *sym->return_type;
    }
    return DemiType(TypeKind::Void);
}

// === Type helpers ===

DemiType SemanticAnalyzer::type_from_typename(const std::string& name) {
    Symbol* sym = symbols_.lookup(name);
    if (sym && sym->type) return *sym->type;
    return DemiType(TypeKind::Primitive, "i32");
}

bool SemanticAnalyzer::types_compatible(const DemiType& a, const DemiType& b) {
    if (a.kind == TypeKind::Void || b.kind == TypeKind::Void) return true;
    if (a.kind == b.kind && a.name == b.name) return true;
    // Numeric promotion: allow any arithmetic → arithmetic
    if (is_arithmetic(a) && is_arithmetic(b)) return true;
    return false;
}

DemiType SemanticAnalyzer::common_type(const DemiType& a, const DemiType& b) {
    // For arithmetic, prefer the wider type
    if (a.name == "f64" || b.name == "f64") return DemiType(TypeKind::Primitive, "f64");
    if (a.name == "f32" || b.name == "f32") return DemiType(TypeKind::Primitive, "f32");
    if (a.name == "i64" || b.name == "i64") return DemiType(TypeKind::Primitive, "i64");
    if (a.name == "u64" || b.name == "u64") return DemiType(TypeKind::Primitive, "u64");
    return DemiType(TypeKind::Primitive, "i32");
}

bool SemanticAnalyzer::is_arithmetic(const DemiType& t) {
    return t.kind == TypeKind::Primitive && 
           (t.name == "i8" || t.name == "i16" || t.name == "i32" || t.name == "i64" ||
            t.name == "u8" || t.name == "u16" || t.name == "u32" || t.name == "u64" ||
            t.name == "f32" || t.name == "f64");
}

bool SemanticAnalyzer::is_comparable(const DemiType& t) {
    return is_arithmetic(t) || t.name == "bool";
}

} // namespace DemiLanguage
