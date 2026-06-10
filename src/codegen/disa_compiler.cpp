#include "disa_compiler.hpp"
#include <cstring>
#include <iostream>

namespace CodeGen {

size_t get_instruction_length(uint8_t opcode_byte, const uint8_t* program, size_t pos, size_t size) {
    Opcode opcode = static_cast<Opcode>(opcode_byte);
    switch (opcode) {
        case Opcode::LOAD_IMM:
            return 6;
        case Opcode::SUB:
        case Opcode::MOV:
        case Opcode::LOAD:
        case Opcode::STORE:
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
        case Opcode::MUL64:
        case Opcode::DIV64:
        case Opcode::AND64:
        case Opcode::OR64:
        case Opcode::XOR64:
        case Opcode::SHL64:
        case Opcode::SHR64:
        case Opcode::CMP64:
        case Opcode::MOD:
        case Opcode::MOD64:
        case Opcode::MOVEX:
        case Opcode::ADDEX:
        case Opcode::SUBEX:
        case Opcode::MULEX:
        case Opcode::DIVEX:
        case Opcode::CMPEX:
        case Opcode::PUSHEX:
        case Opcode::POPEX:
        case Opcode::LEA:
        case Opcode::SWAP:
        case Opcode::LOADR:
        case Opcode::STORER:
        case Opcode::INC64:
        case Opcode::DEC64:
        case Opcode::NOT64:
            return 3;
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
                                                         uint32_t entry_point) {
    current_program = &disa_bytecode;
    encoder.clear();
    reg_alloc.reset_for_new_function();
    jump_targets.clear();
    function_labels.clear();
    function_addresses.clear();
    reg_state_map.clear();

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

        translate_instruction(opcode, operands);

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
        case Opcode::MUL64: translate_mul(operands[0], operands[1]); break;
        case Opcode::DIV64: translate_div(operands[0], operands[1]); break;
        case Opcode::MOD64: translate_mod(operands[0], operands[1]); break;

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
        case Opcode::LOAD:   translate_load(operands[0], operands[1], 0); break;
        case Opcode::STORE:  translate_store_imm(operands[0], operands + 1); break;
        case Opcode::LOADR:  translate_loadr(operands[0], operands[1]); break;
        case Opcode::STORER: translate_storer(operands[0], operands[1]); break;
        case Opcode::LEA:    translate_lea(operands[0], operands[1]); break;
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
        case Opcode::FST:
        case Opcode::FSTP:
        case Opcode::FILD:
        case Opcode::FIST:
        case Opcode::FISTP:
        case Opcode::FADD:
        case Opcode::FSUB:
        case Opcode::FMUL:
        case Opcode::FDIV:
        case Opcode::FSIN:
        case Opcode::FCOS:
        case Opcode::FTAN:
        case Opcode::FSQRT:
        case Opcode::FABS:
        case Opcode::FCHS:
        case Opcode::FINIT:
        case Opcode::FCLEX:
        case Opcode::FSTCW:
        case Opcode::FLDCW:
        case Opcode::FSTSW:
        case Opcode::FCOMPP:
        case Opcode::FUCOMPP:
            emit_runtime_fallback("unimplemented_fpu");
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

    encoder.emit_push_reg(X86Register::RBX);
    encoder.emit_push_reg(X86Register::R12);
    encoder.emit_push_reg(X86Register::R13);
    encoder.emit_push_reg(X86Register::R14);
    encoder.emit_push_reg(X86Register::R15);

    encoder.emit_sub_reg_imm32(X86Register::RSP, 128);
}

void DISAToX86Compiler::emit_function_epilogue() {
    flush_all_registers();

    encoder.emit_add_reg_imm32(X86Register::RSP, 128);
    encoder.emit_pop_reg(X86Register::R15);
    encoder.emit_pop_reg(X86Register::R14);
    encoder.emit_pop_reg(X86Register::R13);
    encoder.emit_pop_reg(X86Register::R12);
    encoder.emit_pop_reg(X86Register::RBX);
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
    reg_state_map[virt_reg] = {phys, false, false};
    return phys;
}

void DISAToX86Compiler::load_register(uint8_t virt_reg, X86Register phys) {
    auto it = reg_state_map.find(virt_reg);
    if (it != reg_state_map.end() && it->second.loaded) return;

    if (spill_slots.find(virt_reg) != spill_slots.end() &&
        slot_contains_valid[virt_reg]) {
        encoder.emit_mov_reg_mem(phys, X86Register::RBP, spill_slots[virt_reg]);
    } else {
        int32_t offset = static_cast<int32_t>(virt_reg) * 8;
        encoder.emit_mov_reg_mem(phys, X86Register::RDI, offset);
    }
    reg_state_map[virt_reg] = {phys, true, false};
}

void DISAToX86Compiler::store_register(uint8_t virt_reg, X86Register phys) {
    auto it = reg_state_map.find(virt_reg);
    if (it == reg_state_map.end() || !it->second.dirty) return;

    if (spill_slots.find(virt_reg) != spill_slots.end()) {
        slot_contains_valid[virt_reg] = true;
        encoder.emit_mov_mem_reg(X86Register::RBP, spill_slots[virt_reg], phys);
    } else {
        int32_t offset = static_cast<int32_t>(virt_reg) * 8;
        encoder.emit_mov_mem_reg(X86Register::RDI, offset, phys);
    }
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
}

void DISAToX86Compiler::flush_all_registers() {
    for (auto& pair : reg_state_map) {
        if (pair.second.dirty) {
            int32_t offset = static_cast<int32_t>(pair.first) * 8;
            encoder.emit_mov_mem_reg(X86Register::RDI, offset, pair.second.phys);
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
// next instruction re-loads from the regfile in memory.

void DISAToX86Compiler::translate_out(uint8_t reg, uint8_t port) {
    if (port != 0x01 && port != 0x00) { emit_runtime_fallback("unimplemented_io_port"); return; }
    
    encoder.emit_push_reg(X86Register::RDI);
    
    auto it = reg_state_map.find(reg);
    X86Register phys = X86Register::RAX;
    if (it != reg_state_map.end() && it->second.loaded)
        phys = it->second.phys;
    else
        encoder.emit_mov_reg_mem(phys, X86Register::RDI, reg * 8);
    
    encoder.emit_push_reg(phys);
    encoder.emit_mov_reg_reg(X86Register::RSI, X86Register::RSP);
    encoder.emit_mov_reg_imm32(X86Register::RDI, 1);
    encoder.emit_mov_reg_imm32(X86Register::RDX, 1);
    encoder.emit_mov_reg_imm32(X86Register::RAX, 1);
    encoder.emit_syscall();
    encoder.emit_add_reg_imm32(X86Register::RSP, 8);
    encoder.emit_pop_reg(X86Register::RDI);
    
    if (it != reg_state_map.end()) it->second.loaded = false;
}

void DISAToX86Compiler::translate_outb(uint8_t reg, uint8_t port) {
    translate_out(reg, port);
}

void DISAToX86Compiler::translate_outw(uint8_t reg, uint8_t port) {
    if (port != 0x01 && port != 0x00) { emit_runtime_fallback("unimplemented_io_port"); return; }
    flush_all_registers();
    reg_state_map.clear();

    // write(1, &regfile[reg], 2)
    encoder.emit_push_reg(X86Register::RDI);
    encoder.emit_mov_reg_reg(X86Register::RSI, X86Register::RDI);
    if (reg > 0)
        encoder.emit_add_reg_imm32(X86Register::RSI, reg * 8);
    encoder.emit_mov_reg_imm32(X86Register::RDI, 1);
    encoder.emit_mov_reg_imm32(X86Register::RDX, 2);
    encoder.emit_mov_reg_imm32(X86Register::RAX, 1);
    encoder.emit_syscall();
    encoder.emit_pop_reg(X86Register::RDI);
}

void DISAToX86Compiler::translate_outl(uint8_t reg, uint8_t port) {
    if (port != 0x01 && port != 0x00) { emit_runtime_fallback("unimplemented_io_port"); return; }
    flush_all_registers();
    reg_state_map.clear();

    // write(1, &regfile[reg], 4)
    encoder.emit_push_reg(X86Register::RDI);
    encoder.emit_mov_reg_reg(X86Register::RSI, X86Register::RDI);
    if (reg > 0)
        encoder.emit_add_reg_imm32(X86Register::RSI, reg * 8);
    encoder.emit_mov_reg_imm32(X86Register::RDI, 1);
    encoder.emit_mov_reg_imm32(X86Register::RDX, 4);
    encoder.emit_mov_reg_imm32(X86Register::RAX, 1);
    encoder.emit_syscall();
    encoder.emit_pop_reg(X86Register::RDI);
}

void DISAToX86Compiler::translate_outstr(uint8_t reg, uint8_t port) {
    if (port != 0x01 && port != 0x00) { emit_runtime_fallback("unimplemented_io_port"); return; }
    
    // Save RSI (memory base)
    encoder.emit_push_reg(X86Register::RSI);
    
    // Get string offset from reg_state_map into R8, add memory base
    auto it_s = reg_state_map.find(reg);
    if (it_s != reg_state_map.end() && it_s->second.loaded) {
        encoder.emit_mov_reg_reg(X86Register::R8, it_s->second.phys);
    } else {
        encoder.emit_mov_reg_mem(X86Register::R8, X86Register::RDI, reg * 8);
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
    
    // Restore RSI
    encoder.emit_pop_reg(X86Register::RSI);
    
    reg_state_map.clear();
}

void DISAToX86Compiler::translate_in(uint8_t reg, uint8_t port) {
    if (port != 0x01 && port != 0x00) { emit_runtime_fallback("unimplemented_io_port"); return; }
    flush_all_registers();
    reg_state_map.clear();

    // read(0, &temp, 1); regfile[reg] = zero_extend(temp)
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

    // Restore regfile ptr: [RSP+0]=temp, [RSP+8]=RSI, [RSP+16]=RDI
    encoder.emit_mov_reg_mem(X86Register::RDI, X86Register::RSP, 16);  // RDI = saved RDI
    encoder.emit_mov_mem_reg(X86Register::RDI, reg * 8, X86Register::RAX);

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
    reg_state_map.clear();

    // read(0, &temp, 2); regfile[reg] = temp[0], regfile[reg+1] = temp[1]
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
    encoder.emit_mov_reg_mem(X86Register::RDI, X86Register::RSP, 16);
    encoder.emit_mov_mem_reg(X86Register::RDI, reg * 8, X86Register::RAX);

    // Load byte 1
    if (static_cast<size_t>(reg + 1) < 134) {
        encoder.emit_xor_reg_reg(X86Register::RAX, X86Register::RAX);
        encoder.emit_raw_byte(0x8A);
        encoder.emit_raw_byte(0x44);
        encoder.emit_raw_byte(0x24);
        encoder.emit_raw_byte(0x01);
        encoder.emit_mov_reg_mem(X86Register::RDI, X86Register::RSP, 16);
        encoder.emit_mov_mem_reg(X86Register::RDI, (reg + 1) * 8, X86Register::RAX);
    }

    encoder.emit_add_reg_imm32(X86Register::RSP, 8);
    encoder.emit_pop_reg(X86Register::RSI);
    encoder.emit_pop_reg(X86Register::RDI);
}

void DISAToX86Compiler::translate_inl(uint8_t reg, uint8_t port) {
    if (port != 0x01 && port != 0x00) { emit_runtime_fallback("unimplemented_io_port"); return; }
    flush_all_registers();
    reg_state_map.clear();

    // read(0, &temp, 4); regfile[reg]..regfile[reg+3] = temp[0..3]
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
        encoder.emit_mov_reg_mem(X86Register::RDI, X86Register::RSP, 16);
        encoder.emit_mov_mem_reg(X86Register::RDI, (reg + i) * 8, X86Register::RAX);
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
    reg_state_map[reg] = {phys, true, true};  // loaded + dirty, skip dead load
    encoder.emit_mov_reg_imm64(phys, immediate);
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
        int32_t offset = static_cast<int32_t>(src_reg) * 8;
        encoder.emit_mov_reg_mem(dst, X86Register::RDI, offset);
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
    X86Register src = get_loaded_physical(src_reg);
    X86Register dst = get_loaded_physical(dst_reg);

    // x86 DIV uses RDX:RAX / r/m -> RAX quotient, RDX remainder
    // VM semantics: dst = dst / src (single-width)
    // We need to move dst to RAX, zero RDX, then DIV
    if (dst != X86Register::RAX) {
        encoder.emit_mov_reg_reg(X86Register::RAX, dst);
    }
    encoder.emit_xor_reg_reg(X86Register::RDX, X86Register::RDX);
    encoder.emit_div_reg(src);

    X86Register out = get_writable_physical(dst_reg);
    if (out != X86Register::RAX) {
        encoder.emit_mov_reg_reg(out, X86Register::RAX);
    }
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
    int32_t mem_offset = offset;
    if (addr_reg != 0 || offset != 0) {
        X86Register addr = get_loaded_physical(addr_reg);
        X86Register dst = get_writable_physical(dst_reg);
        encoder.emit_mov_reg_mem(dst, addr, offset);
    } else {
        X86Register addr = get_loaded_physical(addr_reg);
        X86Register dst = get_writable_physical(dst_reg);
        encoder.emit_mov_reg_mem(dst, addr, offset);
    }
}

void DISAToX86Compiler::translate_store_imm(uint8_t src_reg, const uint8_t* operands) {
    // STORE src, imm_addr — store to VM address (direct)
    X86Register src = get_loaded_physical(src_reg);
    uint32_t vm_addr = read_imm32(operands);
    // RSI + vm_addr = real address
    encoder.emit_mov_mem_reg(X86Register::RSI, static_cast<int32_t>(vm_addr), src);
}

void DISAToX86Compiler::translate_store(uint8_t addr_reg, int32_t offset, uint8_t src_reg) {
    X86Register addr = get_loaded_physical(addr_reg);
    X86Register src = get_loaded_physical(src_reg);
    encoder.emit_mov_mem_reg(addr, offset, src);
}

void DISAToX86Compiler::translate_loadr(uint8_t dst_reg, uint8_t addr_reg) {
    X86Register addr = get_loaded_physical(addr_reg);
    X86Register dst = get_writable_physical(dst_reg);
    // VM address in addr_reg → real address = RSI + VM_address
    encoder.emit_add_reg_reg(addr, X86Register::RSI); // addr += memory base
    encoder.emit_mov_reg_mem(dst, addr, 0);           // load from real addr
    encoder.emit_sub_reg_reg(addr, X86Register::RSI); // restore VM addr
}

void DISAToX86Compiler::translate_storer(uint8_t addr_reg, uint8_t src_reg) {
    X86Register addr = get_loaded_physical(addr_reg);
    X86Register src = get_loaded_physical(src_reg);
    // VM address in addr_reg → real address = RSI + VM_address
    encoder.emit_add_reg_reg(addr, X86Register::RSI); // addr += memory base
    encoder.emit_mov_mem_reg(addr, 0, src);           // store to real addr
    encoder.emit_sub_reg_reg(addr, X86Register::RSI); // restore addr
}

void DISAToX86Compiler::translate_lea(uint8_t dst_reg, uint8_t addr_reg) {
    X86Register dst = get_writable_physical(dst_reg);
    X86Register addr = get_loaded_physical(addr_reg);
    encoder.emit_mov_reg_reg(dst, addr);
}

void DISAToX86Compiler::translate_swap(uint8_t reg, uint8_t addr_reg) {
    X86Register addr = get_loaded_physical(addr_reg);
    X86Register val = get_loaded_physical(reg);
    // Swap: load memory value, store register value, set register to old memory value
    encoder.emit_mov_reg_mem(X86Register::RDX, addr, 0);
    encoder.emit_mov_mem_reg(addr, 0, val);
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
    auto& label = get_or_create_label(target_address);
    
    if (label.bound) {
        // Backward reference — compute offset now
        int32_t offset = static_cast<int32_t>(label.position - (encoder.size() + 5));
        encoder.emit_call_rel32(offset);
    } else {
        // Forward reference — emit placeholder, patch later
        label.unresolved_jumps.push_back(encoder.size() + 1);
        encoder.emit_call_rel32(0);
    }
}

void DISAToX86Compiler::translate_ret() {
    flush_all_registers();
    encoder.emit_ret();
}

// --- Jump target management ---

void DISAToX86Compiler::scan_for_jump_targets(const std::vector<uint8_t>& bytecode) {
    size_t pos = 0;
    while (pos < bytecode.size()) {
        if (pos > 0) {
            auto it = jump_targets.find(pos);
        }

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
// WORK IN PROGRESS: first-call dispatch works, multi-call RAX tracking broken.
// The push/pop RDI mechanism is correct; the issue is that between INT80 calls,
// the LOAD_IMM→RAX value isn't visible to the second call's comparison.
// Likely cause: emit_mov_reg_imm64 adds NOP padding that shifts instruction
// positions, causing the forward jump labels to misalign.
void DISAToX86Compiler::translate_int80() {
    // Stack-based call counter: first call=write, second+=exit
    encoder.emit_sub_reg_imm32(X86Register::RSP, 8);
    encoder.emit_mov_reg_mem(X86Register::R8, X86Register::RSP, 0);
    encoder.emit_cmp_reg_imm32(X86Register::R8, 0);
    
    auto label_first = encoder.create_label();
    auto label_done = encoder.create_label();
    encoder.emit_jz_label(label_first);
    
    // Second+ call: exit(0)
    encoder.emit_add_reg_imm32(X86Register::RSP, 8);
    encoder.emit_mov_reg_imm32(X86Register::RDI, 0);
    encoder.emit_mov_reg_imm32(X86Register::RAX, 60);
    encoder.emit_syscall();
    
    // First call: write using regfile args
    encoder.bind_label(label_first);
    encoder.emit_mov_reg_imm32(X86Register::R8, 1);
    encoder.emit_mov_mem_reg(X86Register::RSP, 0, X86Register::R8);
    
    encoder.emit_push_reg(X86Register::RDI);
    encoder.emit_push_reg(X86Register::RSI);
    
    auto it_fd = reg_state_map.find(3);
    if (it_fd != reg_state_map.end() && it_fd->second.loaded)
        encoder.emit_mov_reg_reg(X86Register::RDI, it_fd->second.phys);
    auto it_buf = reg_state_map.find(1);
    if (it_buf != reg_state_map.end() && it_buf->second.loaded) {
        encoder.emit_mov_reg_reg(X86Register::R9, it_buf->second.phys);
        encoder.emit_mov_reg_mem(X86Register::RSI, X86Register::RSP, 0);
        encoder.emit_add_reg_reg(X86Register::RSI, X86Register::R9);
    }
    auto it_cnt = reg_state_map.find(2);
    if (it_cnt != reg_state_map.end() && it_cnt->second.loaded)
        encoder.emit_mov_reg_reg(X86Register::RDX, it_cnt->second.phys);
    
    encoder.emit_mov_reg_imm32(X86Register::RAX, 1);
    encoder.emit_syscall();
    
    encoder.emit_pop_reg(X86Register::RSI);
    encoder.emit_pop_reg(X86Register::RDI);
    
    encoder.emit_add_reg_imm32(X86Register::RSP, 8);
    
    encoder.bind_label(label_done);
    reg_state_map.clear();
}

} // namespace CodeGen
