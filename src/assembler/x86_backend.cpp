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
    bool is_load,
    std::vector<std::string>& errors) {
    EncodedMemoryOperand encoded;

    if (memory.index) {
        errors.push_back("x86 backend does not yet support indexed memory operands in MOV");
        return encoded;
    }

    if (memory.base && memory.symbol) {
        errors.push_back("x86 backend does not yet support base register plus symbolic displacement in MOV");
        return encoded;
    }

    const uint8_t opcode = is_load ? 0x8B : 0x89;
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

    if (memory.symbol) {
        encoded.bytes.push_back(static_cast<uint8_t>((0b00 << 6) | ((reg_field & 0x7) << 3) | 0b101));
        encoded.displacement_offset = encoded.bytes.size();
        encoded.displacement_size = 4;
        append_i32(encoded.bytes, static_cast<int32_t>(memory.displacement));
        encoded.has_symbol_relocation = true;
        encoded.relocation_symbol = *memory.symbol;
        encoded.relocation_addend = memory.displacement;
        return encoded;
    }

    if (!memory.base) {
        errors.push_back("x86 backend requires a base register or symbol for MOV memory operands");
        return encoded;
    }

    const auto base = parse_reg_id(upper_copy(*memory.base));
    if (!base) {
        errors.push_back("x86 backend does not support that base register in MOV memory operand");
        return encoded;
    }

    const int64_t disp = memory.displacement;
    const bool needs_sib = *base == 4;
    const uint8_t rm_field = needs_sib ? 4 : *base;

    if (disp == 0 && *base != 5) {
        encoded.bytes.push_back(static_cast<uint8_t>((0b00 << 6) | ((reg_field & 0x7) << 3) | rm_field));
        if (needs_sib) {
            encoded.bytes.push_back(0x24);
        }
        return encoded;
    }

    if (fits_i8(disp)) {
        emit_disp8(static_cast<uint8_t>((0b01 << 6) | ((reg_field & 0x7) << 3) | rm_field), static_cast<int8_t>(disp));
        if (needs_sib) {
            // SIB must come before displacement; reorder
            encoded.bytes.insert(encoded.bytes.end() - 1, 0x24);
        }
        return encoded;
    }

    emit_disp32(static_cast<uint8_t>((0b10 << 6) | ((reg_field & 0x7) << 3) | rm_field), static_cast<int32_t>(disp));
    if (needs_sib) {
        encoded.bytes.insert(encoded.bytes.end() - 4, 0x24);
    }
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

    for (size_t instruction_index = 0; instruction_index < program.instructions.size(); ++instruction_index) {
        text_offset_map[static_cast<uint64_t>(instruction_index)] = text_bytes.size();
        const auto estimated = estimate_instruction_size(program.instructions[instruction_index], artifact.errors);
        if (!artifact.ok()) {
            return artifact;
        }
        text_bytes.resize(text_bytes.size() + estimated);
    }

    text_bytes.clear();
    auto adjusted_program = adjust_program_for_encoded_text(program, text_offset_map);
    std::unordered_map<std::string, uint64_t> text_symbol_offsets;
    for (const auto& symbol : adjusted_program.symbols) {
        if (symbol.defined && symbol.section == IRSectionKind::Text) {
            text_symbol_offsets[symbol.name] = symbol.offset;
        }
    }

    std::vector<IRRelocation> text_relocations;

    for (size_t instruction_index = 0; instruction_index < program.instructions.size(); ++instruction_index) {
        const auto& instruction = program.instructions[instruction_index];
        if (instruction.section != IRSectionKind::Text) {
            artifact.errors.push_back("x86 backend only supports .text instructions right now");
            return artifact;
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

size_t X86Backend::estimate_instruction_size(const IRInstruction& instruction, std::vector<std::string>& errors) const {
    const std::string mnemonic = upper_copy(instruction.mnemonic);

    if (mnemonic == "NOP" || mnemonic == "RET") {
        return 1;
    }

    if (mnemonic == "INT") {
        return 2;
    }

    if (mnemonic == "CALL" || mnemonic == "JMP") {
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
            if (is_64bit_mode()) {
                errors.push_back("x86-64 backend does not yet support MOV reg, [mem]");
                return 0;
            }
            const auto& mem = std::get<IRMemoryOperand>(src.value);
            if (mem.index) {
                errors.push_back("x86 backend does not yet support indexed memory operands in MOV");
                return 0;
            }
            if (mem.symbol) {
                return 6;
            }
            if (!mem.base) {
                errors.push_back("x86 backend requires a base register or symbol for MOV memory operands");
                return 0;
            }
            const auto base = encode_register_id(*mem.base);
            if (!base) {
                errors.push_back("x86 backend does not support that base register in MOV memory operand");
                return 0;
            }
            size_t size = 2;
            if (*base == 4) {
                size += 1;
            }
            if (mem.displacement == 0 && *base != 5) {
                return size;
            }
            return size + (fits_i8(mem.displacement) ? 1 : 4);
        }

        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Register) {
            if (is_64bit_mode()) {
                errors.push_back("x86-64 backend does not yet support MOV [mem], reg");
                return 0;
            }
            const auto& mem = std::get<IRMemoryOperand>(dst.value);
            if (mem.index) {
                errors.push_back("x86 backend does not yet support indexed memory operands in MOV");
                return 0;
            }
            if (mem.symbol) {
                return 6;
            }
            if (!mem.base) {
                errors.push_back("x86 backend requires a base register or symbol for MOV memory operands");
                return 0;
            }
            const auto base = encode_register_id(*mem.base);
            if (!base) {
                errors.push_back("x86 backend does not support that base register in MOV memory operand");
                return 0;
            }
            size_t size = 2;
            if (*base == 4) {
                size += 1;
            }
            if (mem.displacement == 0 && *base != 5) {
                return size;
            }
            return size + (fits_i8(mem.displacement) ? 1 : 4);
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
        if (instruction.operands.size() != 1 || instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " reg");
            return 0;
        }
        return 1;
    }

    if (mnemonic == "ADD") {
        if (instruction.operands.size() != 2) {
            errors.push_back("x86 backend expects ADD with two operands");
            return 0;
        }
        const auto& dst = instruction.operands[0];
        const auto& src = instruction.operands[1];
        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Register) {
            return 2;
        }
        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Immediate) {
            const int64_t imm = std::get<IRImmediateOperand>(src.value).value;
            return fits_i8(imm) ? 3 : 6;
        }
    }

    if (mnemonic == "SUB" || mnemonic == "CMP") {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " reg, reg");
            return 0;
        }
        if (instruction.operands[1].kind == IROperandKind::Register) {
            return 2;
        }
        if (instruction.operands[1].kind == IROperandKind::Immediate) {
            const int64_t imm = std::get<IRImmediateOperand>(instruction.operands[1].value).value;
            return fits_i8(imm) ? 3 : 6;
        }
    }

    if (mnemonic == "XOR" || mnemonic == "AND" || mnemonic == "OR") {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " with register destination");
            return 0;
        }
        if (instruction.operands[1].kind == IROperandKind::Register) {
            return 2;
        }
        if (instruction.operands[1].kind == IROperandKind::Immediate) {
            const int64_t imm = std::get<IRImmediateOperand>(instruction.operands[1].value).value;
            return fits_i8(imm) ? 3 : 6;
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

    if (mnemonic == "LEA") {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Register ||
            instruction.operands[1].kind != IROperandKind::Memory) {
            errors.push_back("x86 backend expects LEA reg, [mem]");
            return 0;
        }
        const auto& mem = std::get<IRMemoryOperand>(instruction.operands[1].value);
        if (!mem.base) {
            errors.push_back("x86 backend requires a base register for LEA memory operand");
            return 0;
        }
        const auto base = encode_register_id(*mem.base);
        if (!base) {
            errors.push_back("x86 backend does not support that base register in LEA");
            return 0;
        }
        size_t size = 2;
        if (*base == 4) size += 1;
        if (mem.displacement == 0 && *base != 5) return size;
        return size + (fits_i8(mem.displacement) ? 1 : 4);
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

            const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(src.value), *dst_reg, true, errors);
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

            const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(dst.value), *src_reg, false, errors);
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
        if (instruction.operands.size() != 1 || instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects INC reg");
            return result;
        }
        const auto reg = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[0].value).name);
        if (!reg) {
            errors.push_back("x86 backend does not support that register in INC");
            return result;
        }
        result.bytes = {static_cast<uint8_t>(0x40 + *reg)};
        return result;
    }

    if (mnemonic == "DEC") {
        if (instruction.operands.size() != 1 || instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects DEC reg");
            return result;
        }
        const auto reg = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[0].value).name);
        if (!reg) {
            errors.push_back("x86 backend does not support that register in DEC");
            return result;
        }
        result.bytes = {static_cast<uint8_t>(0x48 + *reg)};
        return result;
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
            result.bytes = {0x01, static_cast<uint8_t>(0xC0 | ((*src_reg & 0x7) << 3) | (*dst_reg & 0x7))};
            return result;
        }

        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Immediate) {
            const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(dst.value).name);
            if (!dst_reg) {
                errors.push_back("x86 backend does not support that register in ADD reg, imm");
                return result;
            }
            const int64_t imm = std::get<IRImmediateOperand>(src.value).value;
            if (fits_i8(imm)) {
                result.bytes = {0x83, static_cast<uint8_t>(0xC0 | *dst_reg), static_cast<uint8_t>(static_cast<int8_t>(imm))};
            } else {
                result.bytes = {0x81, static_cast<uint8_t>(0xC0 | *dst_reg)};
                append_u32(result.bytes, static_cast<uint32_t>(imm));
            }
            return result;
        }
    }

    if (mnemonic == "SUB") {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects SUB with register destination");
            return result;
        }
        const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[0].value).name);
        if (!dst_reg) {
            errors.push_back("x86 backend does not support that register in SUB");
            return result;
        }

        if (instruction.operands[1].kind == IROperandKind::Register) {
            const auto src_reg = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[1].value).name);
            if (!src_reg) {
                errors.push_back("x86 backend does not support that register in SUB reg, reg");
                return result;
            }
            result.bytes = {0x29, static_cast<uint8_t>(0xC0 | ((*src_reg & 0x7) << 3) | (*dst_reg & 0x7))};
            return result;
        }

        if (instruction.operands[1].kind == IROperandKind::Immediate) {
            const int64_t imm = std::get<IRImmediateOperand>(instruction.operands[1].value).value;
            if (fits_i8(imm)) {
                result.bytes = {0x83, static_cast<uint8_t>(0xE8 | *dst_reg), static_cast<uint8_t>(static_cast<int8_t>(imm))};
            } else {
                result.bytes = {0x81, static_cast<uint8_t>(0xE8 | *dst_reg)};
                append_u32(result.bytes, static_cast<uint32_t>(imm));
            }
            return result;
        }
    }

    if (mnemonic == "CMP") {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects CMP with register first operand");
            return result;
        }
        const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[0].value).name);
        if (!dst_reg) {
            errors.push_back("x86 backend does not support that register in CMP");
            return result;
        }

        if (instruction.operands[1].kind == IROperandKind::Register) {
            const auto src_reg = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[1].value).name);
            if (!src_reg) {
                errors.push_back("x86 backend does not support that register in CMP reg, reg");
                return result;
            }
            result.bytes = {0x39, static_cast<uint8_t>(0xC0 | ((*src_reg & 0x7) << 3) | (*dst_reg & 0x7))};
            return result;
        }

        if (instruction.operands[1].kind == IROperandKind::Immediate) {
            const int64_t imm = std::get<IRImmediateOperand>(instruction.operands[1].value).value;
            if (fits_i8(imm)) {
                result.bytes = {0x83, static_cast<uint8_t>(0xF8 | *dst_reg), static_cast<uint8_t>(static_cast<int8_t>(imm))};
            } else {
                result.bytes = {0x81, static_cast<uint8_t>(0xF8 | *dst_reg)};
                append_u32(result.bytes, static_cast<uint32_t>(imm));
            }
            return result;
        }
    }

    if (mnemonic == "XOR" || mnemonic == "AND" || mnemonic == "OR") {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " with register destination");
            return result;
        }
        const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[0].value).name);
        if (!dst_reg) {
            errors.push_back("x86 backend does not support that register in " + instruction.mnemonic);
            return result;
        }

        uint8_t reg_opcode = 0x31;
        uint8_t imm_opcode = 0xF0;
        if (mnemonic == "AND") { reg_opcode = 0x21; imm_opcode = 0xE0; }
        if (mnemonic == "OR")  { reg_opcode = 0x09; imm_opcode = 0xC8; }

        if (instruction.operands[1].kind == IROperandKind::Register) {
            const auto src_reg = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[1].value).name);
            if (!src_reg) {
                errors.push_back("x86 backend does not support that register in " + instruction.mnemonic + " reg, reg");
                return result;
            }
            result.bytes = {reg_opcode, static_cast<uint8_t>(0xC0 | ((*src_reg & 0x7) << 3) | (*dst_reg & 0x7))};
            return result;
        }

        if (instruction.operands[1].kind == IROperandKind::Immediate) {
            const int64_t imm = std::get<IRImmediateOperand>(instruction.operands[1].value).value;
            if (fits_i8(imm)) {
                result.bytes = {0x83, static_cast<uint8_t>(imm_opcode | *dst_reg), static_cast<uint8_t>(static_cast<int8_t>(imm))};
            } else {
                result.bytes = {0x81, static_cast<uint8_t>(imm_opcode | *dst_reg)};
                append_u32(result.bytes, static_cast<uint32_t>(imm));
            }
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
        result.bytes = {0xF7, static_cast<uint8_t>(0xD0 | (*dst_reg & 0x7))};
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
        if (mem.index) {
            errors.push_back("x86 backend does not yet support indexed memory operands in LEA");
            return result;
        }
        if (!mem.base) {
            errors.push_back("x86 backend requires a base register for LEA memory operand");
            return result;
        }
        const auto base = parse_reg_id(upper_copy(*mem.base));
        if (!base) {
            errors.push_back("x86 backend does not support that base register in LEA");
            return result;
        }

        result.bytes.push_back(0x8D);
        const int64_t disp = mem.displacement;
        const bool needs_sib = *base == 4;
        const uint8_t rm_field = needs_sib ? 4 : *base;

        if (disp == 0 && *base != 5) {
            result.bytes.push_back(static_cast<uint8_t>((0b00 << 6) | ((*dst_reg & 0x7) << 3) | rm_field));
            if (needs_sib) result.bytes.push_back(0x24);
        } else if (fits_i8(disp)) {
            result.bytes.push_back(static_cast<uint8_t>((0b01 << 6) | ((*dst_reg & 0x7) << 3) | rm_field));
            if (needs_sib) result.bytes.push_back(0x24);
            result.bytes.push_back(static_cast<uint8_t>(static_cast<int8_t>(disp)));
        } else {
            result.bytes.push_back(static_cast<uint8_t>((0b10 << 6) | ((*dst_reg & 0x7) << 3) | rm_field));
            if (needs_sib) result.bytes.push_back(0x24);
            append_i32(result.bytes, static_cast<int32_t>(disp));
        }
        return result;
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