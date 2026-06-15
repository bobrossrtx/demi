#include "disa_compiler.hpp"
#include <cstring>
#include <iostream>

namespace CodeGen {

size_t get_instruction_length(uint8_t opcode_byte, const uint8_t* program, size_t pos, size_t size) {
    Opcode opcode = static_cast<Opcode>(opcode_byte);
    switch (opcode) {
        case Opcode::LOAD_IMM:
            return 6;
        case Opcode::ADD:
        case Opcode::SUB:
        case Opcode::MOV:
        case Opcode::CMP:
        case Opcode::MUL:
        case Opcode::DIV:
        case Opcode::AND:
        case Opcode::OR:
        case Opcode::XOR:
        case Opcode::SHL:
        case Opcode::SHR:
        case Opcode::IN:
        case Opcode::OUT:
        case Opcode::INB:
        case Opcode::OUTB:
        case Opcode::INW:
        case Opcode::OUTW:
        case Opcode::INL:
        case Opcode::OUTL:
        case Opcode::INSTR:
        case Opcode::OUTSTR:
        case Opcode::ADD64:
        case Opcode::SUB64:
        case Opcode::MOV64:
        case Opcode::AND64:
        case Opcode::OR64:
        case Opcode::XOR64:
        case Opcode::SHL64:
        case Opcode::SHR64:
        case Opcode::CMP64:
        case Opcode::MOD:
        case Opcode::MOVEX:
        case Opcode::ADDEX:
        case Opcode::SUBEX:
        case Opcode::CMPEX:
        case Opcode::PUSHEX:
        case Opcode::POPEX:
        case Opcode::SWAP:
        case Opcode::LOADR:
        case Opcode::STORER:
            return 3;
        case Opcode::MUL64:
        case Opcode::DIV64:
        case Opcode::MOD64:
        case Opcode::MULEX:
        case Opcode::DIVEX:
            return 4;
        case Opcode::LEA:
            return 6;
        case Opcode::LOAD:
        case Opcode::STORE:
            return 6;
        case Opcode::LOADEX:
        case Opcode::STOREX:
            return 10;
        case Opcode::JMP:
        case Opcode::JZ:
        case Opcode::JNZ:
        case Opcode::JS:
        case Opcode::JNS:
        case Opcode::JC:
        case Opcode::JNC:
        case Opcode::JO:
        case Opcode::JNO:
        case Opcode::JG:
        case Opcode::JL:
        case Opcode::JGE:
        case Opcode::JLE:
        case Opcode::CALL:
            return 5;
        case Opcode::PUSH:
        case Opcode::POP:
        case Opcode::INC:
        case Opcode::DEC:
        case Opcode::NOT:
        case Opcode::INC64:
        case Opcode::DEC64:
        case Opcode::NOT64:
        case Opcode::PUSH_ARG:
        case Opcode::POP_ARG:
            return 2;
        case Opcode::HALT:
        case Opcode::NOP:
        case Opcode::RET:
        case Opcode::PUSH_FLAG:
        case Opcode::POP_FLAG:
        case Opcode::MODE32:
        case Opcode::MODE64:
            return 1;
        case Opcode::INT:       // INT is 2 bytes: opcode + interrupt number
            return 2;
        case Opcode::LOAD_IMM64:
            return 10;
        case Opcode::MODECMP:
            return 2;
        case Opcode::FLD:
        case Opcode::FST:
        case Opcode::FSTP:
        case Opcode::FADD:
        case Opcode::FSUB:
        case Opcode::FMUL:
        case Opcode::FDIV:
            if (pos + 1 >= size) return 2;
            if (program[pos + 1] == 0x02) return 10;
            if (program[pos + 1] == 0x03) return 3;
            return 6;
        case Opcode::FILD:
        case Opcode::FIST:
        case Opcode::FISTP:
            return 6;
        case Opcode::FSTCW:
        case Opcode::FLDCW:
            return 5;
        case Opcode::FSTSW:
            if (pos + 1 >= size) return 2;
            return program[pos + 1] == 0x01 ? 2 : 6;
        case Opcode::FINIT:
        case Opcode::FCLEX:
        case Opcode::FSIN:
        case Opcode::FCOS:
        case Opcode::FTAN:
        case Opcode::FSQRT:
        case Opcode::FABS:
        case Opcode::FCHS:
        case Opcode::FCOMPP:
        case Opcode::FUCOMPP:
            return 1;
        case Opcode::DB:
            if (pos + 2 < size) {
                uint8_t length = program[pos + 2];
                return 3 + length;
            }
            return 1;
        default:
            return 1;
    }
}

// --- Track VM register state in x86 registers ---

struct VirtualRegState {
    X86Register phys;
    bool loaded;
    bool dirty;
};

// --- Main compilation ---

std::vector<uint8_t> DISAToX86Compiler::compile_program(const std::vector<uint8_t>& disa_bytecode,
                                                         uint32_t entry_point,
                                                         const std::vector<std::pair<size_t, uint32_t>>* line_map) {
    current_program = &disa_bytecode;
    encoder.clear();
    reg_alloc.reset_for_new_function();
    jump_targets.clear();
    function_labels.clear();
    function_addresses.clear();
    reg_state_map.clear();
    spill_slots.clear();
    slot_contains_valid.clear();
    function_has_calls = false;
    bytecode_line_map = line_map;
    dwarf_line_entries.clear();

    if (disa_bytecode.empty()) return {};

    // Clamp entry_point to valid range
    if (entry_point >= disa_bytecode.size()) {
        entry_point = 0;
    }

    scan_for_jump_targets(disa_bytecode);

    emit_function_prologue();

    // Emit data initialization for the entire bytecode (code + data)
    // Even when entry_point=0, data may be interleaved with or follow code.
    emit_data_initialization(disa_bytecode, entry_point);

    // Compile code starting from entry_point
    current_bytecode_pos = entry_point;
    while (current_bytecode_pos < disa_bytecode.size()) {
        auto jt_it = jump_targets.find(current_bytecode_pos);
        if (jt_it != jump_targets.end()) {
            encoder.bind_label(jt_it->second.x86_label);
        }

        uint8_t opcode_byte = disa_bytecode[current_bytecode_pos];
        Opcode opcode = static_cast<Opcode>(opcode_byte);
        const uint8_t* operands = (current_bytecode_pos + 1 < disa_bytecode.size())
                                  ? &disa_bytecode[current_bytecode_pos + 1]
                                  : nullptr;

        // Record DWARF line entry for this instruction
        if (bytecode_line_map) {
            uint64_t native_pos = encoder.size();
            for (const auto& [bc_offset, line] : *bytecode_line_map) {
                if (bc_offset == current_bytecode_pos) {
                    dwarf_line_entries.push_back({native_pos, line});
                    break;
                }
            }
        }

        translate_instruction(opcode, operands);

        if (opcode == Opcode::HALT) {
            size_t next_target = disa_bytecode.size();
            for (const auto& target : jump_targets) {
                if (target.first > current_bytecode_pos && target.first < next_target) {
                    next_target = target.first;
                }
            }

            if (next_target < disa_bytecode.size()) {
                current_bytecode_pos = next_target;
                continue;
            }

            break;
        }

        size_t instr_len = get_instruction_length(
            opcode_byte,
            disa_bytecode.data(),
            current_bytecode_pos,
            disa_bytecode.size()
        );
        current_bytecode_pos += instr_len;
    }

    emit_function_epilogue();

    return encoder.get_code();
}

// --- Instruction translation dispatch ---

void DISAToX86Compiler::translate_instruction(Opcode opcode, const uint8_t* operands) {
    switch (opcode) {
        case Opcode::NOP:   translate_nop(); break;
        case Opcode::HALT:  translate_halt(); break;

        // Arithmetic
        case Opcode::LOAD_IMM:
            // operands is validated before dispatch; operands[0] is always valid here
            translate_load_imm(operands[0], operands ? static_cast<uint64_t>(read_imm32(operands + 1)) : 0);
            break;
        case Opcode::LOAD_IMM64:
            // operands is validated before dispatch; operands[0] is always valid here
            translate_load_imm(operands[0], operands ? read_imm64_ptr(operands + 1) : 0);
            break;
        case Opcode::ADD:   translate_add(operands[0], operands[1]); break;
        case Opcode::SUB:   translate_sub(operands[0], operands[1]); break;
        case Opcode::MOV:   translate_mov(operands[0], operands[1]); break;
        case Opcode::CMP:   translate_cmp(operands[0], operands[1]); break;
        case Opcode::INC:   translate_inc(operands[0]); break;
        case Opcode::DEC:   translate_dec(operands[0]); break;
        case Opcode::NOT:   translate_not(operands[0]); break;
        case Opcode::MUL:   translate_mul(operands[0], operands[1]); break;
        case Opcode::DIV:   translate_div(operands[0], operands[1]); break;
        case Opcode::MOD:   translate_mod(operands[0], operands[1]); break;

        // Logic
        case Opcode::AND:   translate_and(operands[0], operands[1]); break;
        case Opcode::OR:    translate_or(operands[0], operands[1]); break;
        case Opcode::XOR:   translate_xor(operands[0], operands[1]); break;
        case Opcode::SHL:   translate_shl(operands[0], operands[1]); break;
        case Opcode::SHR:   translate_shr(operands[0], operands[1]); break;

        // 64-bit variants
        case Opcode::ADD64: translate_add(operands[0], operands[1]); break;
        case Opcode::SUB64: translate_sub(operands[0], operands[1]); break;
        case Opcode::MOV64: translate_mov(operands[0], operands[1]); break;
        case Opcode::CMP64: translate_cmp(operands[0], operands[1]); break;
        case Opcode::AND64: translate_and(operands[0], operands[1]); break;
        case Opcode::OR64:  translate_or(operands[0], operands[1]); break;
        case Opcode::XOR64: translate_xor(operands[0], operands[1]); break;
        case Opcode::INC64: translate_inc(operands[0]); break;
        case Opcode::DEC64: translate_dec(operands[0]); break;
        case Opcode::NOT64: translate_not(operands[0]); break;
        case Opcode::SHL64: translate_shl(operands[0], operands[1]); break;
        case Opcode::SHR64: translate_shr(operands[0], operands[1]); break;
        case Opcode::MUL64: translate_mul(operands[0], operands[2]); break;
        case Opcode::DIV64: translate_div(operands[1], operands[2]); break;
        case Opcode::MOD64: translate_mod(operands[0], operands[2]); break;

        // Extended register ops
        case Opcode::MOVEX: translate_mov(operands[0], operands[1]); break;
        case Opcode::ADDEX: translate_add(operands[0], operands[1]); break;
        case Opcode::SUBEX: translate_sub(operands[0], operands[1]); break;
        case Opcode::MULEX: translate_mul(operands[0], operands[1]); break;
        case Opcode::DIVEX: translate_div(operands[0], operands[1]); break;
        case Opcode::CMPEX: translate_cmp(operands[0], operands[1]); break;
        case Opcode::PUSHEX: translate_push(operands[0]); break;
        case Opcode::POPEX:  translate_pop(operands[0]); break;

        // Control flow
        case Opcode::JMP:  translate_jmp(operands ? read_imm32(operands) : 0); break;
        case Opcode::JZ:   translate_jz(operands ? read_imm32(operands) : 0); break;
        case Opcode::JNZ:  translate_jnz(operands ? read_imm32(operands) : 0); break;
        case Opcode::JS:   translate_js(operands ? read_imm32(operands) : 0); break;
        case Opcode::JNS:  translate_jns(operands ? read_imm32(operands) : 0); break;
        case Opcode::JC:   translate_jc(operands ? read_imm32(operands) : 0); break;
        case Opcode::JNC:  translate_jnc(operands ? read_imm32(operands) : 0); break;
        case Opcode::JO:   translate_jo(operands ? read_imm32(operands) : 0); break;
        case Opcode::JNO:  translate_jno(operands ? read_imm32(operands) : 0); break;
        case Opcode::JG:   translate_jg(operands ? read_imm32(operands) : 0); break;
        case Opcode::JL:   translate_jl(operands ? read_imm32(operands) : 0); break;
        case Opcode::JGE:  translate_jge(operands ? read_imm32(operands) : 0); break;
        case Opcode::JLE:  translate_jle(operands ? read_imm32(operands) : 0); break;
        case Opcode::CALL: translate_call(operands ? read_imm32(operands) : 0); break;
        case Opcode::RET:  translate_ret(); break;

        // Stack operations
        case Opcode::PUSH: translate_push(operands[0]); break;
        case Opcode::POP:  translate_pop(operands[0]); break;
        case Opcode::PUSH_ARG: translate_push_arg(); break;
        case Opcode::POP_ARG:  translate_pop_arg(); break;
        case Opcode::PUSH_FLAG: translate_push_flag(); break;
        case Opcode::POP_FLAG:  translate_pop_flag(); break;

        // Memory operations
        case Opcode::LOAD:   translate_load(operands[0], 0, operands ? static_cast<int32_t>(read_imm32(operands + 1)) : 0); break;
        case Opcode::STORE:  translate_store_imm(operands[0], operands + 1); break;
        case Opcode::LOADR:  translate_loadr(operands[0], operands[1]); break;
        case Opcode::STORER: translate_storer(operands[0], operands[1]); break;
        case Opcode::LEA:    translate_lea(operands[0], operands ? read_imm32(operands + 1) : 0); break;
        case Opcode::SWAP:   translate_swap(operands[0], operands[1]); break;

        // Load/Store extended
        case Opcode::LOADEX:
            translate_load(operands[0], 0, 0);
            break;
        case Opcode::STOREX:
            translate_store(0, 0, operands[0]);
            break;

        // I/O operations
        case Opcode::IN:
            translate_in(operands[0], operands[1]);
            break;
        case Opcode::OUT:
            translate_out(operands[0], operands[1]);
            break;
        case Opcode::INB:
            translate_inb(operands[0], operands[1]);
            break;
        case Opcode::OUTB:
            translate_outb(operands[0], operands[1]);
            break;
        case Opcode::INW:
            translate_inw(operands[0], operands[1]);
            break;
        case Opcode::OUTW:
            translate_outw(operands[0], operands[1]);
            break;
        case Opcode::INL:
            translate_inl(operands[0], operands[1]);
            break;
        case Opcode::OUTL:
            translate_outl(operands[0], operands[1]);
            break;
        case Opcode::INSTR:
            translate_instr(operands[0], operands[1]);
            break;
        case Opcode::OUTSTR:
            translate_outstr(operands[0], operands[1]);
            break;

        // DB — data definition, no code emitted (skipped by instruction length)
        case Opcode::DB:
        // DEBUG — assembler-level directive, no runtime code
        case Opcode::DEBUG:
            break;
        case Opcode::INT:
            translate_int80();
            break;

        // Mode control — no-op in native compilation (already x86-64)
        case Opcode::MODE32:
        case Opcode::MODE64:
            break;

        // SIMD - delegate to runtime
        case Opcode::MOVAPS:
        case Opcode::MOVUPS:
        case Opcode::ADDPS:
        case Opcode::SUBPS:
        case Opcode::MULPS:
        case Opcode::DIVPS:
        case Opcode::SQRTPS:
        case Opcode::MAXPS:
        case Opcode::MINPS:
        case Opcode::ANDPS:
        case Opcode::ORPS:
        case Opcode::XORPS:
        case Opcode::CMPPS:
        case Opcode::MOVAPD:
        case Opcode::MOVUPD:
        case Opcode::ADDPD:
        case Opcode::SUBPD:
        case Opcode::MULPD:
        case Opcode::DIVPD:
        case Opcode::SQRTPD:
        case Opcode::MAXPD:
        case Opcode::MINPD:
        case Opcode::ANDPD:
        case Opcode::ORPD:
        case Opcode::XORPD:
        case Opcode::CMPPD:
            emit_runtime_fallback("unimplemented_simd");
            break;

        // AVX - delegate to runtime
        case Opcode::VADDPS:
        case Opcode::VSUBPS:
        case Opcode::VMULPS:
        case Opcode::VDIVPS:
        case Opcode::VSQRTPS:
        case Opcode::VMAXPS:
        case Opcode::VMINPS:
        case Opcode::VANDPS:
        case Opcode::VORPS:
        case Opcode::VXORPS:
        case Opcode::VADDPD:
        case Opcode::VSUBPD:
        case Opcode::VMULPD:
        case Opcode::VDIVPD:
        case Opcode::VSQRTPD:
        case Opcode::VMINPD:
        case Opcode::VANDPD:
        case Opcode::VORPD:
        case Opcode::VXORPD:
        case Opcode::VADD:
        case Opcode::VMUL:
        case Opcode::VDOT:
        case Opcode::VMAX:
        case Opcode::VBROADCAST:
        case Opcode::VCMPGT:
        case Opcode::PACKB:
        case Opcode::UNPACKB:
            emit_runtime_fallback("unimplemented_avx");
            break;

        // FPU - delegate to runtime
        case Opcode::FLD:
            translate_fld(operands);
            break;
        case Opcode::FST:
            translate_fst(operands, false);
            break;
        case Opcode::FSTP:
            translate_fst(operands, true);
            break;
        case Opcode::FILD:
            translate_fild(operands);
            break;
        case Opcode::FIST:
            translate_fist(operands, false);
            break;
        case Opcode::FISTP:
            translate_fist(operands, true);
            break;
        case Opcode::FADD:
            translate_fadd(operands);
            break;
        case Opcode::FSUB:
            translate_fsub(operands);
            break;
        case Opcode::FMUL:
            translate_fmul(operands);
            break;
        case Opcode::FDIV:
            translate_fdiv(operands);
            break;
        case Opcode::FINIT:
            translate_finit();
            break;
        case Opcode::FCLEX:
            translate_fclex();
            break;
        case Opcode::FSTCW:
            translate_fstcw(operands);
            break;
        case Opcode::FLDCW:
            translate_fldcw(operands);
            break;
        case Opcode::FSTSW:
            translate_fstsw(operands);
            break;
        case Opcode::FSIN:
            translate_fpu_unary(0xD9, 0xFE);
            break;
        case Opcode::FCOS:
            translate_fpu_unary(0xD9, 0xFF);
            break;
        case Opcode::FTAN:
            translate_ftan();
            break;
        case Opcode::FSQRT:
            translate_fpu_unary(0xD9, 0xFA);
            break;
        case Opcode::FABS:
            translate_fpu_unary(0xD9, 0xE1);
            break;
        case Opcode::FCHS:
            translate_fpu_unary(0xD9, 0xE0);
            break;
        case Opcode::FCOMPP:
            translate_fcompp(false);
            break;
        case Opcode::FUCOMPP:
            translate_fcompp(true);
            break;

        // MMX - delegate to runtime
        case Opcode::MOVQ:
        case Opcode::PADDB:
        case Opcode::PADDW:
        case Opcode::PADDD:
        case Opcode::PSUBB:
        case Opcode::PSUBW:
        case Opcode::PSUBD:
        case Opcode::PCMPEQB:
        case Opcode::PCMPEQW:
        case Opcode::PCMPEQD:
        case Opcode::EMMS:
            emit_runtime_fallback("unimplemented_mmx");
            break;

        default:
            emit_runtime_fallback("unimplemented_opcode");
            break;
    }
}

// --- Prologue / Epilogue ---

void DISAToX86Compiler::emit_function_prologue() {
    encoder.emit_push_reg(X86Register::RBP);
    encoder.emit_mov_reg_reg(X86Register::RBP, X86Register::RSP);

    if (function_has_calls) {
        // Full save: all allocatable registers (caller state must survive CALL)
        encoder.emit_push_reg(X86Register::RAX);
        encoder.emit_push_reg(X86Register::RCX);
        encoder.emit_push_reg(X86Register::RDX);
        encoder.emit_push_reg(X86Register::RBX);
        encoder.emit_push_reg(X86Register::R8);
        encoder.emit_push_reg(X86Register::R9);
        encoder.emit_push_reg(X86Register::R10);
        encoder.emit_push_reg(X86Register::R11);
        encoder.emit_push_reg(X86Register::R12);
        encoder.emit_push_reg(X86Register::R13);
        encoder.emit_push_reg(X86Register::R14);
        encoder.emit_push_reg(X86Register::R15);
    } else {
        // Minimal save: only callee-saved
        encoder.emit_push_reg(X86Register::RBX);
        encoder.emit_push_reg(X86Register::R12);
        encoder.emit_push_reg(X86Register::R13);
        encoder.emit_push_reg(X86Register::R14);
        encoder.emit_push_reg(X86Register::R15);
    }

    encoder.emit_sub_reg_imm32(X86Register::RSP, SPILL_FRAME_SIZE);
}

void DISAToX86Compiler::emit_function_epilogue() {
    flush_all_registers();

    encoder.emit_add_reg_imm32(X86Register::RSP, SPILL_FRAME_SIZE);
    
    if (function_has_calls) {
        encoder.emit_pop_reg(X86Register::R15);
        encoder.emit_pop_reg(X86Register::R14);
        encoder.emit_pop_reg(X86Register::R13);
        encoder.emit_pop_reg(X86Register::R12);
        encoder.emit_pop_reg(X86Register::R11);
        encoder.emit_pop_reg(X86Register::R10);
        encoder.emit_pop_reg(X86Register::R9);
        encoder.emit_pop_reg(X86Register::R8);
        encoder.emit_pop_reg(X86Register::RBX);
        encoder.emit_pop_reg(X86Register::RDX);
        encoder.emit_pop_reg(X86Register::RCX);
        encoder.emit_pop_reg(X86Register::RAX);
    } else {
        encoder.emit_pop_reg(X86Register::R15);
        encoder.emit_pop_reg(X86Register::R14);
        encoder.emit_pop_reg(X86Register::R13);
        encoder.emit_pop_reg(X86Register::R12);
        encoder.emit_pop_reg(X86Register::RBX);
    }
    encoder.emit_pop_reg(X86Register::RBP);
    encoder.emit_ret();
}

// --- Register management ---

X86Register DISAToX86Compiler::acquire_physical(uint8_t virt_reg) {
    auto it = reg_state_map.find(virt_reg);
    if (it != reg_state_map.end()) {
        reg_alloc.update_lru_custom(virt_reg);
        return it->second.phys;
    }

    X86Register phys = reg_alloc.allocate_register(virt_reg);

    for (auto state_it = reg_state_map.begin(); state_it != reg_state_map.end(); ++state_it) {
        if (state_it->first == virt_reg || state_it->second.phys != phys) {
            continue;
        }

        if (state_it->second.dirty) {
            spill_virtual_value(state_it->first, phys);
        }
        reg_state_map.erase(state_it);
        break;
    }

    reg_state_map[virt_reg] = {phys, false, false};
    return phys;
}

int32_t DISAToX86Compiler::ensure_spill_slot(uint8_t virt_reg) {
    auto it = spill_slots.find(virt_reg);
    if (it != spill_slots.end()) {
        return it->second;
    }

    int32_t offset = -(MAX_SAVED_REGISTER_BYTES +
                       static_cast<int32_t>(spill_slots.size() + 1) * SPILL_SLOT_SIZE);
    spill_slots[virt_reg] = offset;
    slot_contains_valid[virt_reg] = false;
    return offset;
}

void DISAToX86Compiler::restore_virtual_value(uint8_t virt_reg, X86Register phys) {
    auto spill_it = spill_slots.find(virt_reg);
    auto valid_it = slot_contains_valid.find(virt_reg);
    if (spill_it != spill_slots.end() &&
        valid_it != slot_contains_valid.end() &&
        valid_it->second) {
        encoder.emit_mov_reg_mem(phys, X86Register::RBP, spill_it->second);
        return;
    }

    encoder.emit_xor_reg_reg(phys, phys);
}

void DISAToX86Compiler::spill_virtual_value(uint8_t virt_reg, X86Register phys) {
    int32_t offset = ensure_spill_slot(virt_reg);
    slot_contains_valid[virt_reg] = true;
    encoder.emit_mov_mem_reg(X86Register::RBP, offset, phys);
}

void DISAToX86Compiler::clear_cached_registers() {
    for (const auto& pair : reg_state_map) {
        reg_alloc.free_register(pair.first);
    }
    reg_state_map.clear();
}

void DISAToX86Compiler::load_register(uint8_t virt_reg, X86Register phys) {
    auto it = reg_state_map.find(virt_reg);
    if (it != reg_state_map.end() && it->second.loaded) return;

    restore_virtual_value(virt_reg, phys);
    reg_state_map[virt_reg] = {phys, true, false};
}

void DISAToX86Compiler::store_register(uint8_t virt_reg, X86Register phys) {
    auto it = reg_state_map.find(virt_reg);
    if (it == reg_state_map.end() || !it->second.dirty) return;

    spill_virtual_value(virt_reg, phys);
    it->second.dirty = false;
}

void DISAToX86Compiler::mark_dirty(uint8_t virt_reg) {
    auto it = reg_state_map.find(virt_reg);
    if (it != reg_state_map.end()) {
        it->second.dirty = true;
    }
}

void DISAToX86Compiler::flush_register(uint8_t virt_reg) {
    auto it = reg_state_map.find(virt_reg);
    if (it == reg_state_map.end()) return;

    if (it->second.dirty) {
        store_register(virt_reg, it->second.phys);
        it->second.dirty = false;
    }
    reg_state_map.erase(it);
    reg_alloc.free_register(virt_reg);
}

void DISAToX86Compiler::flush_all_registers() {
    for (auto& pair : reg_state_map) {
        if (pair.second.dirty) {
            spill_virtual_value(pair.first, pair.second.phys);
            pair.second.dirty = false;
        }
    }
}

X86Register DISAToX86Compiler::get_loaded_physical(uint8_t virt_reg) {
    acquire_physical(virt_reg);
    X86Register phys = reg_state_map[virt_reg].phys;
    load_register(virt_reg, phys);
    return phys;
}

X86Register DISAToX86Compiler::get_writable_physical(uint8_t virt_reg) {
    acquire_physical(virt_reg);
    X86Register phys = reg_state_map[virt_reg].phys;
    load_register(virt_reg, phys);
    mark_dirty(virt_reg);
    return phys;
}

// --- Data section support ---

void DISAToX86Compiler::emit_data_initialization(const std::vector<uint8_t>& bytecode,
                                                   uint32_t entry_point) {
    // Scan the entire bytecode for non-zero bytes and copy them into the
    // memory buffer at RSI. Memory is zeroed by the _start stub.
    size_t pos = 0;
    while (pos < bytecode.size()) {
        if (bytecode[pos] == 0) { pos++; continue; }
        size_t run_start = pos;
        size_t run_end = run_start;
        while (run_end < bytecode.size() && bytecode[run_end] != 0) run_end++;
        // Include one trailing zero byte so null-terminated strings work
        if (run_end < bytecode.size()) run_end++;
        for (size_t chunk_start = run_start; chunk_start < run_end; chunk_start += 8) {
            size_t chunk_end = std::min(chunk_start + 8, run_end);
            uint64_t val = 0;
            for (size_t i = 0; i < 8 && (chunk_start + i) < run_end; i++)
                val |= static_cast<uint64_t>(bytecode[chunk_start + i]) << (i * 8);
            if (val != 0) {
                encoder.emit_mov_reg_imm64(X86Register::RAX, val);
                encoder.emit_mov_mem_reg(X86Register::RSI, static_cast<int32_t>(chunk_start), X86Register::RAX);
            }
        }
        pos = run_end;
    }
}// --- I/O instruction translators ---
// Uses Linux syscall-based I/O for port 0 (console stdin/stdout).
// After each I/O operation, register cache is invalidated so the
// next instruction re-loads from frame-relative spill slots.

void DISAToX86Compiler::translate_out(uint8_t reg, uint8_t port) {
    if (port != 0x01 && port != 0x00) { emit_runtime_fallback("unimplemented_io_port"); return; }
    flush_all_registers();
    clear_cached_registers();
    
    encoder.emit_push_reg(X86Register::RDI);
    encoder.emit_push_reg(X86Register::RSI);

    restore_virtual_value(reg, X86Register::RAX);
    encoder.emit_push_reg(X86Register::RAX);
    encoder.emit_mov_reg_reg(X86Register::RSI, X86Register::RSP);
    encoder.emit_mov_reg_imm32(X86Register::RDI, 1);
    encoder.emit_mov_reg_imm32(X86Register::RDX, 1);
    encoder.emit_mov_reg_imm32(X86Register::RAX, 1);
    encoder.emit_syscall();
    encoder.emit_add_reg_imm32(X86Register::RSP, 8);
    encoder.emit_pop_reg(X86Register::RSI);
    encoder.emit_pop_reg(X86Register::RDI);

    clear_cached_registers();
}

void DISAToX86Compiler::translate_outb(uint8_t reg, uint8_t port) {
    translate_out(reg, port);
}

void DISAToX86Compiler::translate_outw(uint8_t reg, uint8_t port) {
    if (port != 0x01 && port != 0x00) { emit_runtime_fallback("unimplemented_io_port"); return; }
    flush_all_registers();
    clear_cached_registers();

    encoder.emit_push_reg(X86Register::RSI);
    restore_virtual_value(reg, X86Register::RAX);
    encoder.emit_push_reg(X86Register::RAX);
    encoder.emit_mov_reg_reg(X86Register::RSI, X86Register::RSP);
    encoder.emit_mov_reg_imm32(X86Register::RDI, 1);
    encoder.emit_mov_reg_imm32(X86Register::RDX, 2);
    encoder.emit_mov_reg_imm32(X86Register::RAX, 1);
    encoder.emit_syscall();
    encoder.emit_add_reg_imm32(X86Register::RSP, 8);
    encoder.emit_pop_reg(X86Register::RSI);
}

void DISAToX86Compiler::translate_outl(uint8_t reg, uint8_t port) {
    if (port != 0x01 && port != 0x00) { emit_runtime_fallback("unimplemented_io_port"); return; }
    flush_all_registers();
    clear_cached_registers();

    encoder.emit_push_reg(X86Register::RSI);
    restore_virtual_value(reg, X86Register::RAX);
    encoder.emit_push_reg(X86Register::RAX);
    encoder.emit_mov_reg_reg(X86Register::RSI, X86Register::RSP);
    encoder.emit_mov_reg_imm32(X86Register::RDI, 1);
    encoder.emit_mov_reg_imm32(X86Register::RDX, 4);
    encoder.emit_mov_reg_imm32(X86Register::RAX, 1);
    encoder.emit_syscall();
    encoder.emit_add_reg_imm32(X86Register::RSP, 8);
    encoder.emit_pop_reg(X86Register::RSI);
}

void DISAToX86Compiler::translate_outstr(uint8_t reg, uint8_t port) {
    if (port != 0x01 && port != 0x00) { emit_runtime_fallback("unimplemented_io_port"); return; }
    flush_all_registers();
    
    // Save scratch registers around the strlen/write sequence.
    encoder.emit_push_reg(X86Register::RDI);
    encoder.emit_push_reg(X86Register::RSI);
    
    // Get string offset from reg_state_map into R8, add memory base
    auto it_s = reg_state_map.find(reg);
    if (it_s != reg_state_map.end() && it_s->second.loaded) {
        encoder.emit_mov_reg_reg(X86Register::R8, it_s->second.phys);
    } else {
        restore_virtual_value(reg, X86Register::R8);
    }
    encoder.emit_add_reg_reg(X86Register::R8, X86Register::RSI);
    
    // strlen: scan for null at [R8]
    encoder.emit_mov_reg_reg(X86Register::RDI, X86Register::R8);
    encoder.emit_mov_reg_imm32(X86Register::RCX, 256);
    encoder.emit_xor_reg_reg(X86Register::RAX, X86Register::RAX);
    encoder.emit_cld();
    encoder.emit_repne_scasb();
    
    // length = (rdi - 1) - R8
    encoder.emit_mov_reg_reg(X86Register::RDX, X86Register::RDI);
    encoder.emit_dec_reg(X86Register::RDX);
    encoder.emit_sub_reg_reg(X86Register::RDX, X86Register::R8);
    
    // write(1, R8, len)
    encoder.emit_mov_reg_reg(X86Register::RSI, X86Register::R8);
    encoder.emit_mov_reg_imm32(X86Register::RDI, 1);
    encoder.emit_mov_reg_imm32(X86Register::RAX, 1);
    encoder.emit_syscall();
    
    // Restore RSI and RDI
    encoder.emit_pop_reg(X86Register::RSI);
    encoder.emit_pop_reg(X86Register::RDI);
    
    clear_cached_registers();
}

void DISAToX86Compiler::begin_fpu_sequence() {
    flush_all_registers();
    clear_cached_registers();
}

void DISAToX86Compiler::finish_fpu_sequence() {
    clear_cached_registers();
}

void DISAToX86Compiler::emit_x87_mem_op(uint8_t opcode, uint8_t reg_opcode) {
    encoder.emit_raw_byte(opcode);
    encoder.emit_raw_byte(static_cast<uint8_t>((reg_opcode & 0x7) << 3));
}

void DISAToX86Compiler::emit_vmaddr_in_rax(uint32_t vm_addr) {
    encoder.emit_mov_reg_imm32(X86Register::RAX, static_cast<int32_t>(vm_addr));
    encoder.emit_add_reg_reg(X86Register::RAX, X86Register::RSI);
}

void DISAToX86Compiler::emit_fpu_immediate_double(uint64_t raw_double, uint8_t opcode, uint8_t reg_opcode) {
    encoder.emit_mov_reg_imm64(X86Register::RAX, raw_double);
    encoder.emit_push_reg(X86Register::RAX);
    encoder.emit_mov_reg_reg(X86Register::RAX, X86Register::RSP);
    emit_x87_mem_op(opcode, reg_opcode);
    encoder.emit_add_reg_imm32(X86Register::RSP, 8);
}

void DISAToX86Compiler::emit_fpu_immediate_int32(int32_t value, uint8_t opcode, uint8_t reg_opcode) {
    encoder.emit_mov_reg_imm32(X86Register::RAX, value);
    encoder.emit_push_reg(X86Register::RAX);
    encoder.emit_mov_reg_reg(X86Register::RAX, X86Register::RSP);
    emit_x87_mem_op(opcode, reg_opcode);
    encoder.emit_add_reg_imm32(X86Register::RSP, 8);
}

void DISAToX86Compiler::emit_x87_raw_op(uint8_t opcode1, uint8_t opcode2) {
    encoder.emit_raw_byte(opcode1);
    encoder.emit_raw_byte(opcode2);
}

void DISAToX86Compiler::emit_fpu_compare_flags() {
    auto skip_less = encoder.create_label();

    encoder.emit_raw_byte(0xDF);
    encoder.emit_raw_byte(0xE0);  // FNSTSW AX

    encoder.emit_mov_reg_reg(X86Register::RDX, X86Register::RAX);
    encoder.emit_and_reg_imm32(X86Register::RAX, 0x4100);  // Preserve C0->CF and C3->ZF in AH
    encoder.emit_and_reg_imm32(X86Register::RDX, 0x4500);  // Extract C0/C2/C3 from status word
    encoder.emit_cmp_reg_imm32(X86Register::RDX, 0x0100);  // Less-than is the ordered C0-only case
    encoder.emit_jnz_label(skip_less);
    encoder.emit_or_reg_imm32(X86Register::RAX, 0x8000);   // Map less-than onto SF for VM JL/JLE rules
    encoder.bind_label(skip_less);

    encoder.emit_xor_reg_reg(X86Register::R11, X86Register::R11); // Clear OF before SAHF
    encoder.emit_raw_byte(0x9E);  // SAHF
}

void DISAToX86Compiler::translate_fld(const uint8_t* operands) {
    begin_fpu_sequence();

    uint8_t operand_type = operands ? operands[0] : 0xFF;
    if (operand_type == 0x01) {
        emit_vmaddr_in_rax(read_imm32(operands + 1));
        emit_x87_mem_op(0xDD, 0);  // FLD m64fp
    } else if (operand_type == 0x02) {
        emit_fpu_immediate_double(read_imm64_ptr(operands + 1), 0xDD, 0);
    } else {
        emit_runtime_fallback("unsupported_fld_operand");
        return;
    }

    finish_fpu_sequence();
}

void DISAToX86Compiler::translate_fst(const uint8_t* operands, bool pop_after_store) {
    begin_fpu_sequence();

    uint8_t operand_type = operands ? operands[0] : 0xFF;
    if (operand_type == 0x01) {
        emit_vmaddr_in_rax(read_imm32(operands + 1));
        emit_x87_mem_op(0xDD, pop_after_store ? 3 : 2);  // FST/FSTP m64fp
    } else {
        emit_runtime_fallback("unsupported_fst_operand");
        return;
    }

    finish_fpu_sequence();
}

void DISAToX86Compiler::translate_fild(const uint8_t* operands) {
    begin_fpu_sequence();

    uint8_t operand_type = operands ? operands[0] : 0xFF;
    if (operand_type == 0x00) {
        emit_fpu_immediate_int32(static_cast<int32_t>(read_imm32(operands + 1)), 0xDB, 0); // FILD m32int
    } else if (operand_type == 0x01) {
        emit_vmaddr_in_rax(read_imm32(operands + 1));
        emit_x87_mem_op(0xDB, 0);  // FILD m32int
    } else {
        emit_runtime_fallback("unsupported_fild_operand");
        return;
    }

    finish_fpu_sequence();
}

void DISAToX86Compiler::translate_fist(const uint8_t* operands, bool pop_after_store) {
    begin_fpu_sequence();

    uint8_t operand_type = operands ? operands[0] : 0xFF;
    if (operand_type == 0x01) {
        emit_vmaddr_in_rax(read_imm32(operands + 1));
        emit_x87_mem_op(0xDB, pop_after_store ? 3 : 2);  // FIST/FISTP m32int
    } else {
        emit_runtime_fallback("unsupported_fist_operand");
        return;
    }

    finish_fpu_sequence();
}

void DISAToX86Compiler::translate_fadd(const uint8_t* operands) {
    begin_fpu_sequence();
    uint8_t operand_type = operands ? operands[0] : 0xFF;
    if (operand_type == 0x01) {
        emit_vmaddr_in_rax(read_imm32(operands + 1));
        emit_x87_mem_op(0xDC, 0);  // FADD m64fp
    } else if (operand_type == 0x02) {
        emit_fpu_immediate_double(read_imm64_ptr(operands + 1), 0xDC, 0);
    } else {
        emit_runtime_fallback("unsupported_fadd_operand");
        return;
    }
    finish_fpu_sequence();
}

void DISAToX86Compiler::translate_fsub(const uint8_t* operands) {
    begin_fpu_sequence();
    uint8_t operand_type = operands ? operands[0] : 0xFF;
    if (operand_type == 0x01) {
        emit_vmaddr_in_rax(read_imm32(operands + 1));
        emit_x87_mem_op(0xDC, 4);  // FSUB m64fp
    } else if (operand_type == 0x02) {
        emit_fpu_immediate_double(read_imm64_ptr(operands + 1), 0xDC, 4);
    } else {
        emit_runtime_fallback("unsupported_fsub_operand");
        return;
    }
    finish_fpu_sequence();
}

void DISAToX86Compiler::translate_fmul(const uint8_t* operands) {
    begin_fpu_sequence();
    uint8_t operand_type = operands ? operands[0] : 0xFF;
    if (operand_type == 0x01) {
        emit_vmaddr_in_rax(read_imm32(operands + 1));
        emit_x87_mem_op(0xDC, 1);  // FMUL m64fp
    } else if (operand_type == 0x02) {
        emit_fpu_immediate_double(read_imm64_ptr(operands + 1), 0xDC, 1);
    } else {
        emit_runtime_fallback("unsupported_fmul_operand");
        return;
    }
    finish_fpu_sequence();
}

void DISAToX86Compiler::translate_fdiv(const uint8_t* operands) {
    begin_fpu_sequence();
    uint8_t operand_type = operands ? operands[0] : 0xFF;
    if (operand_type == 0x01) {
        emit_vmaddr_in_rax(read_imm32(operands + 1));
        emit_x87_mem_op(0xDC, 6);  // FDIV m64fp
    } else if (operand_type == 0x02) {
        emit_fpu_immediate_double(read_imm64_ptr(operands + 1), 0xDC, 6);
    } else {
        emit_runtime_fallback("unsupported_fdiv_operand");
        return;
    }
    finish_fpu_sequence();
}

void DISAToX86Compiler::translate_finit() {
    begin_fpu_sequence();
    encoder.emit_raw_byte(0xDB);
    encoder.emit_raw_byte(0xE3);  // FNINIT
    finish_fpu_sequence();
}

void DISAToX86Compiler::translate_fclex() {
    begin_fpu_sequence();
    encoder.emit_raw_byte(0xDB);
    encoder.emit_raw_byte(0xE2);  // FNCLEX
    finish_fpu_sequence();
}

void DISAToX86Compiler::translate_fstcw(const uint8_t* operands) {
    begin_fpu_sequence();
    emit_vmaddr_in_rax(read_imm32(operands));
    emit_x87_mem_op(0xD9, 7);  // FNSTCW m2byte
    finish_fpu_sequence();
}

void DISAToX86Compiler::translate_fldcw(const uint8_t* operands) {
    begin_fpu_sequence();
    emit_vmaddr_in_rax(read_imm32(operands));
    emit_x87_mem_op(0xD9, 5);  // FLDCW m2byte
    finish_fpu_sequence();
}

void DISAToX86Compiler::translate_fstsw(const uint8_t* operands) {
    begin_fpu_sequence();
    uint8_t operand_type = operands ? operands[0] : 0xFF;
    if (operand_type == 0x00) {
        emit_vmaddr_in_rax(read_imm32(operands + 1));
        emit_x87_mem_op(0xDD, 7);  // FNSTSW m2byte
    } else if (operand_type == 0x01) {
        encoder.emit_xor_reg_reg(X86Register::RAX, X86Register::RAX);
        encoder.emit_raw_byte(0xDF);
        encoder.emit_raw_byte(0xE0);  // FNSTSW AX
        spill_virtual_value(0, X86Register::RAX);
    } else {
        emit_runtime_fallback("unsupported_fstsw_operand");
        return;
    }
    finish_fpu_sequence();
}

void DISAToX86Compiler::translate_fpu_unary(uint8_t opcode1, uint8_t opcode2) {
    begin_fpu_sequence();
    emit_x87_raw_op(opcode1, opcode2);
    finish_fpu_sequence();
}

void DISAToX86Compiler::translate_ftan() {
    begin_fpu_sequence();
    emit_x87_raw_op(0xD9, 0xF2);  // FPTAN
    emit_x87_raw_op(0xDD, 0xD8);  // FSTP ST(0): discard the helper 1.0 push
    finish_fpu_sequence();
}

void DISAToX86Compiler::translate_fcompp(bool unordered_compare) {
    begin_fpu_sequence();
    if (unordered_compare) {
        emit_x87_raw_op(0xDA, 0xE9);  // FUCOMPP
    } else {
        emit_x87_raw_op(0xDE, 0xD9);  // FCOMPP
    }
    emit_fpu_compare_flags();
    finish_fpu_sequence();
}

void DISAToX86Compiler::translate_in(uint8_t reg, uint8_t port) {
    if (port != 0x01 && port != 0x00) { emit_runtime_fallback("unimplemented_io_port"); return; }
    flush_all_registers();
    clear_cached_registers();

    // read(0, &temp, 1); spill zero-extended byte into the canonical stack slot
    encoder.emit_push_reg(X86Register::RDI);
    encoder.emit_push_reg(X86Register::RSI);
    encoder.emit_sub_reg_imm32(X86Register::RSP, 8);  // allocate temp

    encoder.emit_xor_reg_reg(X86Register::RDI, X86Register::RDI);  // stdin
    encoder.emit_mov_reg_reg(X86Register::RSI, X86Register::RSP);  // buf = temp
    encoder.emit_mov_reg_imm32(X86Register::RDX, 1);
    encoder.emit_xor_reg_reg(X86Register::RAX, X86Register::RAX);  // sys_read
    encoder.emit_syscall();

    // Zero-extend byte into RAX
    encoder.emit_xor_reg_reg(X86Register::RAX, X86Register::RAX);
    // mov al, byte [rsp]
    encoder.emit_raw_byte(0x8A);
    encoder.emit_raw_byte(0x04);
    encoder.emit_raw_byte(0x24);

    spill_virtual_value(reg, X86Register::RAX);

    // Clean up: add rsp, 8 (remove temp), pop rsi, pop rdi
    encoder.emit_add_reg_imm32(X86Register::RSP, 8);
    encoder.emit_pop_reg(X86Register::RSI);
    encoder.emit_pop_reg(X86Register::RDI);
}

void DISAToX86Compiler::translate_inb(uint8_t reg, uint8_t port) {
    translate_in(reg, port);
}

void DISAToX86Compiler::translate_inw(uint8_t reg, uint8_t port) {
    if (port != 0x01 && port != 0x00) { emit_runtime_fallback("unimplemented_io_port"); return; }
    flush_all_registers();
    clear_cached_registers();

    // read(0, &temp, 2); spill temp[0] and temp[1] into consecutive virtual registers
    encoder.emit_push_reg(X86Register::RDI);
    encoder.emit_push_reg(X86Register::RSI);
    encoder.emit_sub_reg_imm32(X86Register::RSP, 8);

    encoder.emit_xor_reg_reg(X86Register::RDI, X86Register::RDI);
    encoder.emit_mov_reg_reg(X86Register::RSI, X86Register::RSP);
    encoder.emit_mov_reg_imm32(X86Register::RDX, 2);
    encoder.emit_xor_reg_reg(X86Register::RAX, X86Register::RAX);
    encoder.emit_syscall();

    // Load byte 0 - [RSP+0]=temp, [RSP+8]=RSI, [RSP+16]=RDI
    encoder.emit_xor_reg_reg(X86Register::RAX, X86Register::RAX);
    encoder.emit_raw_byte(0x8A);
    encoder.emit_raw_byte(0x04);
    encoder.emit_raw_byte(0x24);
    spill_virtual_value(reg, X86Register::RAX);

    // Load byte 1
    if (static_cast<size_t>(reg + 1) < 134) {
        encoder.emit_xor_reg_reg(X86Register::RAX, X86Register::RAX);
        encoder.emit_raw_byte(0x8A);
        encoder.emit_raw_byte(0x44);
        encoder.emit_raw_byte(0x24);
        encoder.emit_raw_byte(0x01);
        spill_virtual_value(reg + 1, X86Register::RAX);
    }

    encoder.emit_add_reg_imm32(X86Register::RSP, 8);
    encoder.emit_pop_reg(X86Register::RSI);
    encoder.emit_pop_reg(X86Register::RDI);
}

void DISAToX86Compiler::translate_inl(uint8_t reg, uint8_t port) {
    if (port != 0x01 && port != 0x00) { emit_runtime_fallback("unimplemented_io_port"); return; }
    flush_all_registers();
    clear_cached_registers();

    // read(0, &temp, 4); spill temp[0..3] into consecutive virtual registers
    encoder.emit_push_reg(X86Register::RDI);
    encoder.emit_push_reg(X86Register::RSI);
    encoder.emit_sub_reg_imm32(X86Register::RSP, 8);

    encoder.emit_xor_reg_reg(X86Register::RDI, X86Register::RDI);
    encoder.emit_mov_reg_reg(X86Register::RSI, X86Register::RSP);
    encoder.emit_mov_reg_imm32(X86Register::RDX, 4);
    encoder.emit_xor_reg_reg(X86Register::RAX, X86Register::RAX);
    encoder.emit_syscall();

    for (size_t i = 0; i < 4 && (reg + i) < 134; i++) {
        encoder.emit_xor_reg_reg(X86Register::RAX, X86Register::RAX);
        if (i == 0) {
            encoder.emit_raw_byte(0x8A);
            encoder.emit_raw_byte(0x04);
            encoder.emit_raw_byte(0x24);
        } else {
            encoder.emit_raw_byte(0x8A);
            encoder.emit_raw_byte(0x44);
            encoder.emit_raw_byte(0x24);
            encoder.emit_raw_byte(static_cast<uint8_t>(i));
        }
        spill_virtual_value(static_cast<uint8_t>(reg + i), X86Register::RAX);
    }

    encoder.emit_add_reg_imm32(X86Register::RSP, 8);
    encoder.emit_pop_reg(X86Register::RSI);
    encoder.emit_pop_reg(X86Register::RDI);
}

void DISAToX86Compiler::translate_instr(uint8_t reg, uint8_t port) {
    // Complex string input - delegate to runtime
    emit_runtime_fallback("unimplemented_instr");
}

// --- Individual instruction translators ---

void DISAToX86Compiler::translate_nop() {
    encoder.emit_nop();
}

void DISAToX86Compiler::translate_halt() {
    encoder.emit_mov_reg_imm32(X86Register::RDI, 0);
    encoder.emit_mov_reg_imm32(X86Register::RAX, 60);
    encoder.emit_syscall();
    encoder.emit_raw_byte(0xEB);
    encoder.emit_raw_byte(0xFE);
}

void DISAToX86Compiler::translate_load_imm(uint8_t reg, uint64_t immediate) {
    acquire_physical(reg);
    X86Register phys = reg_state_map[reg].phys;
    reg_state_map[reg] = {phys, true, false};
    encoder.emit_mov_reg_imm64(phys, immediate);
    spill_virtual_value(reg, phys);
}

void DISAToX86Compiler::translate_add(uint8_t dst_reg, uint8_t src_reg) {
    X86Register src = get_loaded_physical(src_reg);
    X86Register dst = get_writable_physical(dst_reg);
    if (reg_state_map[dst_reg].phys != dst) {
        dst = reg_state_map[dst_reg].phys;
    }
    encoder.emit_add_reg_reg(dst, src);
}

void DISAToX86Compiler::translate_sub(uint8_t dst_reg, uint8_t src_reg) {
    X86Register src = get_loaded_physical(src_reg);
    X86Register dst = get_writable_physical(dst_reg);
    if (reg_state_map[dst_reg].phys != dst) {
        dst = reg_state_map[dst_reg].phys;
    }
    encoder.emit_sub_reg_reg(dst, src);
}

void DISAToX86Compiler::translate_mov(uint8_t dst_reg, uint8_t src_reg) {
    if (dst_reg == src_reg) return;
    X86Register src = get_loaded_physical(src_reg);
    X86Register dst = reg_state_map[src_reg].phys;
    // Re-acquire for dst
    dst = get_writable_physical(dst_reg);
    // If src got evicted, re-load
    if (reg_state_map.find(src_reg) == reg_state_map.end() || !reg_state_map[src_reg].loaded) {
        restore_virtual_value(src_reg, dst);
    } else {
        src = reg_state_map[src_reg].phys;
        encoder.emit_mov_reg_reg(dst, src);
    }
}

void DISAToX86Compiler::translate_cmp(uint8_t reg1, uint8_t reg2) {
    X86Register r2 = get_loaded_physical(reg2);
    X86Register r1 = get_loaded_physical(reg1);
    encoder.emit_cmp_reg_reg(r1, r2);
}

void DISAToX86Compiler::translate_inc(uint8_t reg) {
    X86Register phys = get_writable_physical(reg);
    encoder.emit_inc_reg(phys);
}

void DISAToX86Compiler::translate_dec(uint8_t reg) {
    X86Register phys = get_writable_physical(reg);
    encoder.emit_dec_reg(phys);
}

void DISAToX86Compiler::translate_neg(uint8_t reg) {
    X86Register phys = get_writable_physical(reg);
    encoder.emit_neg_reg(phys);
}

void DISAToX86Compiler::translate_not(uint8_t reg) {
    X86Register phys = get_writable_physical(reg);
    encoder.emit_not_reg(phys);
}

void DISAToX86Compiler::translate_mul(uint8_t dst_reg, uint8_t src_reg) {
    X86Register src = get_loaded_physical(src_reg);
    X86Register dst = get_writable_physical(dst_reg);
    encoder.emit_imul_reg_reg(dst, src);
}

void DISAToX86Compiler::translate_div(uint8_t dst_reg, uint8_t src_reg) {
    // x86 DIV uses RDX:RAX / r/m -> RAX quotient, RDX remainder
    // VM semantics: dst = dst / src (single-width)
    // Save state, load operands into RAX/RCX, execute DIV, store back.
    flush_all_registers();
    clear_cached_registers();

    // Load dividend into RAX, divisor into RCX
    restore_virtual_value(dst_reg, X86Register::RAX);
    restore_virtual_value(src_reg, X86Register::RCX);

    // Zero-extend into RDX for DIV
    encoder.emit_xor_reg_reg(X86Register::RDX, X86Register::RDX);

    // DIV rcx → RAX = quotient, RDX = remainder
    encoder.emit_div_reg(X86Register::RCX);

    // Store quotient back to dst, remainder to virtual reg 2 (RDX)
    spill_virtual_value(dst_reg, X86Register::RAX);
    spill_virtual_value(2, X86Register::RDX);

    clear_cached_registers();
}

void DISAToX86Compiler::translate_mod(uint8_t dst_reg, uint8_t src_reg) {
    X86Register src = get_loaded_physical(src_reg);
    X86Register dst = get_loaded_physical(dst_reg);

    if (dst != X86Register::RAX) {
        encoder.emit_mov_reg_reg(X86Register::RAX, dst);
    }
    encoder.emit_xor_reg_reg(X86Register::RDX, X86Register::RDX);
    encoder.emit_div_reg(src);

    X86Register out = get_writable_physical(dst_reg);
    if (out != X86Register::RDX) {
        encoder.emit_mov_reg_reg(out, X86Register::RDX);
    }
}

void DISAToX86Compiler::translate_and(uint8_t dst_reg, uint8_t src_reg) {
    X86Register src = get_loaded_physical(src_reg);
    X86Register dst = get_writable_physical(dst_reg);
    encoder.emit_and_reg_reg(dst, src);
}

void DISAToX86Compiler::translate_or(uint8_t dst_reg, uint8_t src_reg) {
    X86Register src = get_loaded_physical(src_reg);
    X86Register dst = get_writable_physical(dst_reg);
    encoder.emit_or_reg_reg(dst, src);
}

void DISAToX86Compiler::translate_xor(uint8_t dst_reg, uint8_t src_reg) {
    X86Register src = get_loaded_physical(src_reg);
    X86Register dst = get_writable_physical(dst_reg);
    encoder.emit_xor_reg_reg(dst, src);
}

void DISAToX86Compiler::translate_shl(uint8_t dst_reg, uint8_t src_reg) {
    X86Register shift_phys = get_loaded_physical(src_reg);
    if (shift_phys != X86Register::RCX) {
        encoder.emit_mov_reg_reg(X86Register::RCX, shift_phys);
    }

    if (src_reg < 32) {
        // Shift amount is small, use immediate shift
        X86Register dst = get_writable_physical(dst_reg);
        encoder.emit_shl_reg_cl(dst);
    } else {
        // Use CL shift
        X86Register dst = get_writable_physical(dst_reg);
        encoder.emit_shl_reg_cl(dst);
    }
}

void DISAToX86Compiler::translate_shr(uint8_t dst_reg, uint8_t src_reg) {
    X86Register shift_phys = get_loaded_physical(src_reg);
    if (shift_phys != X86Register::RCX) {
        encoder.emit_mov_reg_reg(X86Register::RCX, shift_phys);
    }
    X86Register dst = get_writable_physical(dst_reg);
    encoder.emit_shr_reg_cl(dst);
}

// --- Memory operations ---

void DISAToX86Compiler::translate_load(uint8_t dst_reg, uint8_t addr_reg, int32_t offset) {
    X86Register dst = get_writable_physical(dst_reg);
    if (addr_reg == 0) {
        encoder.emit_movzx_reg_mem8(dst, X86Register::RSI, offset);
        return;
    }

    X86Register addr = get_loaded_physical(addr_reg);
    encoder.emit_movzx_reg_mem8(dst, addr, offset);
}

void DISAToX86Compiler::translate_store_imm(uint8_t src_reg, const uint8_t* operands) {
    // STORE src, imm_addr — store to VM address (direct)
    X86Register src = get_loaded_physical(src_reg);
    uint32_t vm_addr = read_imm32(operands);
    // RSI + vm_addr = real address
    encoder.emit_mov_mem8_reg8(X86Register::RSI, static_cast<int32_t>(vm_addr), src);
}

void DISAToX86Compiler::translate_store(uint8_t addr_reg, int32_t offset, uint8_t src_reg) {
    X86Register addr = get_loaded_physical(addr_reg);
    X86Register src = get_loaded_physical(src_reg);
    encoder.emit_mov_mem8_reg8(addr, offset, src);
}

void DISAToX86Compiler::translate_loadr(uint8_t dst_reg, uint8_t addr_reg) {
    X86Register addr = get_loaded_physical(addr_reg);
    X86Register dst = get_writable_physical(dst_reg);
    // VM address in addr_reg → real address = RSI + VM_address
    encoder.emit_add_reg_reg(addr, X86Register::RSI); // addr += memory base
    encoder.emit_movzx_reg_mem8(dst, addr, 0);        // LOADR loads one byte
    encoder.emit_sub_reg_reg(addr, X86Register::RSI); // restore VM addr
}

void DISAToX86Compiler::translate_storer(uint8_t addr_reg, uint8_t src_reg) {
    X86Register addr = get_loaded_physical(addr_reg);
    X86Register src = get_loaded_physical(src_reg);
    // VM address in addr_reg → real address = RSI + VM_address
    encoder.emit_add_reg_reg(addr, X86Register::RSI); // addr += memory base
    encoder.emit_mov_mem8_reg8(addr, 0, src);         // STORER writes one byte
    encoder.emit_sub_reg_reg(addr, X86Register::RSI); // restore addr
}

void DISAToX86Compiler::translate_lea(uint8_t dst_reg, uint32_t addr) {
    translate_load_imm(dst_reg, addr);
}

void DISAToX86Compiler::translate_swap(uint8_t reg, uint8_t addr_reg) {
    X86Register addr = get_loaded_physical(addr_reg);
    X86Register val = get_loaded_physical(reg);
    // Swap: load memory value, store register value, set register to old memory value
    encoder.emit_movzx_reg_mem8(X86Register::RDX, addr, 0);
    encoder.emit_mov_mem8_reg8(addr, 0, val);
    X86Register dst = get_writable_physical(reg);
    if (dst != X86Register::RDX) {
        encoder.emit_mov_reg_reg(dst, X86Register::RDX);
    }
}

// --- Stack operations ---

void DISAToX86Compiler::translate_push(uint8_t reg) {
    X86Register phys = get_loaded_physical(reg);
    encoder.emit_push_reg(phys);
}

void DISAToX86Compiler::translate_pop(uint8_t reg) {
    X86Register phys = get_writable_physical(reg);
    encoder.emit_pop_reg(phys);
}

void DISAToX86Compiler::translate_push_arg() {
    emit_runtime_fallback("push_arg");
}

void DISAToX86Compiler::translate_pop_arg() {
    emit_runtime_fallback("pop_arg");
}

void DISAToX86Compiler::translate_push_flag() {
    emit_runtime_fallback("push_flag");
}

void DISAToX86Compiler::translate_pop_flag() {
    emit_runtime_fallback("pop_flag");
}

// --- Control flow ---

void DISAToX86Compiler::translate_jmp(uint32_t target_address) {
    flush_all_registers();
    auto& label = get_or_create_label(target_address);
    encoder.emit_jmp_label(label);
}

void DISAToX86Compiler::translate_jz(uint32_t target_address) {
    flush_all_registers();
    auto& label = get_or_create_label(target_address);
    encoder.emit_jz_label(label);
}

void DISAToX86Compiler::translate_jnz(uint32_t target_address) {
    flush_all_registers();
    auto& label = get_or_create_label(target_address);
    encoder.emit_jnz_label(label);
}

void DISAToX86Compiler::translate_js(uint32_t target_address) {
    flush_all_registers();
    auto& label = get_or_create_label(target_address);
    encoder.emit_js_label(label);
}

void DISAToX86Compiler::translate_jns(uint32_t target_address) {
    flush_all_registers();
    auto& label = get_or_create_label(target_address);
    encoder.emit_jns_label(label);
}

void DISAToX86Compiler::translate_jc(uint32_t target_address) {
    flush_all_registers();
    auto& label = get_or_create_label(target_address);
    encoder.emit_jc_label(label);
}

void DISAToX86Compiler::translate_jnc(uint32_t target_address) {
    flush_all_registers();
    auto& label = get_or_create_label(target_address);
    encoder.emit_jnc_label(label);
}

void DISAToX86Compiler::translate_jo(uint32_t target_address) {
    flush_all_registers();
    auto& label = get_or_create_label(target_address);
    encoder.emit_jo_label(label);
}

void DISAToX86Compiler::translate_jno(uint32_t target_address) {
    flush_all_registers();
    auto& label = get_or_create_label(target_address);
    encoder.emit_jno_label(label);
}

void DISAToX86Compiler::translate_jg(uint32_t target_address) {
    flush_all_registers();
    auto& label = get_or_create_label(target_address);
    encoder.emit_jg_label(label);
}

void DISAToX86Compiler::translate_jl(uint32_t target_address) {
    flush_all_registers();
    auto& label = get_or_create_label(target_address);
    encoder.emit_jl_label(label);
}

void DISAToX86Compiler::translate_jge(uint32_t target_address) {
    flush_all_registers();
    auto& label = get_or_create_label(target_address);
    encoder.emit_jge_label(label);
}

void DISAToX86Compiler::translate_jle(uint32_t target_address) {
    flush_all_registers();
    auto& label = get_or_create_label(target_address);
    encoder.emit_jle_label(label);
}

void DISAToX86Compiler::translate_call(uint32_t target_address) {
    flush_all_registers();

    // Save caller's spill frame to stack so callee doesn't clobber it.
    // Allocate save area: sub rsp, SPILL_FRAME_SIZE
    encoder.emit_sub_reg_imm32(X86Register::RSP, SPILL_FRAME_SIZE);

    // Copy SPILL_FRAME_SIZE bytes from [RBP - SPILL_FRAME_SIZE] to [RSP]
    // using R10 as source, R11 as dest, RCX as counter
    encoder.emit_mov_reg_reg(X86Register::R10, X86Register::RBP);
    encoder.emit_sub_reg_imm32(X86Register::R10, SPILL_FRAME_SIZE);
    encoder.emit_mov_reg_reg(X86Register::R11, X86Register::RSP);
    encoder.emit_mov_reg_imm32(X86Register::RCX, SPILL_FRAME_SIZE / 8);
    auto copy_loop = encoder.create_label();
    auto copy_done = encoder.create_label();
    encoder.bind_label(copy_loop);
    encoder.emit_cmp_reg_imm32(X86Register::RCX, 0);
    encoder.emit_jz_label(copy_done);
    encoder.emit_mov_reg_mem(X86Register::RAX, X86Register::R10, 0);
    encoder.emit_mov_mem_reg(X86Register::R11, 0, X86Register::RAX);
    encoder.emit_add_reg_imm32(X86Register::R10, 8);
    encoder.emit_add_reg_imm32(X86Register::R11, 8);
    encoder.emit_dec_reg(X86Register::RCX);
    encoder.emit_jmp_label(copy_loop);
    encoder.bind_label(copy_done);

    // Save old RBP, set new frame pointer to saved spill area
    encoder.emit_push_reg(X86Register::RBP);
    encoder.emit_mov_reg_reg(X86Register::RBP, X86Register::RSP);

    // Allocate callee's spill frame
    encoder.emit_sub_reg_imm32(X86Register::RSP, SPILL_FRAME_SIZE);

    clear_cached_registers();  // callee starts fresh

    auto& label = get_or_create_label(target_address);
    if (label.bound) {
        int32_t offset = static_cast<int32_t>(label.position - (encoder.size() + 5));
        encoder.emit_call_rel32(offset);
    } else {
        label.unresolved_jumps.push_back(encoder.size() + 1);
        encoder.emit_call_rel32(0);
    }
}

void DISAToX86Compiler::translate_ret() {
    flush_all_registers();

    // Free callee's spill frame
    encoder.emit_add_reg_imm32(X86Register::RSP, SPILL_FRAME_SIZE);

    // Restore caller's RBP (points to saved spill frame)
    encoder.emit_pop_reg(X86Register::RBP);

    // Copy saved spill frame back to [RBP - SPILL_FRAME_SIZE]
    encoder.emit_mov_reg_reg(X86Register::R10, X86Register::RBP);
    encoder.emit_sub_reg_imm32(X86Register::R10, SPILL_FRAME_SIZE);
    encoder.emit_mov_reg_reg(X86Register::R11, X86Register::RSP);
    encoder.emit_mov_reg_imm32(X86Register::RCX, SPILL_FRAME_SIZE / 8);
    auto restore_loop = encoder.create_label();
    auto restore_done = encoder.create_label();
    encoder.bind_label(restore_loop);
    encoder.emit_cmp_reg_imm32(X86Register::RCX, 0);
    encoder.emit_jz_label(restore_done);
    encoder.emit_mov_reg_mem(X86Register::RAX, X86Register::R11, 0);
    encoder.emit_mov_mem_reg(X86Register::R10, 0, X86Register::RAX);
    encoder.emit_add_reg_imm32(X86Register::R10, 8);
    encoder.emit_add_reg_imm32(X86Register::R11, 8);
    encoder.emit_dec_reg(X86Register::RCX);
    encoder.emit_jmp_label(restore_loop);
    encoder.bind_label(restore_done);

    // Free the saved spill frame area
    encoder.emit_add_reg_imm32(X86Register::RSP, SPILL_FRAME_SIZE);

    clear_cached_registers();
    encoder.emit_ret();
}

// --- Jump target management ---

void DISAToX86Compiler::scan_for_jump_targets(const std::vector<uint8_t>& bytecode) {
    size_t pos = 0;
    while (pos < bytecode.size()) {
        uint8_t opcode_byte = bytecode[pos];
        Opcode opcode = static_cast<Opcode>(opcode_byte);

        // For jump instructions, record the target address
        switch (opcode) {
            case Opcode::JMP:
            case Opcode::JZ:
            case Opcode::JNZ:
            case Opcode::JS:
            case Opcode::JNS:
            case Opcode::JC:
            case Opcode::JNC:
            case Opcode::JO:
            case Opcode::JNO:
            case Opcode::JG:
            case Opcode::JL:
            case Opcode::JGE:
            case Opcode::JLE:
            case Opcode::CALL:
                function_has_calls = true;
                if (pos + 5 <= bytecode.size()) {
                    uint32_t target = 0;
                    for (int i = 0; i < 4; i++) {
                        target |= static_cast<uint32_t>(bytecode[pos + 1 + i]) << (i * 8);
                    }
                    if (target < bytecode.size()) {
                        get_or_create_label(target);
                    }
                }
                break;
            default:
                break;
        }

        size_t len = get_instruction_length(opcode_byte, bytecode.data(), pos, bytecode.size());
        pos += len;
    }
}

void DISAToX86Compiler::resolve_jump_targets() {
}

X86Encoder::Label& DISAToX86Compiler::get_or_create_label(size_t bytecode_address) {
    auto it = jump_targets.find(bytecode_address);
    if (it != jump_targets.end()) {
        return it->second.x86_label;
    }
    JumpTarget jt;
    jt.bytecode_address = bytecode_address;
    jt.x86_label = encoder.create_label();
    jump_targets[bytecode_address] = jt;
    return jump_targets[bytecode_address].x86_label;
}

// --- Runtime helpers ---

void DISAToX86Compiler::emit_runtime_fallback(const char* reason) {
    encoder.emit_int3();
}

void DISAToX86Compiler::emit_runtime_call(const char* function_name) {
    encoder.emit_int3();
}

void DISAToX86Compiler::emit_device_io_call(uint16_t device_id, bool is_input) {
    encoder.emit_int3();
}

// --- Stubs for other declaration methods ---

void DISAToX86Compiler::setup_function_prologue() {
    emit_function_prologue();
}

void DISAToX86Compiler::setup_function_epilogue() {
    emit_function_epilogue();
}

void DISAToX86Compiler::emit_stack_frame_setup(size_t local_vars_size) {
}

void DISAToX86Compiler::emit_stack_frame_teardown() {
}

void DISAToX86Compiler::optimize_register_usage() {
}

void DISAToX86Compiler::eliminate_redundant_moves() {
}

void DISAToX86Compiler::fold_constant_operations() {
}

void DISAToX86Compiler::print_compilation_stats() const {
    std::cout << "DISA Compilation Stats:\n";
    std::cout << "  Code size: " << encoder.size() << " bytes\n";
    std::cout << "  Jump targets: " << jump_targets.size() << "\n";
    std::cout << "  Functions: " << function_addresses.size() << "\n";
}

// --- Helper to extract immediate values from operands ---

uint32_t DISAToX86Compiler::read_imm32(const uint8_t* ptr) const {
    if (!ptr) return 0;
    uint32_t val = 0;
    for (int i = 0; i < 4; i++) {
        val |= static_cast<uint32_t>(ptr[i]) << (i * 8);
    }
    return val;
}

uint64_t DISAToX86Compiler::read_imm64_ptr(const uint8_t* ptr) const {
    if (!ptr) return 0;
    uint64_t val = 0;
    for (int i = 0; i < 8; i++) {
        val |= static_cast<uint64_t>(ptr[i]) << (i * 8);
    }
    return val;
}

// --- INT 0x80 syscall translation ---
// Maps the x86 32-bit INT 0x80 ABI used by the examples onto Linux x86-64
// syscalls while preserving Demi's VM register/memory model.
void DISAToX86Compiler::translate_int80() {
    flush_all_registers();
    clear_cached_registers();

    restore_virtual_value(0, X86Register::R11);  // x86 int80 syscall number

    auto label_exit = encoder.create_label();
    auto label_read = encoder.create_label();
    auto label_write = encoder.create_label();
    auto label_open = encoder.create_label();
    auto label_close = encoder.create_label();
    auto label_unknown = encoder.create_label();
    auto label_done = encoder.create_label();

    encoder.emit_cmp_reg_imm32(X86Register::R11, 1);
    encoder.emit_jz_label(label_exit);
    encoder.emit_cmp_reg_imm32(X86Register::R11, 3);
    encoder.emit_jz_label(label_read);
    encoder.emit_cmp_reg_imm32(X86Register::R11, 4);
    encoder.emit_jz_label(label_write);
    encoder.emit_cmp_reg_imm32(X86Register::R11, 5);
    encoder.emit_jz_label(label_open);
    encoder.emit_cmp_reg_imm32(X86Register::R11, 6);
    encoder.emit_jz_label(label_close);
    encoder.emit_jmp_label(label_unknown);

    encoder.bind_label(label_exit);
    restore_virtual_value(3, X86Register::RDI);  // EBX -> exit code
    encoder.emit_mov_reg_imm32(X86Register::RAX, 60);
    encoder.emit_syscall();

    encoder.bind_label(label_read);
    encoder.emit_push_reg(X86Register::RSI);
    restore_virtual_value(3, X86Register::RDI);  // EBX -> fd
    restore_virtual_value(1, X86Register::R8);   // ECX -> VM buffer offset
    encoder.emit_mov_reg_mem(X86Register::RSI, X86Register::RSP, 0);
    encoder.emit_add_reg_reg(X86Register::RSI, X86Register::R8);
    restore_virtual_value(2, X86Register::RDX);  // EDX -> count
    encoder.emit_xor_reg_reg(X86Register::RAX, X86Register::RAX); // read
    encoder.emit_syscall();
    spill_virtual_value(0, X86Register::RAX);    // EAX return value
    encoder.emit_pop_reg(X86Register::RSI);
    encoder.emit_jmp_label(label_done);

    encoder.bind_label(label_write);
    encoder.emit_push_reg(X86Register::RSI);
    restore_virtual_value(3, X86Register::RDI);  // EBX -> fd
    restore_virtual_value(1, X86Register::R8);   // ECX -> VM buffer offset
    encoder.emit_mov_reg_mem(X86Register::RSI, X86Register::RSP, 0);
    encoder.emit_add_reg_reg(X86Register::RSI, X86Register::R8);
    restore_virtual_value(2, X86Register::RDX);  // EDX -> count
    encoder.emit_mov_reg_imm32(X86Register::RAX, 1); // write
    encoder.emit_syscall();
    spill_virtual_value(0, X86Register::RAX);
    encoder.emit_pop_reg(X86Register::RSI);
    encoder.emit_jmp_label(label_done);

    encoder.bind_label(label_open);
    encoder.emit_push_reg(X86Register::RSI);
    restore_virtual_value(3, X86Register::R8);   // EBX -> VM path offset
    encoder.emit_mov_reg_mem(X86Register::RDI, X86Register::RSP, 0);
    encoder.emit_add_reg_reg(X86Register::RDI, X86Register::R8);
    restore_virtual_value(1, X86Register::RSI);  // ECX -> flags
    restore_virtual_value(2, X86Register::RDX);  // EDX -> mode
    encoder.emit_mov_reg_imm32(X86Register::RAX, 2); // open
    encoder.emit_syscall();
    spill_virtual_value(0, X86Register::RAX);
    encoder.emit_pop_reg(X86Register::RSI);
    encoder.emit_jmp_label(label_done);

    encoder.bind_label(label_close);
    restore_virtual_value(3, X86Register::RDI);  // EBX -> fd
    encoder.emit_mov_reg_imm32(X86Register::RAX, 3); // close
    encoder.emit_syscall();
    spill_virtual_value(0, X86Register::RAX);
    encoder.emit_jmp_label(label_done);

    encoder.bind_label(label_unknown);
    encoder.emit_mov_reg_imm32(X86Register::RAX, -38); // -ENOSYS
    spill_virtual_value(0, X86Register::RAX);

    encoder.bind_label(label_done);
    clear_cached_registers();
}

} // namespace CodeGen
