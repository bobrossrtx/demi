#include "x86_encoder.hpp"
#include <cassert>

namespace CodeGen {

void X86Encoder::emit_rex(bool w, bool r, bool x, bool b) {
    uint8_t rex = 0x40;
    if (w) rex |= 0x08;  // 64-bit operand
    if (r) rex |= 0x04;  // Extension of ModR/M reg field
    if (x) rex |= 0x02;  // Extension of SIB index field  
    if (b) rex |= 0x01;  // Extension of ModR/M r/m field
    code_buffer.push_back(rex);
}

void X86Encoder::emit_rex_if_needed(X86Register reg1, X86Register reg2) {
    bool need_rex = (static_cast<uint8_t>(reg1) >= 8) || 
                    (static_cast<uint8_t>(reg2) >= 8);
    if (need_rex) {
        bool r = static_cast<uint8_t>(reg1) >= 8;
        bool b = static_cast<uint8_t>(reg2) >= 8;
        emit_rex(true, r, false, b);  // Always use 64-bit mode
    } else {
        emit_rex(true, false, false, false);  // Standard 64-bit prefix
    }
}

void X86Encoder::emit_modrm(uint8_t mod, uint8_t reg, uint8_t rm) {
    uint8_t modrm = (mod << 6) | ((reg & 0x7) << 3) | (rm & 0x7);
    code_buffer.push_back(modrm);
}

uint8_t X86Encoder::reg_to_modrm(X86Register reg) {
    return static_cast<uint8_t>(reg) & 0x7;  // Lower 3 bits
}

// Basic MOV reg, reg
void X86Encoder::emit_mov_reg_reg(X86Register dst, X86Register src) {
    emit_rex_if_needed(src, dst);  // REX.R for src, REX.B for dst
    code_buffer.push_back(0x89);   // MOV r/m64, r64
    emit_modrm(0b11, reg_to_modrm(src), reg_to_modrm(dst));
}

// MOV reg, imm64
void X86Encoder::emit_mov_reg_imm64(X86Register dst, uint64_t imm) {
    // REX prefix for 64-bit immediate
    emit_rex(true, false, false, static_cast<uint8_t>(dst) >= 8);
    
    // MOV opcode + register encoding
    code_buffer.push_back(0xB8 + (static_cast<uint8_t>(dst) & 0x7));
    
    // 64-bit immediate (little endian)
    for (int i = 0; i < 8; i++) {
        code_buffer.push_back((imm >> (i * 8)) & 0xFF);
    }
}

// ADD reg, reg
void X86Encoder::emit_add_reg_reg(X86Register dst, X86Register src) {
    emit_rex_if_needed(src, dst);
    code_buffer.push_back(0x01);   // ADD r/m64, r64
    emit_modrm(0b11, reg_to_modrm(src), reg_to_modrm(dst));
}

// SUB reg, reg  
void X86Encoder::emit_sub_reg_reg(X86Register dst, X86Register src) {
    emit_rex_if_needed(src, dst);
    code_buffer.push_back(0x29);   // SUB r/m64, r64
    emit_modrm(0b11, reg_to_modrm(src), reg_to_modrm(dst));
}

// CMP reg, reg
void X86Encoder::emit_cmp_reg_reg(X86Register left, X86Register right) {
    emit_rex_if_needed(right, left);
    code_buffer.push_back(0x39);   // CMP r/m64, r64
    emit_modrm(0b11, reg_to_modrm(right), reg_to_modrm(left));
}

// MOV reg, imm32 (sign-extended to 64-bit)
void X86Encoder::emit_mov_reg_imm32(X86Register dst, int32_t imm) {
    emit_rex(true, false, false, static_cast<uint8_t>(dst) >= 8);
    code_buffer.push_back(0xC7);   // MOV r/m64, imm32
    emit_modrm(0b11, 0, reg_to_modrm(dst));
    for (int i = 0; i < 4; i++) {
        code_buffer.push_back((imm >> (i * 8)) & 0xFF);
    }
}

// INC reg
void X86Encoder::emit_inc_reg(X86Register reg) {
    emit_rex(true, false, false, static_cast<uint8_t>(reg) >= 8);
    code_buffer.push_back(0xFF);   // INC r/m64
    emit_modrm(0b11, 0, reg_to_modrm(reg));  // /0
}

// DEC reg
void X86Encoder::emit_dec_reg(X86Register reg) {
    emit_rex(true, false, false, static_cast<uint8_t>(reg) >= 8);
    code_buffer.push_back(0xFF);   // DEC r/m64
    emit_modrm(0b11, 1, reg_to_modrm(reg));  // /1
}

// NEG reg
void X86Encoder::emit_neg_reg(X86Register reg) {
    emit_rex(true, false, false, static_cast<uint8_t>(reg) >= 8);
    code_buffer.push_back(0xF7);   // NEG r/m64
    emit_modrm(0b11, 3, reg_to_modrm(reg));  // /3
}

// NOT reg
void X86Encoder::emit_not_reg(X86Register reg) {
    emit_rex(true, false, false, static_cast<uint8_t>(reg) >= 8);
    code_buffer.push_back(0xF7);   // NOT r/m64
    emit_modrm(0b11, 2, reg_to_modrm(reg));  // /2
}

// MUL reg (unsigned multiply RAX)
void X86Encoder::emit_mul_reg(X86Register reg) {
    emit_rex(true, false, false, static_cast<uint8_t>(reg) >= 8);
    code_buffer.push_back(0xF7);   // MUL r/m64
    emit_modrm(0b11, 4, reg_to_modrm(reg));  // /4
}

// DIV reg (unsigned divide RDX:RAX)
void X86Encoder::emit_div_reg(X86Register reg) {
    emit_rex(true, false, false, static_cast<uint8_t>(reg) >= 8);
    code_buffer.push_back(0xF7);   // DIV r/m64
    emit_modrm(0b11, 6, reg_to_modrm(reg));  // /6
}

// IDIV reg (signed divide RDX:RAX)
void X86Encoder::emit_idiv_reg(X86Register reg) {
    emit_rex(true, false, false, static_cast<uint8_t>(reg) >= 8);
    code_buffer.push_back(0xF7);   // IDIV r/m64
    emit_modrm(0b11, 7, reg_to_modrm(reg));  // /7
}

// IMUL dst, src (signed multiply)
void X86Encoder::emit_imul_reg_reg(X86Register dst, X86Register src) {
    emit_rex_if_needed(dst, src);
    code_buffer.push_back(0x0F);
    code_buffer.push_back(0xAF);   // IMUL r64, r/m64
    emit_modrm(0b11, reg_to_modrm(dst), reg_to_modrm(src));
}

// AND reg, reg
void X86Encoder::emit_and_reg_reg(X86Register dst, X86Register src) {
    emit_rex_if_needed(src, dst);
    code_buffer.push_back(0x21);   // AND r/m64, r64
    emit_modrm(0b11, reg_to_modrm(src), reg_to_modrm(dst));
}

// OR reg, reg
void X86Encoder::emit_or_reg_reg(X86Register dst, X86Register src) {
    emit_rex_if_needed(src, dst);
    code_buffer.push_back(0x09);   // OR r/m64, r64
    emit_modrm(0b11, reg_to_modrm(src), reg_to_modrm(dst));
}

// XOR reg, reg
void X86Encoder::emit_xor_reg_reg(X86Register dst, X86Register src) {
    emit_rex_if_needed(src, dst);
    code_buffer.push_back(0x31);   // XOR r/m64, r64
    emit_modrm(0b11, reg_to_modrm(src), reg_to_modrm(dst));
}

// TEST reg, reg
void X86Encoder::emit_test_reg_reg(X86Register dst, X86Register src) {
    emit_rex_if_needed(src, dst);
    code_buffer.push_back(0x85);   // TEST r/m64, r64
    emit_modrm(0b11, reg_to_modrm(src), reg_to_modrm(dst));
}

// TEST reg, imm32
void X86Encoder::emit_test_reg_imm32(X86Register reg, int32_t imm) {
    emit_rex(true, false, false, static_cast<uint8_t>(reg) >= 8);
    code_buffer.push_back(0xF7);   // TEST r/m64, imm32
    emit_modrm(0b11, 0, reg_to_modrm(reg));  // /0
    for (int i = 0; i < 4; i++) {
        code_buffer.push_back((imm >> (i * 8)) & 0xFF);
    }
}

// ROL reg, imm8
void X86Encoder::emit_rol_reg_imm8(X86Register reg, uint8_t imm) {
    emit_rex(true, false, false, static_cast<uint8_t>(reg) >= 8);
    code_buffer.push_back(0xC1);   // ROL r/m64, imm8
    emit_modrm(0b11, 0, reg_to_modrm(reg));  // /0
    code_buffer.push_back(imm & 0x1F);
}

// ROR reg, imm8
void X86Encoder::emit_ror_reg_imm8(X86Register reg, uint8_t imm) {
    emit_rex(true, false, false, static_cast<uint8_t>(reg) >= 8);
    code_buffer.push_back(0xC1);   // ROR r/m64, imm8
    emit_modrm(0b11, 1, reg_to_modrm(reg));  // /1
    code_buffer.push_back(imm & 0x1F);
}

// ROL reg, CL
void X86Encoder::emit_rol_reg_cl(X86Register reg) {
    emit_rex(true, false, false, static_cast<uint8_t>(reg) >= 8);
    code_buffer.push_back(0xD3);   // ROL r/m64, CL
    emit_modrm(0b11, 0, reg_to_modrm(reg));  // /0
}

// ROR reg, CL
void X86Encoder::emit_ror_reg_cl(X86Register reg) {
    emit_rex(true, false, false, static_cast<uint8_t>(reg) >= 8);
    code_buffer.push_back(0xD3);   // ROR r/m64, CL
    emit_modrm(0b11, 1, reg_to_modrm(reg));  // /1
}

// SHL reg, imm8
void X86Encoder::emit_shl_reg_imm8(X86Register reg, uint8_t imm) {
    emit_rex(true, false, false, static_cast<uint8_t>(reg) >= 8);
    code_buffer.push_back(0xC1);   // SHL r/m64, imm8
    emit_modrm(0b11, 4, reg_to_modrm(reg));  // /4
    code_buffer.push_back(imm & 0x1F);       // Imm8 (only low 5 bits used)
}

// SHR reg, imm8
void X86Encoder::emit_shr_reg_imm8(X86Register reg, uint8_t imm) {
    emit_rex(true, false, false, static_cast<uint8_t>(reg) >= 8);
    code_buffer.push_back(0xC1);   // SHR r/m64, imm8
    emit_modrm(0b11, 5, reg_to_modrm(reg));  // /5
    code_buffer.push_back(imm & 0x1F);       // Imm8 (only low 5 bits used)
}

// SHL reg, CL
void X86Encoder::emit_shl_reg_cl(X86Register reg) {
    emit_rex(true, false, false, static_cast<uint8_t>(reg) >= 8);
    code_buffer.push_back(0xD3);   // SHL r/m64, CL
    emit_modrm(0b11, 4, reg_to_modrm(reg));  // /4
}

// SHR reg, CL
void X86Encoder::emit_shr_reg_cl(X86Register reg) {
    emit_rex(true, false, false, static_cast<uint8_t>(reg) >= 8);
    code_buffer.push_back(0xD3);   // SHR r/m64, CL
    emit_modrm(0b11, 5, reg_to_modrm(reg));  // /5
}

// ADD reg, imm32 (sign-extended)
void X86Encoder::emit_add_reg_imm32(X86Register dst, int32_t imm) {
    emit_rex(true, false, false, static_cast<uint8_t>(dst) >= 8);
    code_buffer.push_back(0x81);   // ADD r/m64, imm32
    emit_modrm(0b11, 0, reg_to_modrm(dst));  // /0
    for (int i = 0; i < 4; i++) {
        code_buffer.push_back((imm >> (i * 8)) & 0xFF);
    }
}

// SUB reg, imm32 (sign-extended)
void X86Encoder::emit_sub_reg_imm32(X86Register dst, int32_t imm) {
    emit_rex(true, false, false, static_cast<uint8_t>(dst) >= 8);
    code_buffer.push_back(0x81);   // SUB r/m64, imm32
    emit_modrm(0b11, 5, reg_to_modrm(dst));  // /5
    for (int i = 0; i < 4; i++) {
        code_buffer.push_back((imm >> (i * 8)) & 0xFF);
    }
}

// CMP reg, imm32 (sign-extended)
void X86Encoder::emit_cmp_reg_imm32(X86Register dst, int32_t imm) {
    emit_rex(true, false, false, static_cast<uint8_t>(dst) >= 8);
    code_buffer.push_back(0x81);   // CMP r/m64, imm32
    emit_modrm(0b11, 7, reg_to_modrm(dst));  // /7
    for (int i = 0; i < 4; i++) {
        code_buffer.push_back((imm >> (i * 8)) & 0xFF);
    }
}

// AND reg, imm32 (sign-extended)
void X86Encoder::emit_and_reg_imm32(X86Register dst, int32_t imm) {
    emit_rex(true, false, false, static_cast<uint8_t>(dst) >= 8);
    code_buffer.push_back(0x81);   // AND r/m64, imm32
    emit_modrm(0b11, 4, reg_to_modrm(dst));  // /4
    for (int i = 0; i < 4; i++) {
        code_buffer.push_back((imm >> (i * 8)) & 0xFF);
    }
}

// OR reg, imm32 (sign-extended)
void X86Encoder::emit_or_reg_imm32(X86Register dst, int32_t imm) {
    emit_rex(true, false, false, static_cast<uint8_t>(dst) >= 8);
    code_buffer.push_back(0x81);   // OR r/m64, imm32
    emit_modrm(0b11, 1, reg_to_modrm(dst));  // /1
    for (int i = 0; i < 4; i++) {
        code_buffer.push_back((imm >> (i * 8)) & 0xFF);
    }
}

// XOR reg, imm32 (sign-extended)
void X86Encoder::emit_xor_reg_imm32(X86Register dst, int32_t imm) {
    emit_rex(true, false, false, static_cast<uint8_t>(dst) >= 8);
    code_buffer.push_back(0x81);   // XOR r/m64, imm32
    emit_modrm(0b11, 6, reg_to_modrm(dst));  // /6
    for (int i = 0; i < 4; i++) {
        code_buffer.push_back((imm >> (i * 8)) & 0xFF);
    }
}

// Memory operations
void X86Encoder::emit_mov_reg_mem(X86Register dst, X86Register base, int32_t offset) {
    emit_rex_if_needed(dst, base);
    code_buffer.push_back(0x8B);   // MOV r64, r/m64
    
    uint8_t base_modrm = reg_to_modrm(base);
    bool needs_sib = (base_modrm == 4); // RSP or R12+ require SIB byte
    
    if (offset == 0 && base != X86Register::RBP && !needs_sib) {
        // [reg] - no displacement, no SIB
        emit_modrm(0b00, reg_to_modrm(dst), base_modrm);
    } else if (offset == 0 && needs_sib) {
        // [reg] with SIB (no displacement)
        emit_modrm(0b00, reg_to_modrm(dst), 4);
        code_buffer.push_back(0x24);  // SIB: scale=0, index=none, base=ESP/R12
    } else if (offset >= -128 && offset <= 127) {
        // [reg + disp8]
        emit_modrm(0b01, reg_to_modrm(dst), needs_sib ? 4 : base_modrm);
        if (needs_sib) code_buffer.push_back(0x24);
        code_buffer.push_back(static_cast<uint8_t>(offset));
    } else {
        // [reg + disp32]
        emit_modrm(0b10, reg_to_modrm(dst), needs_sib ? 4 : base_modrm);
        if (needs_sib) code_buffer.push_back(0x24);
        for (int i = 0; i < 4; i++) {
            code_buffer.push_back((offset >> (i * 8)) & 0xFF);
        }
    }
}

void X86Encoder::emit_movzx_reg_mem8(X86Register dst, X86Register base, int32_t offset) {
    emit_rex(true, static_cast<uint8_t>(dst) >= 8, false, static_cast<uint8_t>(base) >= 8);
    code_buffer.push_back(0x0F);
    code_buffer.push_back(0xB6);   // MOVZX r64, r/m8

    uint8_t base_modrm = reg_to_modrm(base);
    bool needs_sib = (base_modrm == 4);

    if (offset == 0 && base != X86Register::RBP && !needs_sib) {
        emit_modrm(0b00, reg_to_modrm(dst), base_modrm);
    } else if (offset == 0 && needs_sib) {
        emit_modrm(0b00, reg_to_modrm(dst), 4);
        code_buffer.push_back(0x24);
    } else if (offset >= -128 && offset <= 127) {
        emit_modrm(0b01, reg_to_modrm(dst), needs_sib ? 4 : base_modrm);
        if (needs_sib) code_buffer.push_back(0x24);
        code_buffer.push_back(static_cast<uint8_t>(offset));
    } else {
        emit_modrm(0b10, reg_to_modrm(dst), needs_sib ? 4 : base_modrm);
        if (needs_sib) code_buffer.push_back(0x24);
        for (int i = 0; i < 4; i++) {
            code_buffer.push_back((offset >> (i * 8)) & 0xFF);
        }
    }
}

void X86Encoder::emit_mov_mem_reg(X86Register base, int32_t offset, X86Register src) {
    emit_rex_if_needed(src, base);
    code_buffer.push_back(0x89);   // MOV r/m64, r64
    
    uint8_t base_modrm = reg_to_modrm(base);
    bool needs_sib = (base_modrm == 4);
    
    if (offset == 0 && base != X86Register::RBP && !needs_sib) {
        emit_modrm(0b00, reg_to_modrm(src), base_modrm);
    } else if (offset == 0 && needs_sib) {
        emit_modrm(0b00, reg_to_modrm(src), 4);
        code_buffer.push_back(0x24);
    } else if (offset >= -128 && offset <= 127) {
        emit_modrm(0b01, reg_to_modrm(src), needs_sib ? 4 : base_modrm);
        if (needs_sib) code_buffer.push_back(0x24);
        code_buffer.push_back(static_cast<uint8_t>(offset));
    } else {
        emit_modrm(0b10, reg_to_modrm(src), needs_sib ? 4 : base_modrm);
        if (needs_sib) code_buffer.push_back(0x24);
        for (int i = 0; i < 4; i++) {
            code_buffer.push_back((offset >> (i * 8)) & 0xFF);
        }
    }
}

void X86Encoder::emit_mov_mem8_reg8(X86Register base, int32_t offset, X86Register src) {
    emit_rex(false,
             static_cast<uint8_t>(src) >= 8,
             false,
             static_cast<uint8_t>(base) >= 8);
    code_buffer.push_back(0x88);   // MOV r/m8, r8

    uint8_t base_modrm = reg_to_modrm(base);
    bool needs_sib = (base_modrm == 4);

    if (offset == 0 && base != X86Register::RBP && !needs_sib) {
        emit_modrm(0b00, reg_to_modrm(src), base_modrm);
    } else if (offset == 0 && needs_sib) {
        emit_modrm(0b00, reg_to_modrm(src), 4);
        code_buffer.push_back(0x24);
    } else if (offset >= -128 && offset <= 127) {
        emit_modrm(0b01, reg_to_modrm(src), needs_sib ? 4 : base_modrm);
        if (needs_sib) code_buffer.push_back(0x24);
        code_buffer.push_back(static_cast<uint8_t>(offset));
    } else {
        emit_modrm(0b10, reg_to_modrm(src), needs_sib ? 4 : base_modrm);
        if (needs_sib) code_buffer.push_back(0x24);
        for (int i = 0; i < 4; i++) {
            code_buffer.push_back((offset >> (i * 8)) & 0xFF);
        }
    }
}

// Stack operations
void X86Encoder::emit_push_reg(X86Register reg) {
    if (static_cast<uint8_t>(reg) >= 8) {
        emit_rex(false, false, false, true);  // REX.B for extended registers
    }
    code_buffer.push_back(0x50 + (static_cast<uint8_t>(reg) & 0x7));
}

void X86Encoder::emit_pop_reg(X86Register reg) {
    if (static_cast<uint8_t>(reg) >= 8) {
        emit_rex(false, false, false, true);
    }
    code_buffer.push_back(0x58 + (static_cast<uint8_t>(reg) & 0x7));
}

// Control flow
void X86Encoder::emit_jmp_rel32(int32_t offset) {
    code_buffer.push_back(0xE9);   // JMP rel32
    for (int i = 0; i < 4; i++) {
        code_buffer.push_back((offset >> (i * 8)) & 0xFF);
    }
}

void X86Encoder::emit_jz_rel32(int32_t offset) {
    code_buffer.push_back(0x0F);   // Two-byte opcode prefix
    code_buffer.push_back(0x84);   // JZ rel32
    for (int i = 0; i < 4; i++) {
        code_buffer.push_back((offset >> (i * 8)) & 0xFF);
    }
}

void X86Encoder::emit_jnz_rel32(int32_t offset) {
    code_buffer.push_back(0x0F);   // Two-byte opcode prefix  
    code_buffer.push_back(0x85);   // JNZ rel32
    for (int i = 0; i < 4; i++) {
        code_buffer.push_back((offset >> (i * 8)) & 0xFF);
    }
}

void X86Encoder::emit_jg_rel32(int32_t offset) {
    code_buffer.push_back(0x0F);
    code_buffer.push_back(0x8F);   // JG rel32
    for (int i = 0; i < 4; i++)
        code_buffer.push_back((offset >> (i * 8)) & 0xFF);
}

void X86Encoder::emit_jl_rel32(int32_t offset) {
    code_buffer.push_back(0x0F);
    code_buffer.push_back(0x8C);   // JL rel32
    for (int i = 0; i < 4; i++)
        code_buffer.push_back((offset >> (i * 8)) & 0xFF);
}

void X86Encoder::emit_jge_rel32(int32_t offset) {
    code_buffer.push_back(0x0F);
    code_buffer.push_back(0x8D);   // JGE rel32
    for (int i = 0; i < 4; i++)
        code_buffer.push_back((offset >> (i * 8)) & 0xFF);
}

void X86Encoder::emit_jle_rel32(int32_t offset) {
    code_buffer.push_back(0x0F);
    code_buffer.push_back(0x8E);   // JLE rel32
    for (int i = 0; i < 4; i++)
        code_buffer.push_back((offset >> (i * 8)) & 0xFF);
}

void X86Encoder::emit_jc_rel32(int32_t offset) {
    code_buffer.push_back(0x0F);
    code_buffer.push_back(0x82);   // JC rel32 (JB)
    for (int i = 0; i < 4; i++)
        code_buffer.push_back((offset >> (i * 8)) & 0xFF);
}

void X86Encoder::emit_jnc_rel32(int32_t offset) {
    code_buffer.push_back(0x0F);
    code_buffer.push_back(0x83);   // JNC rel32 (JNB/JAE)
    for (int i = 0; i < 4; i++)
        code_buffer.push_back((offset >> (i * 8)) & 0xFF);
}

void X86Encoder::emit_jo_rel32(int32_t offset) {
    code_buffer.push_back(0x0F);
    code_buffer.push_back(0x80);   // JO rel32
    for (int i = 0; i < 4; i++)
        code_buffer.push_back((offset >> (i * 8)) & 0xFF);
}

void X86Encoder::emit_jno_rel32(int32_t offset) {
    code_buffer.push_back(0x0F);
    code_buffer.push_back(0x81);   // JNO rel32
    for (int i = 0; i < 4; i++)
        code_buffer.push_back((offset >> (i * 8)) & 0xFF);
}

void X86Encoder::emit_js_rel32(int32_t offset) {
    code_buffer.push_back(0x0F);
    code_buffer.push_back(0x88);   // JS rel32
    for (int i = 0; i < 4; i++)
        code_buffer.push_back((offset >> (i * 8)) & 0xFF);
}

void X86Encoder::emit_jns_rel32(int32_t offset) {
    code_buffer.push_back(0x0F);
    code_buffer.push_back(0x89);   // JNS rel32
    for (int i = 0; i < 4; i++)
        code_buffer.push_back((offset >> (i * 8)) & 0xFF);
}

void X86Encoder::emit_call_rel32(int32_t offset) {
    code_buffer.push_back(0xE8);   // CALL rel32
    for (int i = 0; i < 4; i++) {
        code_buffer.push_back((offset >> (i * 8)) & 0xFF);
    }
}

void X86Encoder::emit_ret() {
    code_buffer.push_back(0xC3);   // RET
}

// String operations
void X86Encoder::emit_cld() {
    code_buffer.push_back(0xFC);
}

void X86Encoder::emit_repne_scasb() {
    code_buffer.push_back(0xF2);
    code_buffer.push_back(0xAE);
}

// System interface
void X86Encoder::emit_syscall() {
    code_buffer.push_back(0x0F);
    code_buffer.push_back(0x05);
}

// Utility
void X86Encoder::emit_nop() {
    code_buffer.push_back(0x90);   // NOP
}

void X86Encoder::emit_int3() {
    code_buffer.push_back(0xCC);   // INT 3 (breakpoint)
}

// Label management (simplified for now)
X86Encoder::Label X86Encoder::create_label() {
    return Label{0, false, {}};
}

void X86Encoder::bind_label(Label& label) {
    label.position = code_buffer.size();
    label.bound = true;
    
    // Resolve all pending jumps to this label
    for (size_t jump_pos : label.unresolved_jumps) {
        int32_t offset = static_cast<int32_t>(label.position - (jump_pos + 4));
        // Patch the 4-byte offset at jump_pos
        for (int i = 0; i < 4; i++) {
            code_buffer[jump_pos + i] = (offset >> (i * 8)) & 0xFF;
        }
    }
    label.unresolved_jumps.clear();
}

void X86Encoder::emit_jmp_label(Label& label) {
    if (label.bound) {
        int32_t offset = static_cast<int32_t>(label.position - (code_buffer.size() + 5));
        emit_jmp_rel32(offset);
    } else {
        // Forward reference - save position for later patching
        label.unresolved_jumps.push_back(code_buffer.size() + 1);  // +1 to skip opcode
        emit_jmp_rel32(0);  // Placeholder offset
    }
}

void X86Encoder::emit_jz_label(Label& label) {
    if (label.bound) {
        int32_t offset = static_cast<int32_t>(label.position - (code_buffer.size() + 6));
        emit_jz_rel32(offset);
    } else {
        label.unresolved_jumps.push_back(code_buffer.size() + 2);  // +2 for two-byte opcode
        emit_jz_rel32(0);
    }
}

void X86Encoder::emit_jnz_label(Label& label) {
    if (label.bound) {
        int32_t offset = static_cast<int32_t>(label.position - (code_buffer.size() + 6));
        emit_jnz_rel32(offset);
    } else {
        label.unresolved_jumps.push_back(code_buffer.size() + 2);
        emit_jnz_rel32(0);
    }
}

void X86Encoder::emit_jg_label(Label& label) {
    if (label.bound) {
        int32_t offset = static_cast<int32_t>(label.position - (code_buffer.size() + 6));
        emit_jg_rel32(offset);
    } else {
        label.unresolved_jumps.push_back(code_buffer.size() + 2);
        emit_jg_rel32(0);
    }
}

void X86Encoder::emit_jl_label(Label& label) {
    if (label.bound) {
        int32_t offset = static_cast<int32_t>(label.position - (code_buffer.size() + 6));
        emit_jl_rel32(offset);
    } else {
        label.unresolved_jumps.push_back(code_buffer.size() + 2);
        emit_jl_rel32(0);
    }
}

void X86Encoder::emit_jge_label(Label& label) {
    if (label.bound) {
        int32_t offset = static_cast<int32_t>(label.position - (code_buffer.size() + 6));
        emit_jge_rel32(offset);
    } else {
        label.unresolved_jumps.push_back(code_buffer.size() + 2);
        emit_jge_rel32(0);
    }
}

void X86Encoder::emit_jle_label(Label& label) {
    if (label.bound) {
        int32_t offset = static_cast<int32_t>(label.position - (code_buffer.size() + 6));
        emit_jle_rel32(offset);
    } else {
        label.unresolved_jumps.push_back(code_buffer.size() + 2);
        emit_jle_rel32(0);
    }
}

void X86Encoder::emit_jc_label(Label& label) {
    if (label.bound) {
        int32_t offset = static_cast<int32_t>(label.position - (code_buffer.size() + 6));
        emit_jc_rel32(offset);
    } else {
        label.unresolved_jumps.push_back(code_buffer.size() + 2);
        emit_jc_rel32(0);
    }
}

void X86Encoder::emit_jnc_label(Label& label) {
    if (label.bound) {
        int32_t offset = static_cast<int32_t>(label.position - (code_buffer.size() + 6));
        emit_jnc_rel32(offset);
    } else {
        label.unresolved_jumps.push_back(code_buffer.size() + 2);
        emit_jnc_rel32(0);
    }
}

void X86Encoder::emit_jo_label(Label& label) {
    if (label.bound) {
        int32_t offset = static_cast<int32_t>(label.position - (code_buffer.size() + 6));
        emit_jo_rel32(offset);
    } else {
        label.unresolved_jumps.push_back(code_buffer.size() + 2);
        emit_jo_rel32(0);
    }
}

void X86Encoder::emit_jno_label(Label& label) {
    if (label.bound) {
        int32_t offset = static_cast<int32_t>(label.position - (code_buffer.size() + 6));
        emit_jno_rel32(offset);
    } else {
        label.unresolved_jumps.push_back(code_buffer.size() + 2);
        emit_jno_rel32(0);
    }
}

void X86Encoder::emit_js_label(Label& label) {
    if (label.bound) {
        int32_t offset = static_cast<int32_t>(label.position - (code_buffer.size() + 6));
        emit_js_rel32(offset);
    } else {
        label.unresolved_jumps.push_back(code_buffer.size() + 2);
        emit_js_rel32(0);
    }
}

void X86Encoder::emit_jns_label(Label& label) {
    if (label.bound) {
        int32_t offset = static_cast<int32_t>(label.position - (code_buffer.size() + 6));
        emit_jns_rel32(offset);
    } else {
        label.unresolved_jumps.push_back(code_buffer.size() + 2);
        emit_jns_rel32(0);
    }
}

} // namespace CodeGen
