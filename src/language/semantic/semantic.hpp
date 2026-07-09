#pragma once
#include "../ast/demi_ast.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

namespace DemiLanguage {

enum class SymbolKind { Variable, Function, Struct, Enum, Parameter };

struct Symbol {
    SymbolKind kind;
    std::string name;
    std::unique_ptr<DemiType> type;
    bool is_mutable = false;
    bool is_public = false;
    // For functions: parameter types, return type
    std::vector<std::pair<std::string, std::unique_ptr<DemiType>>> params;
    std::unique_ptr<DemiType> return_type;
};

class SymbolTable {
public:
    SymbolTable() { push_scope(); } // module scope
    
    void push_scope();
    void pop_scope();
    
    // Declare a symbol in the current scope
    bool declare(const std::string& name, Symbol sym);
    
    // Look up a symbol from innermost scope outward
    Symbol* lookup(const std::string& name);
    bool exists(const std::string& name) const;
    bool exists_in_current_scope(const std::string& name) const;
    
    // For module-level lookups (skip local scopes)
    Symbol* lookup_module_level(const std::string& name);
    
    size_t scope_depth() const { return scopes_.size(); }

private:
    std::vector<std::unordered_map<std::string, Symbol>> scopes_;
};

// Built-in types and functions
void populate_builtins(SymbolTable& symbols);

// === Semantic Analyzer ===

struct SemanticError {
    std::string message;
    size_t line;
    size_t column;
};

class SemanticAnalyzer {
public:
    SemanticAnalyzer();
    
    bool analyze(DemiModule& module);
    const std::vector<SemanticError>& get_errors() const { return errors_; }
    bool has_errors() const { return !errors_.empty(); }
    
private:
    SymbolTable symbols_;
    DemiModule* module_ = nullptr;
    DemiFunction* current_function_ = nullptr;
    std::vector<SemanticError> errors_;
    
    void error(size_t line, size_t col, const std::string& msg);
    
    // Module-level pass: declare structs, enums, function signatures
    void declare_module_symbols(DemiModule& mod);
    
    // Per-function pass: type-check body
    void analyze_function(DemiFunction& func);
    
    // Statement analysis
    void analyze_stmt(DemiStmt& stmt);
    void analyze_let(DemiStmt& stmt);
    void analyze_if(DemiStmt& stmt);
    void analyze_while(DemiStmt& stmt);
    void analyze_for(DemiStmt& stmt);
    void analyze_return(DemiStmt& stmt);
    void analyze_block(std::vector<std::unique_ptr<DemiStmt>>& stmts);
    void analyze_register_write(DemiStmt& stmt);
    
    // Expression analysis — returns the inferred type
    DemiType analyze_expr(DemiExpr& expr);
    DemiType analyze_binary(DemiExpr& expr);
    DemiType analyze_call(DemiExpr& expr);
    DemiType analyze_literal(DemiExpr& expr);
    
    // Type helpers
    DemiType type_from_typename(const std::string& name);
    DemiType resolve_type(const DemiType& t);
    bool types_compatible(const DemiType& a, const DemiType& b);
    DemiType common_type(const DemiType& a, const DemiType& b);
    bool is_arithmetic(const DemiType& t);
    bool is_comparable(const DemiType& t);
};

} // namespace DemiLanguage
