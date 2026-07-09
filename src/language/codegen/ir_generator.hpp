#pragma once
#include "../ast/demi_ast.hpp"
#include "../semantic/semantic.hpp"
#include "../../assembler/ir.hpp"
#include <vector>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace DemiLanguage {

// Generates Demi VM bytecode + IRProgram from semantically-analyzed DemiAST.
// Produces both raw bytecode (for VM execution) and IRProgram (for DASM pipeline).
struct CodegenResult {
    std::vector<uint8_t> bytecode;        // raw Demi VM bytecode
    std::string entry_label;              // entry point label name
    std::unordered_map<std::string, uint32_t> symbol_offsets; // label → bytecode offset
};

class IRGenerator {
public:
    IRGenerator();
    
    // Generate code from a verified module (call SemanticAnalyzer first)
    CodegenResult generate(const DemiModule& module);
    const std::vector<std::string>& get_errors() const { return errors_; }

private:
    std::vector<uint8_t> code_;
    std::unordered_map<std::string, uint32_t> symbols_;
    std::vector<std::string> errors_;
    
    // Register allocator state
    int next_local_reg_ = 8;  // R8+ for local variables
    std::unordered_map<std::string, int> var_regs_;  // variable name → register index

    // String literal pool: index → (offset_in_code, string_data)
    std::vector<std::pair<uint32_t, std::string>> string_pool_;

    // String heap counter — advances per string literal (starts at 0xC0)
    int string_heap_ = 0xC0;
    
    // Current function context
    std::string current_func_;
    uint32_t func_start_offset_ = 0;
    
    // Emit helpers
    void emit_byte(uint8_t b);
    void emit_opcode(uint8_t op);
    void emit_u32(uint32_t val);
    void emit_load_imm(int reg, uint32_t val);
    void emit_label(const std::string& name);
    void patch_jump(uint32_t jump_offset, uint32_t target_offset);
    
    // Sub-generators
    void generate_function(const DemiFunction& func);
    void generate_stmt(const DemiStmt& stmt);
    void generate_expr(const DemiExpr& expr, int dest_reg);
    
    // Allocate a register for a local variable
    int alloc_local(const std::string& name);
    int get_var_reg(const std::string& name);
};

} // namespace DemiLanguage
