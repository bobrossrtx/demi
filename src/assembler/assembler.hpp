#pragma once
#include "ast.hpp"
#include <vector>
#include <unordered_map>
#include <string>

#include <cstdint>
#include "../config.hpp"

namespace Assembler {

struct Symbol {
    std::string name;
    uint32_t address;
    bool defined;
    bool is_global = false;
    std::string section = ".text";
    
    Symbol() : name(""), address(0), defined(false) {}  // Default constructor
    explicit Symbol(const std::string& n, uint32_t addr = 0, bool def = false)
        : name(n), address(addr), defined(def) {}
};

class AssemblerEngine {
    static uint32_t db_next_addr;
public:
    AssemblerEngine();
    ~AssemblerEngine();
    
    // Main assembly function
    std::vector<uint8_t> assemble(const Program& program);
    
    // Error handling
    const std::vector<std::string>& get_errors() const { return errors; }
    bool has_errors() const { return !errors.empty(); }
    
    // Get symbol table for debugging
    const std::unordered_map<std::string, Symbol>& get_symbols() const { return symbol_table; }
    uint32_t get_entry_address() const { return entry_address; }
    size_t get_memory_size() const { return memory_size; } // Get memory size set by .memory directive
    
    // Set entry point symbol (default: "_start")
    void set_entry_point_symbol(const std::string& symbol) { entry_point_symbol = symbol; }

    // Get bytecode-to-source-line mapping (for DWARF debug info)
    const std::vector<std::pair<size_t, uint32_t>>& get_line_map() const { return bytecode_line_map; }

    // Get listing output (call after assemble when listing is enabled)
    void write_listing(const std::string& source, const std::string& filename) const;

    // Architecture detection
    Architecture detect_architecture(const Program& program);

private:
    std::vector<std::string> errors;
    std::unordered_map<std::string, Symbol> symbol_table;
    std::unordered_map<std::string, uint8_t> mnemonic_to_opcode;
    std::unordered_map<std::string, uint8_t> register_to_number;
    std::unordered_map<std::string, uint8_t> register_sizes; // Map register name to size in bits (32 or 64)
    
    // Current assembly state
    uint32_t current_address;
    std::vector<uint8_t> bytecode;
    std::vector<std::pair<size_t, uint32_t>> bytecode_line_map; // (offset, source_line)
    uint32_t entry_address = 0; // Entry point for execution
    std::string entry_point_symbol = "_start"; // Entry point symbol name
    std::string last_label_name; // Track most recent label for DB assignment
    size_t memory_size = 0; // Memory size set by .memory directive (0 = use default)
    std::string current_section = ".text"; // Current section name
    std::string previous_section;          // For .previous directive
    bool in_struct = false;               // Inside .struct/.endstruct block
    uint32_t struct_offset = 0;           // Current offset within struct
    
    // Section base addresses for memory layout
    uint32_t data_section_base = 0x100;  // .data starts at 0x100 (fixed)
    uint32_t text_section_base = 0;      // .text base (calculated dynamically)
    uint32_t data_section_size = 0;      // Track .data section size
    uint32_t text_section_size = 0;      // Track .text section size
    bool has_explicit_sections = false;   // Track if .data/.text used
    uint32_t section_alignment = 0x100;  // Align sections to 256-byte boundaries
    
    // DB format detection
    bool use_simple_db_format = false; // True for multiple DBs or with .org
    
    // One-time directive tracking (directives that can only be used once)
    bool memory_directive_used = false;     // .memory
    bool trace_directive_used = false;      // .trace
    
    // Forward reference tracking
    struct ForwardRef {
        uint32_t address;      // Where to patch
        std::string symbol;    // Symbol name
        size_t size;          // Size of address field (1, 2, 4 bytes)
        bool relative;        // Is it a relative address?
    };
    std::vector<ForwardRef> forward_refs;
    
    // Assembly passes
    void first_pass(const Program& program);   // Collect symbols
    void second_pass(const Program& program);  // Generate code
    void resolve_forward_references();
    
    // Statement processing
    void process_label(const Label& label);
    void process_directive(const Directive& directive);
    void process_instruction(const Instruction& instruction);
    
    // Expression evaluation
    int64_t evaluate_expression(const Expression& expr, bool& is_symbol_ref, std::string& symbol_name);
    uint8_t get_register_number(const std::string& reg_name);
    uint8_t get_opcode(const std::string& mnemonic);
    
    // Code generation helpers
    void emit_byte(uint8_t byte);
    void emit_word(uint16_t word);
    void emit_dword(uint32_t dword);
    void emit_qword(uint64_t qword);
    void emit_address(uint32_t address, size_t size = 4);
    void emit_mode_aware_address(int64_t address);  // Emit address based on current architecture mode
    void emit_forward_ref(const std::string& symbol, size_t size = 4, bool relative = false);
    
    // Architecture helpers
    size_t get_address_size() const;  // Returns 4 for x86, 8 for x64
    
    // Instruction encoding
    void encode_instruction(const Instruction& instruction);
    size_t get_instruction_size(const std::string& mnemonic, const std::vector<std::unique_ptr<Expression>>& operands);

    // Encoder function type and map (mnemonic -> handler)
    using EncoderFunc = void (AssemblerEngine::*)(const Instruction&, uint8_t);
    std::unordered_map<std::string, EncoderFunc> encoder_map_;
    void init_encoder_map();

    // Encoder handler methods (one per opcode group)
    void enc_debug(const Instruction& instr, uint8_t opcode);
    void enc_load_imm(const Instruction& instr, uint8_t opcode);
    void enc_load_imm64(const Instruction& instr, uint8_t opcode);
    void enc_mov(const Instruction& instr, uint8_t opcode);
    void enc_alu_2reg(const Instruction& instr, uint8_t opcode);
    void enc_alu_3reg(const Instruction& instr, uint8_t opcode);
    void enc_mode(const Instruction& instr, uint8_t opcode);
    void enc_jump(const Instruction& instr, uint8_t opcode);
    void enc_single_reg(const Instruction& instr, uint8_t opcode);
    void enc_io(const Instruction& instr, uint8_t opcode);
    void enc_load_store(const Instruction& instr, uint8_t opcode);
    void enc_loadex_storex(const Instruction& instr, uint8_t opcode);
    void enc_shift(const Instruction& instr, uint8_t opcode);
    void enc_fpu_ldst(const Instruction& instr, uint8_t opcode);
    void enc_fpu_arith(const Instruction& instr, uint8_t opcode);
    void enc_fild(const Instruction& instr, uint8_t opcode);
    void enc_fist(const Instruction& instr, uint8_t opcode);
    void enc_fpu_ctrl(const Instruction& instr, uint8_t opcode);
    void enc_fstsw(const Instruction& instr, uint8_t opcode);
    void enc_sse_cmp(const Instruction& instr, uint8_t opcode);
    void enc_sse(const Instruction& instr, uint8_t opcode);
    void enc_no_operands(const Instruction& instr, uint8_t opcode);
    void enc_int(const Instruction& instr, uint8_t opcode);
    
    // Data directive handling
    void handle_db_directive(const std::vector<std::unique_ptr<Expression>>& args);
    void handle_dw_directive(const std::vector<std::unique_ptr<Expression>>& args);
    void handle_dd_directive(const std::vector<std::unique_ptr<Expression>>& args);
    void handle_string_directive(const std::vector<std::unique_ptr<Expression>>& args);
    void handle_align_directive(const std::vector<std::unique_ptr<Expression>>& args);
    void handle_bss_directive(const std::vector<std::unique_ptr<Expression>>& args);
    void handle_org_directive(const std::vector<std::unique_ptr<Expression>>& args);
    void handle_equ_directive(const std::vector<std::unique_ptr<Expression>>& args);
    void handle_memory_directive(const std::vector<std::unique_ptr<Expression>>& args);

    // Level 4 directive handlers
    void handle_skip_directive(const std::vector<std::unique_ptr<Expression>>& args);
    void handle_fill_directive(const std::vector<std::unique_ptr<Expression>>& args);
    void handle_p2align_directive(const std::vector<std::unique_ptr<Expression>>& args);
    void handle_previous_directive(const std::vector<std::unique_ptr<Expression>>& args);
    void handle_struct_directive(const std::vector<std::unique_ptr<Expression>>& args);
    void handle_endstruct_directive(const std::vector<std::unique_ptr<Expression>>& args);

    // Structure directive handling
    void handle_section_directive(const std::vector<std::unique_ptr<Expression>>& args);
    void handle_global_directive(const std::vector<std::unique_ptr<Expression>>& args);
    void handle_extern_directive(const std::vector<std::unique_ptr<Expression>>& args);

    void handle_data_section();
    void handle_text_section();
    // Utility methods
    void add_error(const std::string& message);
    void add_error(const std::string& message, size_t line, size_t column);
    void init_opcode_table();
    void init_register_table();
    bool is_simple_register_indirect(const MemoryReferenceExpression& mem) const;
    bool resolve_memory_operand_address(const MemoryReferenceExpression& mem, int64_t& address, bool& is_symbol_ref, std::string& symbol_name);
    
    // Bracket syntax detection helper (static for use in both passes)
    static bool is_bracket_register_syntax(const Expression* operand);
};

} // namespace Assembler
