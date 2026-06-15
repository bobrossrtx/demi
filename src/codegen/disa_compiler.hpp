#pragma once
#include <vector>
#include <unordered_map>
#include "x86_encoder.hpp"
#include "register_allocator.hpp"
#include "../engine/cpu.hpp"

namespace CodeGen {

// Translates D-ISA bytecode to native x86-64 machine code
class DISAToX86Compiler {
private:
    static constexpr int32_t MAX_SAVED_REGISTER_BYTES = 96;
    static constexpr int32_t SPILL_SLOT_SIZE = 8;
    static constexpr int32_t SPILL_FRAME_SIZE = MAX_SAVED_REGISTER_BYTES +
                                                static_cast<int32_t>(TOTAL_REGISTERS * SPILL_SLOT_SIZE);

    X86Encoder encoder;
    RegisterAllocator reg_alloc;

    // Jump target resolution
    struct JumpTarget {
        size_t bytecode_address;
        X86Encoder::Label x86_label;
    };
    std::unordered_map<size_t, JumpTarget> jump_targets;

    // Function call management
    std::vector<size_t> function_addresses;
    std::unordered_map<size_t, X86Encoder::Label> function_labels;

    // Current compilation state
    size_t current_bytecode_pos;
    const std::vector<uint8_t>* current_program;

    // Per-virtual-register state tracking
    struct VirtualRegState {
        X86Register phys;
        bool loaded;
        bool dirty;
    };
    std::unordered_map<uint8_t, VirtualRegState> reg_state_map;
    std::unordered_map<uint8_t, int32_t> spill_slots;
    std::unordered_map<uint8_t, bool> slot_contains_valid;
    bool function_has_calls = false;

public:
    DISAToX86Compiler() : current_bytecode_pos(0) {}

    // Main compilation interface
    // bytecode_line_map: optional mapping of bytecode offsets to source lines (for DWARF)
    std::vector<uint8_t> compile_program(const std::vector<uint8_t>& disa_bytecode,
                                         uint32_t entry_point = 0,
                                         const std::vector<std::pair<size_t, uint32_t>>* bytecode_line_map = nullptr,
                                         bool is_64bit = true);

    // Get line entries generated during compilation (for DWARF .debug_line)
    const std::vector<std::pair<uint64_t, uint32_t>>& get_line_entries() const { return dwarf_line_entries; }

    // Instruction translation dispatch
    void translate_instruction(Opcode opcode, const uint8_t* operands);

    // Individual instruction handlers
    void translate_nop();
    void translate_halt();
    void translate_load_imm(uint8_t reg, uint64_t immediate);
    void translate_add(uint8_t dst_reg, uint8_t src_reg);
    void translate_sub(uint8_t dst_reg, uint8_t src_reg);
    void translate_mov(uint8_t dst_reg, uint8_t src_reg);
    void translate_cmp(uint8_t reg1, uint8_t reg2);
    void translate_inc(uint8_t reg);
    void translate_dec(uint8_t reg);
    void translate_neg(uint8_t reg);
    void translate_not(uint8_t reg);
    void translate_mul(uint8_t dst_reg, uint8_t src_reg);
    void translate_div(uint8_t dst_reg, uint8_t src_reg);
    void translate_mod(uint8_t dst_reg, uint8_t src_reg);
    void translate_and(uint8_t dst_reg, uint8_t src_reg);
    void translate_or(uint8_t dst_reg, uint8_t src_reg);
    void translate_xor(uint8_t dst_reg, uint8_t src_reg);
    void translate_shl(uint8_t dst_reg, uint8_t src_reg);
    void translate_shr(uint8_t dst_reg, uint8_t src_reg);

    // Control flow
    void translate_jmp(uint32_t target_address);
    void translate_jz(uint32_t target_address);
    void translate_jnz(uint32_t target_address);
    void translate_js(uint32_t target_address);
    void translate_jns(uint32_t target_address);
    void translate_jc(uint32_t target_address);
    void translate_jnc(uint32_t target_address);
    void translate_jo(uint32_t target_address);
    void translate_jno(uint32_t target_address);
    void translate_jg(uint32_t target_address);
    void translate_jl(uint32_t target_address);
    void translate_jge(uint32_t target_address);
    void translate_jle(uint32_t target_address);
    void translate_call(uint32_t target_address);
    void translate_ret();

    // Memory operations
    void translate_load(uint8_t dst_reg, uint8_t addr_reg, int32_t offset);
    void translate_store_imm(uint8_t src_reg, const uint8_t* operands);
    void translate_store(uint8_t addr_reg, int32_t offset, uint8_t src_reg);
    void translate_loadr(uint8_t dst_reg, uint8_t addr_reg);
    void translate_storer(uint8_t addr_reg, uint8_t src_reg);
    void translate_lea(uint8_t dst_reg, uint32_t addr);
    void translate_swap(uint8_t reg, uint8_t addr_reg);

    // I/O operations
    void translate_in(uint8_t reg, uint8_t port);
    void translate_out(uint8_t reg, uint8_t port);
    void translate_inb(uint8_t reg, uint8_t port);
    void translate_outb(uint8_t reg, uint8_t port);
    void translate_inw(uint8_t reg, uint8_t port);
    void translate_outw(uint8_t reg, uint8_t port);
    void translate_inl(uint8_t reg, uint8_t port);
    void translate_outl(uint8_t reg, uint8_t port);
    void translate_instr(uint8_t reg, uint8_t port);
    void translate_outstr(uint8_t reg, uint8_t port);

    // FPU operations
    void translate_fld(const uint8_t* operands);
    void translate_fst(const uint8_t* operands, bool pop_after_store);
    void translate_fild(const uint8_t* operands);
    void translate_fist(const uint8_t* operands, bool pop_after_store);
    void translate_fadd(const uint8_t* operands);
    void translate_fsub(const uint8_t* operands);
    void translate_fmul(const uint8_t* operands);
    void translate_fdiv(const uint8_t* operands);
    void translate_finit();
    void translate_fclex();
    void translate_fstcw(const uint8_t* operands);
    void translate_fldcw(const uint8_t* operands);
    void translate_fstsw(const uint8_t* operands);
    void translate_fpu_unary(uint8_t opcode1, uint8_t opcode2);
    void translate_ftan();
    void translate_fcompp(bool unordered_compare);

    // Stack operations
    void translate_push(uint8_t reg);
    void translate_pop(uint8_t reg);
    void translate_push_arg();
    void translate_pop_arg();
    void translate_push_flag();
    void translate_pop_flag();

    // Prologue/epilogue
    void setup_function_prologue();
    void setup_function_epilogue();
    void emit_stack_frame_setup(size_t local_vars_size);
    void emit_stack_frame_teardown();

    // Jump target management
    void scan_for_jump_targets(const std::vector<uint8_t>& bytecode);
    void resolve_jump_targets();
    X86Encoder::Label& get_or_create_label(size_t bytecode_address);

    // Runtime support
    void emit_runtime_call(const char* function_name);
    void emit_device_io_call(uint16_t device_id, bool is_input);

    // Optimization passes
    void optimize_register_usage();
    void eliminate_redundant_moves();
    void fold_constant_operations();

    // Debug and analysis
    void print_compilation_stats() const;
    size_t get_code_size() const { return encoder.size(); }

private:
    // Internal register management
    void emit_function_prologue();
    void emit_function_epilogue();
    void emit_runtime_fallback(const char* reason);

    X86Register acquire_physical(uint8_t virt_reg);
    int32_t ensure_spill_slot(uint8_t virt_reg);
    void restore_virtual_value(uint8_t virt_reg, X86Register phys);
    void spill_virtual_value(uint8_t virt_reg, X86Register phys);
    void clear_cached_registers();
    void load_register(uint8_t virt_reg, X86Register phys);
    void store_register(uint8_t virt_reg, X86Register phys);
    void mark_dirty(uint8_t virt_reg);
    // INT 0x80 syscall translation
    void translate_int80();

    // Register management helpers used by syscall translation
    void flush_register(uint8_t virt_reg);
    void flush_all_registers();
    void flush_all_cached_registers();
    X86Register get_loaded_physical(uint8_t virt_reg);
    X86Register get_writable_physical(uint8_t virt_reg);

    // Data section support
    void emit_data_initialization(const std::vector<uint8_t>& bytecode, uint32_t entry_point);

    // Immediate extraction helpers
    uint32_t read_imm32(const uint8_t* ptr) const;
    uint64_t read_imm64_ptr(const uint8_t* ptr) const;

    void begin_fpu_sequence();
    void finish_fpu_sequence();
    void emit_x87_mem_op(uint8_t opcode, uint8_t reg_opcode);
    void emit_vmaddr_in_rax(uint32_t vm_addr);
    void emit_fpu_immediate_double(uint64_t raw_double, uint8_t opcode, uint8_t reg_opcode);
    void emit_fpu_immediate_int32(int32_t value, uint8_t opcode, uint8_t reg_opcode);
    void emit_x87_raw_op(uint8_t opcode1, uint8_t opcode2);
    void emit_fpu_compare_flags();

    // DWARF line tracking
    const std::vector<std::pair<size_t, uint32_t>>* bytecode_line_map = nullptr;
    std::vector<std::pair<uint64_t, uint32_t>> dwarf_line_entries; // (native_addr, source_line)
};

} // namespace CodeGen
