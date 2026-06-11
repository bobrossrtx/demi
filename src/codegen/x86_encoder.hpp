#pragma once
#include <vector>
#include <cstdint>
#include <string>

namespace CodeGen {

// x86-64 Register encoding
enum class X86Register : uint8_t {
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8  = 8, R9  = 9, R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15
};

// x86-64 Instruction encoder
class X86Encoder {
private:
    std::vector<uint8_t> code_buffer;

    // REX prefix helpers
    void emit_rex(bool w, bool r, bool x, bool b);
    void emit_rex_if_needed(X86Register reg1, X86Register reg2 = X86Register::RAX);

    // ModR/M and SIB helpers
    void emit_modrm(uint8_t mod, uint8_t reg, uint8_t rm);
    uint8_t reg_to_modrm(X86Register reg);

public:
    X86Encoder() = default;

    // Basic instruction emission
    void emit_mov_reg_reg(X86Register dst, X86Register src);
    void emit_mov_reg_imm64(X86Register dst, uint64_t imm);
    void emit_mov_reg_imm32(X86Register dst, int32_t imm);
    void emit_add_reg_reg(X86Register dst, X86Register src);
    void emit_sub_reg_reg(X86Register dst, X86Register src);
    void emit_cmp_reg_reg(X86Register left, X86Register right);

    // One-operand arithmetic (unsigned)
    void emit_inc_reg(X86Register reg);
    void emit_dec_reg(X86Register reg);
    void emit_neg_reg(X86Register reg);
    void emit_not_reg(X86Register reg);
    void emit_mul_reg(X86Register reg);
    void emit_div_reg(X86Register reg);
    void emit_idiv_reg(X86Register reg);

    // Two-operand signed multiply
    void emit_imul_reg_reg(X86Register dst, X86Register src);

    // Logic operations (two registers)
    void emit_and_reg_reg(X86Register dst, X86Register src);
    void emit_or_reg_reg(X86Register dst, X86Register src);
    void emit_xor_reg_reg(X86Register dst, X86Register src);

    // TEST instruction (sets flags without writing result)
    void emit_test_reg_reg(X86Register dst, X86Register src);
    void emit_test_reg_imm32(X86Register reg, int32_t imm);

    // Rotate operations
    void emit_rol_reg_imm8(X86Register reg, uint8_t imm);
    void emit_ror_reg_imm8(X86Register reg, uint8_t imm);
    void emit_rol_reg_cl(X86Register reg);
    void emit_ror_reg_cl(X86Register reg);

    // Shift operations
    void emit_shl_reg_imm8(X86Register reg, uint8_t imm);
    void emit_shr_reg_imm8(X86Register reg, uint8_t imm);
    void emit_shl_reg_cl(X86Register reg);
    void emit_shr_reg_cl(X86Register reg);

    // Register-immediate arithmetic (sign-extended 32-bit immediate)
    void emit_add_reg_imm32(X86Register dst, int32_t imm);
    void emit_sub_reg_imm32(X86Register dst, int32_t imm);
    void emit_cmp_reg_imm32(X86Register dst, int32_t imm);
    void emit_and_reg_imm32(X86Register dst, int32_t imm);
    void emit_or_reg_imm32(X86Register dst, int32_t imm);
    void emit_xor_reg_imm32(X86Register dst, int32_t imm);

    // Memory operations
    void emit_mov_reg_mem(X86Register dst, X86Register base, int32_t offset = 0);
    void emit_movzx_reg_mem8(X86Register dst, X86Register base, int32_t offset = 0);
    void emit_mov_mem_reg(X86Register base, int32_t offset, X86Register src);
    void emit_mov_mem8_reg8(X86Register base, int32_t offset, X86Register src);

    // Stack operations
    void emit_push_reg(X86Register reg);
    void emit_pop_reg(X86Register reg);

    // Control flow
    void emit_jmp_rel32(int32_t offset);
    void emit_jz_rel32(int32_t offset);
    void emit_jnz_rel32(int32_t offset);
    void emit_jg_rel32(int32_t offset);
    void emit_jl_rel32(int32_t offset);
    void emit_jge_rel32(int32_t offset);
    void emit_jle_rel32(int32_t offset);
    void emit_jc_rel32(int32_t offset);
    void emit_jnc_rel32(int32_t offset);
    void emit_jo_rel32(int32_t offset);
    void emit_jno_rel32(int32_t offset);
    void emit_js_rel32(int32_t offset);
    void emit_jns_rel32(int32_t offset);
    void emit_call_rel32(int32_t offset);
    void emit_ret();

    // String operations
    void emit_cld();           // Clear direction flag
    void emit_repne_scasb();   // REPNE SCASB - scan for AL byte

    // System interface
    void emit_syscall();       // SYSCALL instruction

    // Program structure
    void emit_nop();
    void emit_int3();  // Breakpoint for debugging

    // Raw byte emission for special instructions not covered by named methods
    void emit_raw_byte(uint8_t byte) { code_buffer.push_back(byte); }
    void emit_raw_bytes(const uint8_t* data, size_t len) {
        code_buffer.insert(code_buffer.end(), data, data + len);
    }

    // Code buffer management
    const std::vector<uint8_t>& get_code() const { return code_buffer; }
    void clear() { code_buffer.clear(); }
    size_t size() const { return code_buffer.size(); }

    // Label/jump management for forward references
    struct Label {
        size_t position;
        bool bound;
        std::vector<size_t> unresolved_jumps;
    };

    Label create_label();
    void bind_label(Label& label);
    void emit_jmp_label(Label& label);
    void emit_jz_label(Label& label);
    void emit_jnz_label(Label& label);
    void emit_jg_label(Label& label);
    void emit_jl_label(Label& label);
    void emit_jge_label(Label& label);
    void emit_jle_label(Label& label);
    void emit_jc_label(Label& label);
    void emit_jnc_label(Label& label);
    void emit_jo_label(Label& label);
    void emit_jno_label(Label& label);
    void emit_js_label(Label& label);
    void emit_jns_label(Label& label);
};

} // namespace CodeGen
