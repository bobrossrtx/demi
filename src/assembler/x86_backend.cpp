#include "x86_backend.hpp"

#include "elf32_writer.hpp"
#include "elf64_writer.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>

namespace Assembler {

namespace {

std::string upper_copy(const std::string& value) {
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return result;
}

void append_u32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void append_i32(std::vector<uint8_t>& out, int32_t value) {
    append_u32(out, static_cast<uint32_t>(value));
}

bool fits_i8(int64_t value) {
    return value >= -128 && value <= 127;
}

std::optional<uint8_t> jcc_opcode(const std::string& mnemonic) {
    if (mnemonic == "JZ" || mnemonic == "JE") return 0x84;
    if (mnemonic == "JNZ" || mnemonic == "JNE") return 0x85;
    if (mnemonic == "JG") return 0x8F;
    if (mnemonic == "JGE") return 0x8D;
    if (mnemonic == "JL") return 0x8C;
    if (mnemonic == "JLE") return 0x8E;
    if (mnemonic == "JC") return 0x82;
    if (mnemonic == "JNC") return 0x83;
    if (mnemonic == "JO") return 0x80;
    if (mnemonic == "JNO") return 0x81;
    if (mnemonic == "JS") return 0x88;
    if (mnemonic == "JNS") return 0x89;
    return std::nullopt;
}

struct EncodedMemoryOperand {
    std::vector<uint8_t> bytes;
    size_t displacement_offset = 0;
    size_t displacement_size = 0;
    bool has_symbol_relocation = false;
    int64_t relocation_addend = 0;
    std::string relocation_symbol;
};

std::optional<uint8_t> parse_reg_id(const std::string& upper) {
    if (upper == "EAX" || upper == "RAX") return 0;
    if (upper == "ECX" || upper == "RCX") return 1;
    if (upper == "EDX" || upper == "RDX") return 2;
    if (upper == "EBX" || upper == "RBX") return 3;
    if (upper == "ESP" || upper == "RSP") return 4;
    if (upper == "EBP" || upper == "RBP") return 5;
    if (upper == "ESI" || upper == "RSI") return 6;
    if (upper == "EDI" || upper == "RDI") return 7;
    return std::nullopt;
}

EncodedMemoryOperand encode_memory_operand32(
    const IRMemoryOperand& memory,
    uint8_t reg_field,
    uint8_t opcode,
    bool is64,
    std::vector<std::string>& errors) {
    EncodedMemoryOperand encoded;

    if (memory.base && memory.symbol) {
        errors.push_back("x86 backend does not yet support base register plus symbolic displacement");
        return encoded;
    }

    if (is64) encoded.bytes.push_back(0x48);
    encoded.bytes.push_back(opcode);

    auto emit_disp8 = [&](uint8_t modrm, int8_t disp) {
        encoded.bytes.push_back(modrm);
        encoded.displacement_offset = encoded.bytes.size();
        encoded.displacement_size = 1;
        encoded.bytes.push_back(static_cast<uint8_t>(disp));
    };

    auto emit_disp32 = [&](uint8_t modrm, int32_t disp) {
        encoded.bytes.push_back(modrm);
        encoded.displacement_offset = encoded.bytes.size();
        encoded.displacement_size = 4;
        append_i32(encoded.bytes, disp);
    };

    auto sib_byte = [](uint8_t scale, uint8_t idx, uint8_t base) -> uint8_t {
        uint8_t s = 0;
        if (scale == 2) s = 1;
        else if (scale == 4) s = 2;
        else if (scale == 8) s = 3;
        return (s << 6) | ((idx & 0x7) << 3) | (base & 0x7);
    };

    // Symbol-only memory (no base, no index)
    if (memory.symbol && !memory.base && !memory.index) {
        encoded.bytes.push_back(static_cast<uint8_t>((0b00 << 6) | ((reg_field & 0x7) << 3) | 0b101));
        encoded.displacement_offset = encoded.bytes.size();
        encoded.displacement_size = 4;
        append_i32(encoded.bytes, static_cast<int32_t>(memory.displacement));
        encoded.has_symbol_relocation = true;
        encoded.relocation_symbol = *memory.symbol;
        encoded.relocation_addend = memory.displacement;
        return encoded;
    }

    // Indexed addressing: [base + index*scale + disp] or [index*scale + disp]
    if (memory.index) {
        const auto idx = parse_reg_id(upper_copy(*memory.index));
        if (!idx) { errors.push_back("unsupported index register"); return encoded; }

        const bool has_base = memory.base.has_value();
        uint8_t base_id = 5; // default: no base
        if (has_base) {
            const auto b = parse_reg_id(upper_copy(*memory.base));
            if (!b) { errors.push_back("unsupported base register"); return encoded; }
            base_id = *b & 0x7;
        }

        const uint8_t sib = sib_byte(memory.scale, *idx, base_id);
        const uint8_t modrm_base = static_cast<uint8_t>((0b00 << 6) | ((reg_field & 0x7) << 3) | 0b100);
        const int64_t disp = memory.displacement;

        // No base register: must use mod=00 with disp32
        if (!has_base) {
            encoded.bytes.push_back(modrm_base);
            encoded.bytes.push_back(sib);
            encoded.displacement_offset = encoded.bytes.size();
            encoded.displacement_size = 4;
            append_i32(encoded.bytes, static_cast<int32_t>(disp));
            return encoded;
        }

        // Special: EBP as base in SIB requires disp even for disp=0
        const bool base_is_ebp = has_base && base_id == 5;

        if (disp == 0 && !base_is_ebp) {
            encoded.bytes.push_back(modrm_base);
            encoded.bytes.push_back(sib);
        } else if (fits_i8(disp)) {
            encoded.bytes.push_back(static_cast<uint8_t>((0b01 << 6) | ((reg_field & 0x7) << 3) | 0b100));
            encoded.bytes.push_back(sib);
            encoded.displacement_offset = encoded.bytes.size();
            encoded.displacement_size = 1;
            encoded.bytes.push_back(static_cast<uint8_t>(static_cast<int8_t>(disp)));
        } else {
            encoded.bytes.push_back(static_cast<uint8_t>((0b10 << 6) | ((reg_field & 0x7) << 3) | 0b100));
            encoded.bytes.push_back(sib);
            encoded.displacement_offset = encoded.bytes.size();
            encoded.displacement_size = 4;
            append_i32(encoded.bytes, static_cast<int32_t>(disp));
        }
        return encoded;
    }

    // Base-only addressing (no index)
    if (!memory.base) {
        errors.push_back("x86 backend requires a base register, index, or symbol for memory operand");
        return encoded;
    }

    const auto base = parse_reg_id(upper_copy(*memory.base));
    if (!base) {
        errors.push_back("x86 backend does not support that base register in memory operand");
        return encoded;
    }

    const int64_t disp = memory.displacement;
    const bool needs_sib = *base == 4;
    const uint8_t rm_field = needs_sib ? 4 : *base;

    if (disp == 0 && *base != 5) {
        encoded.bytes.push_back(static_cast<uint8_t>((0b00 << 6) | ((reg_field & 0x7) << 3) | rm_field));
        if (needs_sib) encoded.bytes.push_back(0x24);
        return encoded;
    }

    if (fits_i8(disp)) {
        emit_disp8(static_cast<uint8_t>((0b01 << 6) | ((reg_field & 0x7) << 3) | rm_field), static_cast<int8_t>(disp));
        if (needs_sib) encoded.bytes.insert(encoded.bytes.end() - 1, 0x24);
        return encoded;
    }

    emit_disp32(static_cast<uint8_t>((0b10 << 6) | ((reg_field & 0x7) << 3) | rm_field), static_cast<int32_t>(disp));
    if (needs_sib) encoded.bytes.insert(encoded.bytes.end() - 4, 0x24);
    return encoded;
}

} // namespace

X86Backend::X86Backend(X86BackendMode mode) : mode_(mode) {}

IRTarget X86Backend::target() const {
    return mode_ == X86BackendMode::X86_32 ? IRTarget::X86Elf32 : IRTarget::X86Elf64;
}

BackendArtifact X86Backend::emit(const IRProgram& program) {
    BackendArtifact artifact;
    std::vector<uint8_t> text_bytes;
    std::unordered_map<uint64_t, uint64_t> text_offset_map;

    // Collect function entry instruction indices
    std::unordered_map<uint64_t, std::string> function_entries; // instruction_index -> name
    for (const auto& symbol : program.symbols) {
        if (symbol.is_function && symbol.defined && symbol.section == IRSectionKind::Text) {
            // Find the instruction index at this symbol's logical offset
            for (size_t idx = 0; idx < program.instructions.size(); ++idx) {
                if (program.instructions[idx].section == IRSectionKind::Text) {
                    // Logical offset is instruction count before this one in text
                    size_t logical_offset = 0;
                    for (size_t j = 0; j < idx; ++j)
                        if (program.instructions[j].section == IRSectionKind::Text) ++logical_offset;
                    if (logical_offset == symbol.offset) {
                        function_entries[idx] = symbol.name;
                        break;
                    }
                }
            }
        }
    }

    // Track function state for epilogue insertion
    bool in_function = false;

    const size_t prologue_size = is_64bit_mode() ? 4 : 3;  // PUSH EBP + MOV EBP,ESP (or RBP variant)
    const size_t epilogue_size = 1;  // LEAVE

    for (size_t instruction_index = 0; instruction_index < program.instructions.size(); ++instruction_index) {
        size_t base_size = 0;
        if (function_entries.count(instruction_index)) {
            base_size += prologue_size;
            in_function = true;
        }
        text_offset_map[static_cast<uint64_t>(instruction_index)] = text_bytes.size() + base_size;
        const auto estimated = estimate_instruction_size(program.instructions[instruction_index], artifact.errors);
        if (!artifact.ok()) return artifact;
        if (in_function && program.instructions[instruction_index].mnemonic == "RET") {
            text_bytes.resize(text_bytes.size() + estimated + epilogue_size);
        } else {
            text_bytes.resize(text_bytes.size() + estimated + base_size);
        }
    }

    text_bytes.clear();
    auto adjusted_program = adjust_program_for_encoded_text(program, text_offset_map);

    // Fix function symbol offsets: they point to instruction start,
    // but should point to prologue start
    for (auto& symbol : adjusted_program.symbols) {
        if (symbol.is_function && symbol.defined && symbol.section == IRSectionKind::Text) {
            symbol.offset = (symbol.offset >= prologue_size) ? symbol.offset - prologue_size : 0;
        }
    }

    std::unordered_map<std::string, uint64_t> text_symbol_offsets;
    for (const auto& symbol : adjusted_program.symbols) {
        if (symbol.defined && symbol.section == IRSectionKind::Text) {
            uint64_t offset = symbol.offset;
            if (symbol.is_function) {
                offset = (offset >= prologue_size) ? offset - prologue_size : 0;
            }
            text_symbol_offsets[symbol.name] = offset;
        }
    }

    std::vector<IRRelocation> text_relocations;

    in_function = false;

    for (size_t instruction_index = 0; instruction_index < program.instructions.size(); ++instruction_index) {
        const auto& instruction = program.instructions[instruction_index];
        if (instruction.section != IRSectionKind::Text) {
            artifact.errors.push_back("x86 backend only supports .text instructions right now");
            return artifact;
        }

        // Emit prologue at function entry
        if (function_entries.count(instruction_index)) {
            in_function = true;
            if (is_64bit_mode()) {
                // PUSH RBP (55) + MOV RBP, RSP (48 89 E5)
                text_bytes.push_back(0x55);
                text_bytes.push_back(0x48);
                text_bytes.push_back(0x89);
                text_bytes.push_back(0xE5);
            } else {
                // PUSH EBP (55) + MOV EBP, ESP (89 E5)
                text_bytes.push_back(0x55);
                text_bytes.push_back(0x89);
                text_bytes.push_back(0xE5);
            }
        }

        // Emit epilogue (LEAVE) before RET in function bodies
        if (in_function && instruction.mnemonic == "RET") {
            text_bytes.push_back(0xC9); // LEAVE
        }

        const auto instruction_offset = text_offset_map[static_cast<uint64_t>(instruction_index)];
        auto encoded = encode_instruction(instruction, instruction_offset, text_symbol_offsets, artifact.errors);
        if (!artifact.ok()) {
            return artifact;
        }
        text_relocations.insert(text_relocations.end(), encoded.relocations.begin(), encoded.relocations.end());
        text_bytes.insert(text_bytes.end(), encoded.bytes.begin(), encoded.bytes.end());
    }

    if (mode_ == X86BackendMode::X86_32) {
        ELF32ObjectWriter writer;
        artifact.bytes = writer.write_object(adjusted_program, artifact.errors, &text_bytes, &text_relocations);
        return artifact;
    }

    ELF64ObjectWriter writer;
    artifact.bytes = writer.write_object(adjusted_program, artifact.errors, &text_bytes, &text_relocations);
    return artifact;
}

std::optional<uint8_t> X86Backend::encode_register_id(const std::string& name) const {
    return parse_reg_id(upper_copy(name));
}

size_t compute_memory_operand_size(const IRMemoryOperand& mem, std::vector<std::string>& errors) {
    if (mem.symbol && !mem.base && !mem.index) {
        return 6; // opcode + modrm + disp32
    }
    if (mem.index) {
        // Indexed: opcode + modrm + sib + disp
        size_t size = 3; // opcode + modrm + sib
        if (!mem.base) {
            return size + 4; // no base → always disp32
        }
        const auto b = parse_reg_id(upper_copy(*mem.base));
        if (!b) { errors.push_back("bad base register"); return 0; }
        if (mem.displacement == 0 && *b != 5) return size;
        return size + (fits_i8(mem.displacement) ? 1 : 4);
    }
    if (!mem.base) {
        errors.push_back("x86 backend requires a base register for memory operand");
        return 0;
    }
    const auto base = parse_reg_id(upper_copy(*mem.base));
    if (!base) {
        errors.push_back("x86 backend does not support that base register in memory operand");
        return 0;
    }
    size_t size = 2; // opcode + modrm
    if (*base == 4) size += 1; // SIB
    if (mem.displacement == 0 && *base != 5) return size;
    return size + (fits_i8(mem.displacement) ? 1 : 4);
}

size_t X86Backend::estimate_instruction_size(const IRInstruction& instruction, std::vector<std::string>& errors) const {
    const std::string mnemonic = upper_copy(instruction.mnemonic);

    if (mnemonic == "NOP" || mnemonic == "RET") {
        return 1;
    }

    if (mnemonic == "INT") {
        return 2;
    }

    if (mnemonic == "CALL" || mnemonic == "JMP") {
        // Register-indirect: CALL reg / JMP reg (2 bytes)
        if (instruction.operands.size() == 1 &&
            instruction.operands[0].kind == IROperandKind::Register) {
            return 2;
        }
        return 5;
    }

    if (jcc_opcode(mnemonic)) {
        return 6;
    }

    if (mnemonic == "MOV") {
        if (instruction.operands.size() != 2) {
            errors.push_back("x86 backend expects MOV with two operands");
            return 0;
        }

        const auto& dst = instruction.operands[0];
        const auto& src = instruction.operands[1];
        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Immediate) {
            return is_64bit_mode() ? 7 : 5;
        }

        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Symbol) {
            return is_64bit_mode() ? 7 : 5;
        }

        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Register) {
            return is_64bit_mode() ? 3 : 2;
        }

        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Memory) {
            const auto& mem = std::get<IRMemoryOperand>(src.value);
            if (mem.symbol && !mem.base && !mem.index) {
                return 6;
            }
            return compute_memory_operand_size(mem, errors);
        }

        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Register) {
            const auto& mem = std::get<IRMemoryOperand>(dst.value);
            if (mem.symbol && !mem.base && !mem.index) {
                return 6;
            }
            return compute_memory_operand_size(mem, errors);
        }

        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Immediate) {
            const auto& mem = std::get<IRMemoryOperand>(dst.value);
            if (mem.index || mem.symbol) {
                errors.push_back("x86 backend does not yet support MOV [mem], imm with index or symbol");
                return 0;
            }
            if (!mem.base) {
                errors.push_back("x86 backend requires a base register for MOV [mem], imm");
                return 0;
            }
            const auto base = encode_register_id(*mem.base);
            if (!base) { errors.push_back("bad base register"); return 0; }
            size_t size = 2;
            if (*base == 4) size += 1;
            if (mem.displacement == 0 && *base != 5) { /* no extra */ }
            else size += fits_i8(mem.displacement) ? 1 : 4;
            return size + 1; // +1 for imm8 (always use imm8 for simplicity)
        }
    }

    if (mnemonic == "PUSH" || mnemonic == "POP") {
        if (instruction.operands.size() != 1 || instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " reg");
            return 0;
        }
        return 1;
    }

    if (mnemonic == "INC" || mnemonic == "DEC") {
        if (instruction.operands.size() != 1) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " with one operand");
            return 0;
        }
        if (instruction.operands[0].kind == IROperandKind::Register) {
            return is_64bit_mode() ? 3 : 1;
        }
        if (instruction.operands[0].kind == IROperandKind::Memory) {
            return compute_memory_operand_size(std::get<IRMemoryOperand>(instruction.operands[0].value), errors);
        }
    }

    if (mnemonic == "ADD") {
        if (instruction.operands.size() != 2) {
            errors.push_back("x86 backend expects ADD with two operands");
            return 0;
        }
        const auto& dst = instruction.operands[0];
        const auto& src = instruction.operands[1];
        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Register) {
            return is_64bit_mode() ? 3 : 2;
        }
        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Immediate) {
            const int64_t imm = std::get<IRImmediateOperand>(src.value).value;
            return (fits_i8(imm) ? 3 : 6) + (is_64bit_mode() ? 1 : 0);
        }
        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Register) {
            return compute_memory_operand_size(std::get<IRMemoryOperand>(dst.value), errors);
        }
        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Immediate) {
            const int64_t imm = std::get<IRImmediateOperand>(src.value).value;
            const size_t mem_size = compute_memory_operand_size(std::get<IRMemoryOperand>(dst.value), errors);
            if (mem_size == 0) return 0;
            return mem_size + (fits_i8(imm) ? 1 : 4);
        }
    }

    if (mnemonic == "SUB" || mnemonic == "CMP") {
        if (instruction.operands.size() != 2) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " with two operands");
            return 0;
        }
        const auto& dst = instruction.operands[0];
        const auto& src = instruction.operands[1];
        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Register) {
            return is_64bit_mode() ? 3 : 2;
        }
        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Immediate) {
            const int64_t imm = std::get<IRImmediateOperand>(src.value).value;
            return (fits_i8(imm) ? 3 : 6) + (is_64bit_mode() ? 1 : 0);
        }
        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Register) {
            return compute_memory_operand_size(std::get<IRMemoryOperand>(dst.value), errors);
        }
        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Immediate) {
            const int64_t imm = std::get<IRImmediateOperand>(src.value).value;
            const size_t mem_size = compute_memory_operand_size(std::get<IRMemoryOperand>(dst.value), errors);
            if (mem_size == 0) return 0;
            return mem_size + (fits_i8(imm) ? 1 : 4);
        }
    }

    if (mnemonic == "XOR" || mnemonic == "AND" || mnemonic == "OR") {
        if (instruction.operands.size() != 2) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " with two operands");
            return 0;
        }
        const auto& dst = instruction.operands[0];
        const auto& src = instruction.operands[1];
        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Register) {
            return is_64bit_mode() ? 3 : 2;
        }
        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Immediate) {
            const int64_t imm = std::get<IRImmediateOperand>(src.value).value;
            return (fits_i8(imm) ? 3 : 6) + (is_64bit_mode() ? 1 : 0);
        }
        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Register) {
            return compute_memory_operand_size(std::get<IRMemoryOperand>(dst.value), errors);
        }
        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Immediate) {
            const int64_t imm = std::get<IRImmediateOperand>(src.value).value;
            const size_t mem_size = compute_memory_operand_size(std::get<IRMemoryOperand>(dst.value), errors);
            if (mem_size == 0) return 0;
            return mem_size + (fits_i8(imm) ? 1 : 4);
        }
    }

    if (mnemonic == "NOT") {
        if (instruction.operands.size() != 1 ||
            instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects NOT reg");
            return 0;
        }
        return 2;
    }

    if (mnemonic == "NEG") {
        if (instruction.operands.size() != 1 ||
            instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects NEG reg");
            return 0;
        }
        return 2;
    }

    if (mnemonic == "TEST") {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Register ||
            instruction.operands[1].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects TEST reg, reg");
            return 0;
        }
        return 2;
    }

    if (mnemonic == "SHL" || mnemonic == "SHR") {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " reg, imm8 or reg, CL");
            return 0;
        }
        if (instruction.operands[1].kind == IROperandKind::Immediate) {
            return 3; // C1 /r ib
        }
        if (instruction.operands[1].kind == IROperandKind::Register) {
            return 2; // D3 /r
        }
        if (instruction.operands[1].kind == IROperandKind::Symbol) {
            return 2; // D3 /r (CL as symbol)
        }
    }

    if (mnemonic == "LEA") {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Register ||
            instruction.operands[1].kind != IROperandKind::Memory) {
            errors.push_back("x86 backend expects LEA reg, [mem]");
            return 0;
        }
        const auto& mem = std::get<IRMemoryOperand>(instruction.operands[1].value);
        if (mem.symbol && !mem.base && !mem.index) {
            return 6;
        }
        return compute_memory_operand_size(mem, errors);
    }

    errors.push_back("x86 backend does not yet support instruction: " + instruction.mnemonic);
    return 0;
}

EncodedInstructionResult X86Backend::encode_instruction(
    const IRInstruction& instruction,
    uint64_t instruction_offset,
    const std::unordered_map<std::string, uint64_t>& text_symbol_offsets,
    std::vector<std::string>& errors) const {
    EncodedInstructionResult result;
    const std::string mnemonic = upper_copy(instruction.mnemonic);

    if (mnemonic == "NOP") {
        result.bytes = {0x90};
        return result;
    }

    if (mnemonic == "RET") {
        result.bytes = {0xC3};
        return result;
    }

    if (mnemonic == "INT") {
        if (instruction.operands.size() != 1 || instruction.operands[0].kind != IROperandKind::Immediate) {
            errors.push_back("x86 backend expects INT imm8");
            return result;
        }
        const auto value = std::get<IRImmediateOperand>(instruction.operands[0].value).value;
        if (value < 0 || value > 255) {
            errors.push_back("x86 backend only supports INT imm8 values");
            return result;
        }
        result.bytes = {0xCD, static_cast<uint8_t>(value)};
        return result;
    }

    if (mnemonic == "CALL" || mnemonic == "JMP" || jcc_opcode(mnemonic)) {
        // Register-indirect: CALL reg / JMP reg
        if ((mnemonic == "CALL" || mnemonic == "JMP") &&
            instruction.operands.size() == 1 &&
            instruction.operands[0].kind == IROperandKind::Register) {
            const auto reg = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[0].value).name);
            if (!reg) {
                errors.push_back("unsupported register in " + instruction.mnemonic);
                return result;
            }
            result.bytes = {0xFF, static_cast<uint8_t>((mnemonic == "CALL" ? 0xD0 : 0xE0) | (*reg & 0x7))};
            return result;
        }

        if (instruction.operands.size() != 1 || instruction.operands[0].kind != IROperandKind::Symbol) {
            errors.push_back("x86 backend currently supports symbolic control-flow operands only");
            return result;
        }

        const auto& symbol = std::get<IRSymbolOperand>(instruction.operands[0].value).name;
        const size_t size = estimate_instruction_size(instruction, errors);
        if (!errors.empty()) {
            return result;
        }

        size_t displacement_offset = 0;
        if (mnemonic == "CALL") {
            result.bytes.push_back(0xE8);
            displacement_offset = 1;
        } else if (mnemonic == "JMP") {
            result.bytes.push_back(0xE9);
            displacement_offset = 1;
        } else {
            result.bytes.push_back(0x0F);
            result.bytes.push_back(*jcc_opcode(mnemonic));
            displacement_offset = 2;
        }

        auto it = text_symbol_offsets.find(symbol);
        if (it != text_symbol_offsets.end()) {
            const int64_t rel = static_cast<int64_t>(it->second) - static_cast<int64_t>(instruction_offset + size);
            append_i32(result.bytes, static_cast<int32_t>(rel));
            return result;
        }

        append_i32(result.bytes, -4);
        IRRelocation relocation;
        relocation.section = IRSectionKind::Text;
        relocation.offset = instruction_offset + displacement_offset;
        relocation.symbol = symbol;
        relocation.kind = IRRelocationKind::PcRelative32;
        relocation.addend = -4;
        result.relocations.push_back(std::move(relocation));
        return result;
    }

    if (mnemonic == "MOV") {
        if (instruction.operands.size() != 2) {
            errors.push_back("x86 backend expects MOV with two operands");
            return result;
        }

        const auto& dst = instruction.operands[0];
        const auto& src = instruction.operands[1];

        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Immediate) {
            const auto reg = encode_register_id(std::get<IRRegisterOperand>(dst.value).name);
            if (!reg) {
                errors.push_back("x86 backend does not support that register in MOV reg, imm");
                return result;
            }
            if (is_64bit_mode()) {
                result.bytes = {0x48, 0xC7, static_cast<uint8_t>(0xC0 | (*reg & 0x7))};
                append_u32(result.bytes, static_cast<uint32_t>(std::get<IRImmediateOperand>(src.value).value));
            } else {
                result.bytes = {static_cast<uint8_t>(0xB8 + *reg)};
                append_u32(result.bytes, static_cast<uint32_t>(std::get<IRImmediateOperand>(src.value).value));
            }
            return result;
        }

        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Symbol) {
            const auto reg = encode_register_id(std::get<IRRegisterOperand>(dst.value).name);
            if (!reg) {
                errors.push_back("x86 backend does not support that register in MOV reg, symbol");
                return result;
            }
            const auto& symbol_name = std::get<IRSymbolOperand>(src.value).name;
            if (is_64bit_mode()) {
                result.bytes = {0x48, 0xC7, static_cast<uint8_t>(0xC0 | (*reg & 0x7))};
                append_u32(result.bytes, 0);
            } else {
                result.bytes = {static_cast<uint8_t>(0xB8 + *reg)};
                append_u32(result.bytes, 0);
            }
            IRRelocation relocation;
            relocation.section = IRSectionKind::Text;
            relocation.offset = instruction_offset + 1;
            relocation.symbol = symbol_name;
            relocation.kind = IRRelocationKind::Absolute32;
            relocation.addend = 0;
            result.relocations.push_back(std::move(relocation));
            return result;
        }

        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Register) {
            const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(dst.value).name);
            const auto src_reg = encode_register_id(std::get<IRRegisterOperand>(src.value).name);
            if (!dst_reg || !src_reg) {
                errors.push_back("x86 backend does not support that register in MOV reg, reg");
                return result;
            }
            if (is_64bit_mode()) {
                result.bytes = {0x48, 0x89, static_cast<uint8_t>(0xC0 | ((*src_reg & 0x7) << 3) | (*dst_reg & 0x7))};
            } else {
                result.bytes = {0x89, static_cast<uint8_t>(0xC0 | ((*src_reg & 0x7) << 3) | (*dst_reg & 0x7))};
            }
            return result;
        }

        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Memory) {
            const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(dst.value).name);
            if (!dst_reg) {
                errors.push_back("x86 backend does not support that register in MOV reg, [mem]");
                return result;
            }

            const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(src.value), *dst_reg, 0x8B, is_64bit_mode(), errors);
            if (!errors.empty()) {
                return result;
            }
            result.bytes = encoded_mem.bytes;
            if (encoded_mem.has_symbol_relocation) {
                IRRelocation relocation;
                relocation.section = IRSectionKind::Text;
                relocation.offset = instruction_offset + encoded_mem.displacement_offset;
                relocation.symbol = encoded_mem.relocation_symbol;
                relocation.kind = IRRelocationKind::Absolute32;
                relocation.addend = encoded_mem.relocation_addend;
                result.relocations.push_back(std::move(relocation));
            }
            return result;
        }

        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Register) {
            const auto src_reg = encode_register_id(std::get<IRRegisterOperand>(src.value).name);
            if (!src_reg) {
                errors.push_back("x86 backend does not support that register in MOV [mem], reg");
                return result;
            }

            const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(dst.value), *src_reg, 0x89, is_64bit_mode(), errors);
            if (!errors.empty()) {
                return result;
            }
            result.bytes = encoded_mem.bytes;
            if (encoded_mem.has_symbol_relocation) {
                IRRelocation relocation;
                relocation.section = IRSectionKind::Text;
                relocation.offset = instruction_offset + encoded_mem.displacement_offset;
                relocation.symbol = encoded_mem.relocation_symbol;
                relocation.kind = IRRelocationKind::Absolute32;
                relocation.addend = encoded_mem.relocation_addend;
                result.relocations.push_back(std::move(relocation));
            }
            return result;
        }

        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Immediate) {
            const int64_t imm = std::get<IRImmediateOperand>(src.value).value;
            const auto& mem = std::get<IRMemoryOperand>(dst.value);
            const auto encoded_mem = encode_memory_operand32(mem, 0, static_cast<uint8_t>(fits_i8(imm) ? 0xC6 : 0xC7), is_64bit_mode(), errors);
            if (!errors.empty()) return result;
            result.bytes = encoded_mem.bytes;
            if (fits_i8(imm)) {
                result.bytes.push_back(static_cast<uint8_t>(static_cast<int8_t>(imm)));
            } else {
                append_u32(result.bytes, static_cast<uint32_t>(imm));
            }
            return result;
        }
    }

    if (mnemonic == "PUSH") {
        if (instruction.operands.size() != 1 || instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects PUSH reg");
            return result;
        }
        const auto reg = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[0].value).name);
        if (!reg) {
            errors.push_back("x86 backend does not support that register in PUSH");
            return result;
        }
        result.bytes = {static_cast<uint8_t>(0x50 + *reg)};
        return result;
    }

    if (mnemonic == "POP") {
        if (instruction.operands.size() != 1 || instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects POP reg");
            return result;
        }
        const auto reg = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[0].value).name);
        if (!reg) {
            errors.push_back("x86 backend does not support that register in POP");
            return result;
        }
        result.bytes = {static_cast<uint8_t>(0x58 + *reg)};
        return result;
    }

    if (mnemonic == "INC") {
        if (instruction.operands.size() != 1) {
            errors.push_back("x86 backend expects INC with one operand");
            return result;
        }
        if (instruction.operands[0].kind == IROperandKind::Register) {
            const auto reg = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[0].value).name);
            if (!reg) {
                errors.push_back("x86 backend does not support that register in INC");
                return result;
            }
            if (is_64bit_mode()) {
                result.bytes = {0x48, 0xFF, static_cast<uint8_t>(0xC0 | (*reg & 0x7))};
            } else {
                result.bytes = {static_cast<uint8_t>(0x40 + *reg)};
            }
            return result;
        }
        if (instruction.operands[0].kind == IROperandKind::Memory) {
            const auto& mem = std::get<IRMemoryOperand>(instruction.operands[0].value);
            const auto encoded_mem = encode_memory_operand32(mem, 0, 0xFF, is_64bit_mode(), errors);
            if (!errors.empty()) return result;
            result.bytes = encoded_mem.bytes;
            return result;
        }
    }

    if (mnemonic == "DEC") {
        if (instruction.operands.size() != 1) {
            errors.push_back("x86 backend expects DEC with one operand");
            return result;
        }
        if (instruction.operands[0].kind == IROperandKind::Register) {
            const auto reg = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[0].value).name);
            if (!reg) {
                errors.push_back("x86 backend does not support that register in DEC");
                return result;
            }
            if (is_64bit_mode()) {
                result.bytes = {0x48, 0xFF, static_cast<uint8_t>(0xC8 | (*reg & 0x7))};
            } else {
                result.bytes = {static_cast<uint8_t>(0x48 + *reg)};
            }
            return result;
        }
        if (instruction.operands[0].kind == IROperandKind::Memory) {
            const auto& mem = std::get<IRMemoryOperand>(instruction.operands[0].value);
            const auto encoded_mem = encode_memory_operand32(mem, 1, 0xFF, is_64bit_mode(), errors);
            if (!errors.empty()) return result;
            result.bytes = encoded_mem.bytes;
            return result;
        }
    }

    if (mnemonic == "ADD") {
        if (instruction.operands.size() != 2) {
            errors.push_back("x86 backend expects ADD with two operands");
            return result;
        }
        const auto& dst = instruction.operands[0];
        const auto& src = instruction.operands[1];

        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Register) {
            const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(dst.value).name);
            const auto src_reg = encode_register_id(std::get<IRRegisterOperand>(src.value).name);
            if (!dst_reg || !src_reg) {
                errors.push_back("x86 backend does not support that register in ADD reg, reg");
                return result;
            }
            if (is_64bit_mode()) result.bytes.push_back(0x48);
            result.bytes.push_back(0x01);
            result.bytes.push_back(static_cast<uint8_t>(0xC0 | ((*src_reg & 0x7) << 3) | (*dst_reg & 0x7)));
            return result;
        }

        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Immediate) {
            const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(dst.value).name);
            if (!dst_reg) {
                errors.push_back("x86 backend does not support that register in ADD reg, imm");
                return result;
            }
            const int64_t imm = std::get<IRImmediateOperand>(src.value).value;
            if (is_64bit_mode()) result.bytes.push_back(0x48);
            if (fits_i8(imm)) {
                result.bytes.push_back(0x83);
                result.bytes.push_back(static_cast<uint8_t>(0xC0 | *dst_reg));
                result.bytes.push_back(static_cast<uint8_t>(static_cast<int8_t>(imm)));
            } else {
                result.bytes.push_back(0x81);
                result.bytes.push_back(static_cast<uint8_t>(0xC0 | *dst_reg));
                append_u32(result.bytes, static_cast<uint32_t>(imm));
            }
            return result;
        }

        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Register) {
            const auto src_reg = encode_register_id(std::get<IRRegisterOperand>(src.value).name);
            if (!src_reg) {
                errors.push_back("x86 backend does not support that register in ADD [mem], reg");
                return result;
            }
            const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(dst.value), *src_reg, 0x01, is_64bit_mode(), errors);
            if (!errors.empty()) return result;
            result.bytes = encoded_mem.bytes;
            return result;
        }

        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Immediate) {
            const int64_t imm = std::get<IRImmediateOperand>(src.value).value;
            if (fits_i8(imm)) {
                const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(dst.value), 0, 0x83, is_64bit_mode(), errors);
                if (!errors.empty()) return result;
                result.bytes = encoded_mem.bytes;
                result.bytes.push_back(static_cast<uint8_t>(static_cast<int8_t>(imm)));
            } else {
                const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(dst.value), 0, 0x81, is_64bit_mode(), errors);
                if (!errors.empty()) return result;
                result.bytes = encoded_mem.bytes;
                append_u32(result.bytes, static_cast<uint32_t>(imm));
            }
            return result;
        }
    }

    if (mnemonic == "SUB") {
        if (instruction.operands.size() != 2) {
            errors.push_back("x86 backend expects SUB with two operands");
            return result;
        }
        const auto& dst = instruction.operands[0];
        const auto& src = instruction.operands[1];

        auto encode_sub_reg = [&](uint8_t dst_reg) -> bool {
            if (src.kind == IROperandKind::Register) {
                const auto src_reg = encode_register_id(std::get<IRRegisterOperand>(src.value).name);
                if (!src_reg) { errors.push_back("bad src register in SUB"); return false; }
                if (is_64bit_mode()) result.bytes.push_back(0x48);
                result.bytes.push_back(0x29);
                result.bytes.push_back(static_cast<uint8_t>(0xC0 | ((*src_reg & 0x7) << 3) | (dst_reg & 0x7)));
                return true;
            }
            if (src.kind == IROperandKind::Immediate) {
                const int64_t imm = std::get<IRImmediateOperand>(src.value).value;
                if (is_64bit_mode()) result.bytes.push_back(0x48);
                if (fits_i8(imm)) {
                    result.bytes.push_back(0x83);
                    result.bytes.push_back(static_cast<uint8_t>(0xE8 | dst_reg));
                    result.bytes.push_back(static_cast<uint8_t>(static_cast<int8_t>(imm)));
                } else {
                    result.bytes.push_back(0x81);
                    result.bytes.push_back(static_cast<uint8_t>(0xE8 | dst_reg));
                    append_u32(result.bytes, static_cast<uint32_t>(imm));
                }
                return true;
            }
            return false;
        };

        if (dst.kind == IROperandKind::Register) {
            const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(dst.value).name);
            if (!dst_reg) { errors.push_back("bad dst register in SUB"); return result; }
            if (!encode_sub_reg(*dst_reg)) return result;
            return result;
        }

        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Register) {
            const auto src_reg = encode_register_id(std::get<IRRegisterOperand>(src.value).name);
            if (!src_reg) { errors.push_back("bad src register in SUB [mem], reg"); return result; }
            const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(dst.value), *src_reg, 0x29, is_64bit_mode(), errors);
            if (!errors.empty()) return result;
            result.bytes = encoded_mem.bytes;
            return result;
        }

        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Immediate) {
            const int64_t imm = std::get<IRImmediateOperand>(src.value).value;
            if (fits_i8(imm)) {
                const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(dst.value), 5, 0x83, is_64bit_mode(), errors);
                if (!errors.empty()) return result;
                result.bytes = encoded_mem.bytes;
                result.bytes.push_back(static_cast<uint8_t>(static_cast<int8_t>(imm)));
            } else {
                const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(dst.value), 5, 0x81, is_64bit_mode(), errors);
                if (!errors.empty()) return result;
                result.bytes = encoded_mem.bytes;
                append_u32(result.bytes, static_cast<uint32_t>(imm));
            }
            return result;
        }
    }

    if (mnemonic == "CMP") {
        if (instruction.operands.size() != 2) {
            errors.push_back("x86 backend expects CMP with two operands");
            return result;
        }
        const auto& dst = instruction.operands[0];
        const auto& src = instruction.operands[1];

        auto encode_cmp_reg = [&](uint8_t dst_reg) -> bool {
            if (src.kind == IROperandKind::Register) {
                const auto src_reg = encode_register_id(std::get<IRRegisterOperand>(src.value).name);
                if (!src_reg) { errors.push_back("bad src register in CMP"); return false; }
                if (is_64bit_mode()) result.bytes.push_back(0x48);
                result.bytes.push_back(0x39);
                result.bytes.push_back(static_cast<uint8_t>(0xC0 | ((*src_reg & 0x7) << 3) | (dst_reg & 0x7)));
                return true;
            }
            if (src.kind == IROperandKind::Immediate) {
                const int64_t imm = std::get<IRImmediateOperand>(src.value).value;
                if (is_64bit_mode()) result.bytes.push_back(0x48);
                if (fits_i8(imm)) {
                    result.bytes.push_back(0x83);
                    result.bytes.push_back(static_cast<uint8_t>(0xF8 | dst_reg));
                    result.bytes.push_back(static_cast<uint8_t>(static_cast<int8_t>(imm)));
                } else {
                    result.bytes.push_back(0x81);
                    result.bytes.push_back(static_cast<uint8_t>(0xF8 | dst_reg));
                    append_u32(result.bytes, static_cast<uint32_t>(imm));
                }
                return true;
            }
            return false;
        };

        if (dst.kind == IROperandKind::Register) {
            const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(dst.value).name);
            if (!dst_reg) { errors.push_back("bad dst register in CMP"); return result; }
            if (!encode_cmp_reg(*dst_reg)) return result;
            return result;
        }

        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Register) {
            const auto src_reg = encode_register_id(std::get<IRRegisterOperand>(src.value).name);
            if (!src_reg) { errors.push_back("bad src register in CMP [mem], reg"); return result; }
            const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(dst.value), *src_reg, 0x39, is_64bit_mode(), errors);
            if (!errors.empty()) return result;
            result.bytes = encoded_mem.bytes;
            return result;
        }

        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Immediate) {
            const int64_t imm = std::get<IRImmediateOperand>(src.value).value;
            if (fits_i8(imm)) {
                const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(dst.value), 7, 0x83, is_64bit_mode(), errors);
                if (!errors.empty()) return result;
                result.bytes = encoded_mem.bytes;
                result.bytes.push_back(static_cast<uint8_t>(static_cast<int8_t>(imm)));
            } else {
                const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(dst.value), 7, 0x81, is_64bit_mode(), errors);
                if (!errors.empty()) return result;
                result.bytes = encoded_mem.bytes;
                append_u32(result.bytes, static_cast<uint32_t>(imm));
            }
            return result;
        }
    }

    if (mnemonic == "XOR" || mnemonic == "AND" || mnemonic == "OR") {
        if (instruction.operands.size() != 2) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " with two operands");
            return result;
        }
        const auto& dst = instruction.operands[0];
        const auto& src = instruction.operands[1];

        uint8_t reg_opcode = 0x31;
        uint8_t imm_opcode = 0xF0;
        if (mnemonic == "AND") { reg_opcode = 0x21; imm_opcode = 0xE0; }
        if (mnemonic == "OR")  { reg_opcode = 0x09; imm_opcode = 0xC8; }

        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Register) {
            const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(dst.value).name);
            const auto src_reg = encode_register_id(std::get<IRRegisterOperand>(src.value).name);
            if (!dst_reg || !src_reg) {
                errors.push_back("bad register in " + instruction.mnemonic + " reg, reg");
                return result;
            }
            if (is_64bit_mode()) result.bytes.push_back(0x48);
            result.bytes.push_back(reg_opcode);
            result.bytes.push_back(static_cast<uint8_t>(0xC0 | ((*src_reg & 0x7) << 3) | (*dst_reg & 0x7)));
            return result;
        }

        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Immediate) {
            const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(dst.value).name);
            if (!dst_reg) { errors.push_back("bad register in " + instruction.mnemonic + " reg, imm"); return result; }
            const int64_t imm = std::get<IRImmediateOperand>(src.value).value;
            if (is_64bit_mode()) result.bytes.push_back(0x48);
            if (fits_i8(imm)) {
                result.bytes.push_back(0x83);
                result.bytes.push_back(static_cast<uint8_t>(imm_opcode | *dst_reg));
                result.bytes.push_back(static_cast<uint8_t>(static_cast<int8_t>(imm)));
            } else {
                result.bytes.push_back(0x81);
                result.bytes.push_back(static_cast<uint8_t>(imm_opcode | *dst_reg));
                append_u32(result.bytes, static_cast<uint32_t>(imm));
            }
            return result;
        }

        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Register) {
            const auto src_reg = encode_register_id(std::get<IRRegisterOperand>(src.value).name);
            if (!src_reg) { errors.push_back("bad src register in " + instruction.mnemonic + " [mem], reg"); return result; }
            const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(dst.value), *src_reg, reg_opcode, is_64bit_mode(), errors);
            if (!errors.empty()) return result;
            result.bytes = encoded_mem.bytes;
            return result;
        }
    }

    if (mnemonic == "NOT") {
        if (instruction.operands.size() != 1 ||
            instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects NOT reg");
            return result;
        }
        const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[0].value).name);
        if (!dst_reg) {
            errors.push_back("x86 backend does not support that register in NOT");
            return result;
        }
        if (is_64bit_mode()) result.bytes.push_back(0x48);
        result.bytes.push_back(0xF7);
        result.bytes.push_back(static_cast<uint8_t>(0xD0 | (*dst_reg & 0x7)));
        return result;
    }

    if (mnemonic == "LEA") {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Register ||
            instruction.operands[1].kind != IROperandKind::Memory) {
            errors.push_back("x86 backend expects LEA reg, [mem]");
            return result;
        }
        const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[0].value).name);
        if (!dst_reg) {
            errors.push_back("x86 backend does not support that register in LEA");
            return result;
        }

        const auto& mem = std::get<IRMemoryOperand>(instruction.operands[1].value);

        // Symbol-only memory (e.g., LEA reg, [msg])
        if (!mem.base && mem.symbol) {
            const auto encoded_mem = encode_memory_operand32(mem, *dst_reg, 0x8D, is_64bit_mode(), errors);
            if (!errors.empty()) return result;
            result.bytes = encoded_mem.bytes;
            if (encoded_mem.has_symbol_relocation) {
                IRRelocation relocation;
                relocation.section = IRSectionKind::Text;
                relocation.offset = instruction_offset + encoded_mem.displacement_offset;
                relocation.symbol = encoded_mem.relocation_symbol;
                relocation.kind = IRRelocationKind::Absolute32;
                relocation.addend = encoded_mem.relocation_addend;
                result.relocations.push_back(std::move(relocation));
            }
            return result;
        }

        // Base-only, indexed, or base+index: use shared memory encoder
        const auto encoded_mem = encode_memory_operand32(mem, *dst_reg, 0x8D, is_64bit_mode(), errors);
        if (!errors.empty()) return result;
        result.bytes = encoded_mem.bytes;
        return result;
    }

    if (mnemonic == "NEG") {
        if (instruction.operands.size() != 1 ||
            instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects NEG reg");
            return result;
        }
        const auto reg = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[0].value).name);
        if (!reg) { errors.push_back("bad register in NEG"); return result; }
        if (is_64bit_mode()) result.bytes.push_back(0x48);
        result.bytes.push_back(0xF7);
        result.bytes.push_back(static_cast<uint8_t>(0xD8 | (*reg & 0x7)));
        return result;
    }

    if (mnemonic == "TEST") {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Register ||
            instruction.operands[1].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects TEST reg, reg");
            return result;
        }
        const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[0].value).name);
        const auto src_reg = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[1].value).name);
        if (!dst_reg || !src_reg) { errors.push_back("bad register in TEST"); return result; }
        if (is_64bit_mode()) result.bytes.push_back(0x48);
        result.bytes.push_back(0x85);
        result.bytes.push_back(static_cast<uint8_t>(0xC0 | ((*src_reg & 0x7) << 3) | (*dst_reg & 0x7)));
        return result;
    }

    if (mnemonic == "SHL" || mnemonic == "SHR") {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " reg, imm8 or reg, CL");
            return result;
        }
        const auto reg = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[0].value).name);
        if (!reg) { errors.push_back("bad register in " + instruction.mnemonic); return result; }
        const uint8_t subcode = mnemonic == "SHL" ? 0xE0 : 0xE8;

        if (instruction.operands[1].kind == IROperandKind::Immediate) {
            if (is_64bit_mode()) result.bytes.push_back(0x48);
            result.bytes.push_back(0xC1);
            result.bytes.push_back(static_cast<uint8_t>(subcode | (*reg & 0x7)));
            result.bytes.push_back(static_cast<uint8_t>(std::get<IRImmediateOperand>(instruction.operands[1].value).value & 0xFF));
            return result;
        }

        if (instruction.operands[1].kind == IROperandKind::Register) {
            const auto cl = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[1].value).name);
            if (!cl || *cl != 1) { errors.push_back("x86 backend only supports ECX register for variable shift"); return result; }
            if (is_64bit_mode()) result.bytes.push_back(0x48);
            result.bytes.push_back(0xD3);
            result.bytes.push_back(static_cast<uint8_t>(subcode | (*reg & 0x7)));
            return result;
        }

        if (instruction.operands[1].kind == IROperandKind::Symbol) {
            const auto& name = std::get<IRSymbolOperand>(instruction.operands[1].value).name;
            if (upper_copy(name) != "CL") { errors.push_back("x86 backend only supports CL for variable shift"); return result; }
            if (is_64bit_mode()) result.bytes.push_back(0x48);
            result.bytes.push_back(0xD3);
            result.bytes.push_back(static_cast<uint8_t>(subcode | (*reg & 0x7)));
            return result;
        }
    }

    errors.push_back("x86 backend does not yet support instruction: " + instruction.mnemonic);
    return result;
}

IRProgram X86Backend::adjust_program_for_encoded_text(
    const IRProgram& program,
    const std::unordered_map<uint64_t, uint64_t>& text_offset_map) const {
    IRProgram adjusted = program;

    for (auto& symbol : adjusted.symbols) {
        if (symbol.section != IRSectionKind::Text) {
            continue;
        }
        auto it = text_offset_map.find(symbol.offset);
        if (it != text_offset_map.end()) {
            symbol.offset = it->second;
        }
    }

    for (auto& relocation : adjusted.relocations) {
        if (relocation.section != IRSectionKind::Text) {
            continue;
        }
        auto it = text_offset_map.find(relocation.offset);
        if (it != text_offset_map.end()) {
            relocation.offset = it->second;
        }
    }

    return adjusted;
}

bool X86Backend::is_64bit_mode() const {
    return mode_ == X86BackendMode::X86_64;
}

} // namespace Assembler