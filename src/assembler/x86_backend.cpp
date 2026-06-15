#include "x86_backend.hpp"

#include "elf32_executable.hpp"
#include "elf64_executable.hpp"
#include "elf32_writer.hpp"
#include "elf64_writer.hpp"
#include "../config.hpp"

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
    if (mnemonic == "JC" || mnemonic == "JB" || mnemonic == "JNAE") return 0x82;
    if (mnemonic == "JNC" || mnemonic == "JAE" || mnemonic == "JNB") return 0x83;
    if (mnemonic == "JO") return 0x80;
    if (mnemonic == "JNO") return 0x81;
    if (mnemonic == "JS") return 0x88;
    if (mnemonic == "JNS") return 0x89;
    if (mnemonic == "JBE" || mnemonic == "JNA") return 0x86;
    if (mnemonic == "JA" || mnemonic == "JNBE") return 0x87;
    return std::nullopt;
}

struct EncodedMemoryOperand {
    std::vector<uint8_t> bytes;
    size_t displacement_offset = 0;
    size_t displacement_size = 0;
    bool has_symbol_relocation = false;
    int64_t relocation_addend = 0;
    std::string relocation_symbol;
    bool is_pc_relative = false;
};

std::optional<uint8_t> parse_reg_id(const std::string& upper) {
    if (upper == "EAX" || upper == "RAX" || upper == "AX" || upper == "AL") return 0;
    if (upper == "ECX" || upper == "RCX" || upper == "CX" || upper == "CL") return 1;
    if (upper == "EDX" || upper == "RDX" || upper == "DX" || upper == "DL") return 2;
    if (upper == "EBX" || upper == "RBX" || upper == "BX" || upper == "BL") return 3;
    if (upper == "ESP" || upper == "RSP" || upper == "SP") return 4;
    if (upper == "EBP" || upper == "RBP" || upper == "BP") return 5;
    if (upper == "ESI" || upper == "RSI" || upper == "SI") return 6;
    if (upper == "EDI" || upper == "RDI" || upper == "DI") return 7;
    // R8–R15: parse R8/R8D/R8W/R8B pattern
    if (upper.size() >= 2 && upper[0] == 'R') {
        try { int n = std::stoi(upper.substr(1)); if (n >= 8 && n <= 15) return n; }
        catch (...) {}
    }
    if (upper.size() >= 3 && upper[0] == 'R' && upper.back() == 'D') {
        try { int n = std::stoi(upper.substr(1, upper.size()-2)); if (n >= 8 && n <= 15) return n; }
        catch (...) {}
    }
    if (upper.size() >= 3 && upper[0] == 'R' && upper.back() == 'W') {
        try { int n = std::stoi(upper.substr(1, upper.size()-2)); if (n >= 8 && n <= 15) return n; }
        catch (...) {}
    }
    if (upper.size() >= 3 && upper[0] == 'R' && upper.back() == 'B') {
        try { int n = std::stoi(upper.substr(1, upper.size()-2)); if (n >= 8 && n <= 15) return n; }
        catch (...) {}
    }
    return std::nullopt;
}

// Determine operand size in bits from register name
static int operand_size_from_reg(const std::string& name) {
    std::string upper = upper_copy(name);
    if (upper.empty()) return 32;
    if (upper.back() == 'L' || upper.back() == 'H') return 8;
    if (upper.back() == 'X' && upper.size() == 2) return 16;   // AX, BX, ...
    if (upper.back() == 'L' && upper.size() > 2) return 8;      // R8B, etc already caught
    if (upper.back() == 'W' && upper.size() > 2) return 16;     // R8W
    if (upper.back() == 'D' && upper.size() > 2) return 32;     // R8D
    if (upper[0] == 'R' && upper.size() >= 2) {
        if (upper[1] >= '0' && upper[1] <= '9') return 64;     // R8, R9, ...
    }
    if (upper[0] == 'E') return 32;  // EAX, EBX, ...
    if (upper[0] == 'R' && upper.size() >= 2 && upper[1] != '8' && upper[1] != '9' && upper[1] != '1') return 64; // RAX, etc.
    return 32;
}

// Compute REX prefix byte for x86-64.
// reg_id: the register field (ModR/M reg or opcode reg), can be nullopt
// rm_id: the ModR/M rm field or base, can be nullopt
// index_id: the SIB index field, can be nullopt
// is64: true for 64-bit operand size (sets REX.W)
static uint8_t compute_rex(bool is64, std::optional<uint8_t> reg_id,
                           std::optional<uint8_t> rm_id = std::nullopt,
                           std::optional<uint8_t> index_id = std::nullopt) {
    uint8_t rex = 0x40;
    if (is64) rex |= 0x08;
    if (reg_id && *reg_id >= 8) rex |= 0x04;
    if (index_id && *index_id >= 8) rex |= 0x02;
    if (rm_id && *rm_id >= 8) rex |= 0x01;
    return rex;
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

    if (is64) {
        std::optional<uint8_t> base_id, idx_id;
        if (memory.base) base_id = parse_reg_id(upper_copy(*memory.base));
        if (memory.index) idx_id = parse_reg_id(upper_copy(*memory.index));
        encoded.bytes.push_back(compute_rex(true, reg_field, base_id, idx_id));
    } else {
        // 8-bit ops in 64-bit mode still need REX for R8-R15, but without REX.W
        std::optional<uint8_t> base_id, idx_id;
        if (memory.base) base_id = parse_reg_id(upper_copy(*memory.base));
        if (memory.index) idx_id = parse_reg_id(upper_copy(*memory.index));
        bool needs_rex = (base_id && *base_id >= 8) || (idx_id && *idx_id >= 8) || reg_field >= 8;
        if (needs_rex)
            encoded.bytes.push_back(compute_rex(false, reg_field, base_id, idx_id));
    }
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
        encoded.is_pc_relative = is64;
        return encoded;
    }

    // Symbol + index (no base): [symbol + index*scale + displacement]
    if (memory.symbol && !memory.base && memory.index) {
        auto idx_id = parse_reg_id(upper_copy(*memory.index));
        if (!idx_id) { errors.push_back("unknown index register"); return encoded; }
        uint8_t scale_val = memory.scale ? static_cast<uint8_t>(memory.scale) : 1;
        uint8_t sib = sib_byte(scale_val, *idx_id, 5); // base=5 (no base with disp32)
        encoded.bytes.push_back(static_cast<uint8_t>((0b00 << 6) | ((reg_field & 0x7) << 3) | 0b100));
        encoded.bytes.push_back(sib);
        encoded.displacement_offset = encoded.bytes.size();
        encoded.displacement_size = 4;
        append_i32(encoded.bytes, static_cast<int32_t>(memory.displacement));
        encoded.has_symbol_relocation = true;
        encoded.relocation_symbol = *memory.symbol;
        encoded.relocation_addend = memory.displacement;
        encoded.is_pc_relative = is64;
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
    const size_t epilogue_size = is_64bit_mode() ? 2 : 1;  // LEAVE (REX.W + 0xC9 in 64-bit)

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
            if (is_64bit_mode()) text_bytes.push_back(0x48); // REX.W for 64-bit LEAVE
            text_bytes.push_back(0xC9); // LEAVE
        }

        const auto instruction_offset = text_offset_map[static_cast<uint64_t>(instruction_index)];

        auto encoded = encode_instruction(instruction, instruction_offset, text_symbol_offsets, adjusted_program.equ_constants, artifact.errors, artifact.warnings);
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

BackendArtifact X86Backend::emit_executable(const IRProgram& program) {
    auto reloc = emit(program);
    if (!reloc.ok()) return reloc;

    // Build line entries from program instructions for DWARF
    std::vector<Assembler::LineEntry> line_entries;
    if (Config::debug_info) {
        size_t offset = 0;
        for (const auto& instr : program.instructions) {
            if (instr.section == IRSectionKind::Text && instr.line > 0) {
                line_entries.push_back({offset, instr.line});
            }
            offset += 4;  // approximate
        }
    }

    std::vector<std::string> errs;
    std::vector<uint8_t> exe;
    if (is_64bit_mode()) {
        exe = make_elf64_executable(reloc.bytes, program, errs, &line_entries, "", Config::shared_library);
    } else {
        exe = make_elf32_executable(reloc.bytes, program, errs);
    }
    if (!errs.empty()) {
        reloc.errors.insert(reloc.errors.end(), errs.begin(), errs.end());
        return reloc;
    }
    reloc.bytes = exe;
    return reloc;
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
            const auto reg = encode_register_id(std::get<IRRegisterOperand>(instruction.operands[0].value).name);
            if (!reg) { errors.push_back("bad register"); return 0; }
            return 2;
        }
        // Memory-indirect: CALL [mem] / JMP [mem]
        if (instruction.operands.size() == 1 &&
            instruction.operands[0].kind == IROperandKind::Memory) {
            return compute_memory_operand_size(std::get<IRMemoryOperand>(instruction.operands[0].value), errors);
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
        if (instruction.operands.size() != 1) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " with one operand");
            return 0;
        }
        if (instruction.operands[0].kind == IROperandKind::Register)
            return 1;
        if (instruction.operands[0].kind == IROperandKind::Immediate) {
            const auto imm = std::get<IRImmediateOperand>(instruction.operands[0].value).value;
            return fits_i8(imm) ? 2 : 5;
        }
        if (instruction.operands[0].kind == IROperandKind::Memory)
            return compute_memory_operand_size(std::get<IRMemoryOperand>(instruction.operands[0].value), errors);
        errors.push_back("x86 backend expects " + instruction.mnemonic + " reg, imm, or [mem]");
        return 0;
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
            errors.push_back("x86 backend expects NEG with one operand");
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

    if (mnemonic == "MUL" || mnemonic == "DIV") {
        if (instruction.operands.size() != 1 ||
            instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " reg");
            return 0;
        }
        return 2;
    }

    if (mnemonic == "ADC" || mnemonic == "SBB") {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Register ||
            instruction.operands[1].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " reg, reg");
            return 0;
        }
        return 2;
    }

    if (mnemonic == "MOVSX" || mnemonic == "MOVZX") {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Register ||
            instruction.operands[1].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " reg, reg");
            return 0;
        }
        return 3; // 0F opcode + modrm
    }

    if (mnemonic == "SAL" || mnemonic == "SAR") {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " reg, imm8 or reg, CL");
            return 0;
        }
        if (instruction.operands[1].kind == IROperandKind::Immediate) return 3;
        if (instruction.operands[1].kind == IROperandKind::Register) return 2;
        if (instruction.operands[1].kind == IROperandKind::Symbol) return 2;
    }

    if (mnemonic == "LOOP" || mnemonic == "LOOPE" || mnemonic == "LOOPNE") {
        if (instruction.operands.size() != 1 ||
            instruction.operands[0].kind != IROperandKind::Symbol) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " label");
            return 0;
        }
        return 2; // opcode + rel8
    }

    // Zero-operand instructions
    if (mnemonic == "CLC" || mnemonic == "STC" || mnemonic == "CMC" ||
        mnemonic == "CLD" || mnemonic == "STD" ||
        mnemonic == "CBW" || mnemonic == "CWDE" || mnemonic == "CWD" || mnemonic == "CDQ" ||
        mnemonic == "LAHF" || mnemonic == "SAHF" ||
        mnemonic == "CPUID" || mnemonic == "RDTSC" ||
        mnemonic == "SYSCALL" || mnemonic == "SYSENTER") {
        return mnemonic == "CPUID" || mnemonic == "RDTSC" ||
               mnemonic == "SYSCALL" || mnemonic == "SYSENTER" ? 2 : 1;
    }

    // REP prefix
    if (mnemonic == "REP") return 1;

    // Rotates: ROL/ROR/RCL/RCR reg, imm8 or reg, CL
    if (mnemonic == "ROL" || mnemonic == "ROR" || mnemonic == "RCL" || mnemonic == "RCR") {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " reg, imm8 or reg, CL");
            return 0;
        }
        if (instruction.operands[1].kind == IROperandKind::Immediate) return 3;
        if (instruction.operands[1].kind == IROperandKind::Register) return 2;
        if (instruction.operands[1].kind == IROperandKind::Symbol) return 2;
    }

    // IDIV: same pattern as DIV
    if (mnemonic == "IDIV") {
        if (instruction.operands.size() != 1 ||
            instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects IDIV reg");
            return 0;
        }
        return 2;
    }

    // BSWAP reg
    if (mnemonic == "BSWAP") {
        if (instruction.operands.size() != 1 ||
            instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects BSWAP reg");
            return 0;
        }
        return 2; // 0F C8+reg
    }

    // XCHG reg,reg
    if (mnemonic == "XCHG") {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Register ||
            instruction.operands[1].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects XCHG reg, reg");
            return 0;
        }
        return 2;
    }

    // ENTER imm16, imm8
    if (mnemonic == "ENTER") {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Immediate ||
            instruction.operands[1].kind != IROperandKind::Immediate) {
            errors.push_back("x86 backend expects ENTER imm16, imm8");
            return 0;
        }
        return 4; // C8 + imm16 + imm8
    }

    // BT/BTS/BTR/BTC reg,reg
    if (mnemonic == "BT" || mnemonic == "BTS" || mnemonic == "BTR" || mnemonic == "BTC") {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Register ||
            instruction.operands[1].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " reg, reg");
            return 0;
        }
        return 3; // 0F xx /r
    }

    // CMPXCHG reg,reg — implicit AL/AX/EAX/RAX
    if (mnemonic == "CMPXCHG" || mnemonic == "XADD") {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Register ||
            instruction.operands[1].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " reg, reg");
            return 0;
        }
        return 3; // 0F xx /r
    }

    // CMOVcc reg,reg
    if (mnemonic.rfind("CMOV", 0) == 0) {
        if (instruction.operands.size() != 2 ||
            instruction.operands[0].kind != IROperandKind::Register ||
            instruction.operands[1].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " reg, reg");
            return 0;
        }
        return 3; // 0F 4x /r
    }

    // String ops
    if (mnemonic == "MOVSB" || mnemonic == "MOVSW" || mnemonic == "MOVSD" ||
        mnemonic == "STOSB" || mnemonic == "STOSW" || mnemonic == "STOSD" ||
        mnemonic == "LODSB" || mnemonic == "LODSW" || mnemonic == "LODSD") {
        return 1; // single byte opcode (word variants get 66 prefix)
    }

    if (mnemonic.rfind("SET", 0) == 0) { // SETcc
        if (instruction.operands.size() != 1 ||
            instruction.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + instruction.mnemonic + " reg8");
            return 0;
        }
        return 3; // 0F 9x C0+reg
    }

    if (mnemonic == "IMUL") {
        if (instruction.operands.size() == 1) {
            if (instruction.operands[0].kind != IROperandKind::Register) {
                errors.push_back("x86 backend expects IMUL reg, IMUL reg,reg, or IMUL reg,reg,imm");
                return 0;
            }
            return 2; // F7 /5
        }
        if (instruction.operands.size() == 2) {
            if (instruction.operands[0].kind != IROperandKind::Register ||
                instruction.operands[1].kind != IROperandKind::Register) {
                errors.push_back("x86 backend expects IMUL reg, reg");
                return 0;
            }
            return 3; // 0F AF /r
        }
        if (instruction.operands.size() == 3) {
            if (instruction.operands[0].kind != IROperandKind::Register ||
                instruction.operands[1].kind != IROperandKind::Register ||
                instruction.operands[2].kind != IROperandKind::Immediate) {
                errors.push_back("x86 backend expects IMUL reg, reg, imm");
                return 0;
            }
            const int64_t imm = std::get<IRImmediateOperand>(instruction.operands[2].value).value;
            return fits_i8(imm) ? 3 : 6; // 6B /r ib or 69 /r id
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
    const std::unordered_map<std::string, int64_t>& equ_constants,
    std::vector<std::string>& errors,
    std::vector<std::string>& warnings) const {
    EncodedInstructionResult result;
    const std::string mnemonic = upper_copy(instruction.mnemonic);

    // Substitute .equ constants: replace Symbol operands with Immediate
    auto resolve_equ = [&](const IROperand& op) -> IROperand {
        if (op.kind == IROperandKind::Symbol) {
            auto it = equ_constants.find(std::get<IRSymbolOperand>(op.value).name);
            if (it != equ_constants.end())
                return {IROperandKind::Immediate, IRImmediateOperand{it->second}};
        }
        return op;
    };

    // Create a local copy with resolved operands
    IRInstruction resolved = instruction;
    for (auto& op : resolved.operands)
        op = resolve_equ(op);

    // Use resolved throughout
    const auto& inst = resolved;

    auto warn_unsized_memory = [&](const IRMemoryOperand& mem, const std::string& context) {
        if (!mem.width_bits && !mem.symbol) {
            warnings.push_back("warning: memory operand has no size qualifier in " + context +
                           " — use dword/word/byte before [mem]");
        }
    };

    if (mnemonic == "NOP") {
        result.bytes = {0x90};
        return result;
    }

    if (mnemonic == "RET") {
        result.bytes = {0xC3};
        return result;
    }

    if (mnemonic == "INT") {
        if (inst.operands.size() != 1 || inst.operands[0].kind != IROperandKind::Immediate) {
            errors.push_back("x86 backend expects INT imm8");
            return result;
        }
        const auto value = std::get<IRImmediateOperand>(inst.operands[0].value).value;
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
            inst.operands.size() == 1 &&
            inst.operands[0].kind == IROperandKind::Register) {
            const auto reg = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
            if (!reg) {
                errors.push_back("unsupported register in " + inst.mnemonic);
                return result;
            }
            result.bytes = {0xFF, static_cast<uint8_t>((mnemonic == "CALL" ? 0xD0 : 0xE0) | (*reg & 0x7))};
            return result;
        }

        // Memory-indirect: CALL [mem] / JMP [mem]
        if ((mnemonic == "CALL" || mnemonic == "JMP") &&
            inst.operands.size() == 1 &&
            inst.operands[0].kind == IROperandKind::Memory) {
            const uint8_t subcode = mnemonic == "CALL" ? 2 : 4;
            const auto encoded_mem = encode_memory_operand32(
                std::get<IRMemoryOperand>(inst.operands[0].value), subcode, 0xFF, false, errors);
            if (!errors.empty()) return result;
            result.bytes = encoded_mem.bytes;
            return result;
        }

        if (inst.operands.size() != 1 || inst.operands[0].kind != IROperandKind::Symbol) {
            errors.push_back("x86 backend currently supports symbolic control-flow operands only");
            return result;
        }

        const auto& symbol = std::get<IRSymbolOperand>(inst.operands[0].value).name;
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
        if (inst.operands.size() != 2) {
            errors.push_back("x86 backend expects MOV with two operands");
            return result;
        }

        const auto& dst = inst.operands[0];
        const auto& src = inst.operands[1];

        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Immediate) {
            const auto reg = encode_register_id(std::get<IRRegisterOperand>(dst.value).name);
            if (!reg) {
                errors.push_back("x86 backend does not support that register in MOV reg, imm");
                return result;
            }
            if (is_64bit_mode()) {
                uint8_t rex = compute_rex(true, std::nullopt, reg);
                result.bytes = {rex, 0xC7, static_cast<uint8_t>(0xC0 | (*reg & 0x7))};
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
                uint8_t rex = compute_rex(true, std::nullopt, reg);
                result.bytes = {rex, 0xC7, static_cast<uint8_t>(0xC0 | (*reg & 0x7))};
                append_u32(result.bytes, 0);
            } else {
                result.bytes = {static_cast<uint8_t>(0xB8 + *reg)};
                append_u32(result.bytes, 0);
            }
            IRRelocation relocation;
            relocation.section = IRSectionKind::Text;
            relocation.offset = instruction_offset + 1;
            relocation.symbol = symbol_name;
            relocation.kind = is_64bit_mode() ? IRRelocationKind::Absolute32S : IRRelocationKind::Absolute32;
            relocation.addend = src.reloc_addend;
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
                uint8_t rex = compute_rex(true, dst_reg, src_reg);
                result.bytes = {rex, 0x89, static_cast<uint8_t>(0xC0 | ((*src_reg & 0x7) << 3) | (*dst_reg & 0x7))};
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

            int opsize = operand_size_from_reg(std::get<IRRegisterOperand>(dst.value).name);
            uint8_t opcode = (opsize == 8) ? 0x8A : 0x8B;
            bool use_64 = (opsize == 64) || (is_64bit_mode() && opsize >= 32);
            const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(src.value), *dst_reg, opcode, use_64, errors);
            if (opsize == 16) result.bytes.push_back(0x66); // operand size override
            if (!errors.empty()) {
                return result;
            }
            result.bytes = encoded_mem.bytes;
            if (encoded_mem.has_symbol_relocation) {
                IRRelocation relocation;
                relocation.section = IRSectionKind::Text;
                relocation.offset = instruction_offset + encoded_mem.displacement_offset;
                relocation.symbol = encoded_mem.relocation_symbol;
                relocation.kind = encoded_mem.is_pc_relative ? IRRelocationKind::PcRelative32
                    : (is_64bit_mode() ? IRRelocationKind::Absolute32S : IRRelocationKind::Absolute32);
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

            warn_unsized_memory(std::get<IRMemoryOperand>(dst.value), "MOV [mem], reg");

            int opsize = operand_size_from_reg(std::get<IRRegisterOperand>(src.value).name);
            uint8_t opcode = (opsize == 8) ? 0x88 : 0x89;
            bool use_64 = (opsize == 64) || (is_64bit_mode() && opsize >= 32);
            const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(dst.value), *src_reg, opcode, use_64, errors);
            if (opsize == 16) result.bytes.push_back(0x66);
            if (!errors.empty()) {
                return result;
            }
            result.bytes = encoded_mem.bytes;
            if (encoded_mem.has_symbol_relocation) {
                IRRelocation relocation;
                relocation.section = IRSectionKind::Text;
                relocation.offset = instruction_offset + encoded_mem.displacement_offset;
                relocation.symbol = encoded_mem.relocation_symbol;
                relocation.kind = encoded_mem.is_pc_relative ? IRRelocationKind::PcRelative32
                    : (is_64bit_mode() ? IRRelocationKind::Absolute32S : IRRelocationKind::Absolute32);
                relocation.addend = encoded_mem.relocation_addend;
                result.relocations.push_back(std::move(relocation));
            }
            return result;
        }

        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Immediate) {
            const int64_t imm = std::get<IRImmediateOperand>(src.value).value;
            const auto& mem = std::get<IRMemoryOperand>(dst.value);
            warn_unsized_memory(mem, "MOV [mem], imm");
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
        if (inst.operands.size() != 1) {
            errors.push_back("x86 backend expects PUSH with one operand");
            return result;
        }

        if (inst.operands[0].kind == IROperandKind::Register) {
            const auto reg = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
            if (!reg) { errors.push_back("x86 backend does not support that register in PUSH"); return result; }
            if (is_64bit_mode() && *reg >= 8)
                result.bytes.push_back(0x41);
            result.bytes.push_back(static_cast<uint8_t>(0x50 + (*reg & 0x7)));
            return result;
        }

        if (inst.operands[0].kind == IROperandKind::Immediate) {
            const int64_t imm = std::get<IRImmediateOperand>(inst.operands[0].value).value;
            if (fits_i8(imm)) {
                result.bytes.push_back(0x6A);
                result.bytes.push_back(static_cast<uint8_t>(imm));
            } else {
                result.bytes.push_back(0x68);
                append_u32(result.bytes, static_cast<uint32_t>(imm));
            }
            return result;
        }

        if (inst.operands[0].kind == IROperandKind::Memory) {
            const auto& mem = std::get<IRMemoryOperand>(inst.operands[0].value);
            const auto encoded_mem = encode_memory_operand32(mem, 6, 0xFF, is_64bit_mode(), errors);
            if (!errors.empty()) return result;
            result.bytes = encoded_mem.bytes;
            return result;
        }

        errors.push_back("x86 backend expects PUSH reg, imm, or [mem]");
        return result;
    }

    if (mnemonic == "POP") {
        if (inst.operands.size() != 1) {
            errors.push_back("x86 backend expects POP with one operand");
            return result;
        }

        if (inst.operands[0].kind == IROperandKind::Register) {
            const auto reg = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
            if (!reg) { errors.push_back("x86 backend does not support that register in POP"); return result; }
            if (is_64bit_mode() && *reg >= 8)
                result.bytes.push_back(0x41);
            result.bytes.push_back(static_cast<uint8_t>(0x58 + (*reg & 0x7)));
            return result;
        }

        if (inst.operands[0].kind == IROperandKind::Memory) {
            const auto& mem = std::get<IRMemoryOperand>(inst.operands[0].value);
            const auto encoded_mem = encode_memory_operand32(mem, 0, 0x8F, is_64bit_mode(), errors);
            if (!errors.empty()) return result;
            result.bytes = encoded_mem.bytes;
            return result;
        }

        errors.push_back("x86 backend expects POP reg or [mem]");
        return result;
    }

    if (mnemonic == "INC") {
        if (inst.operands.size() != 1) {
            errors.push_back("x86 backend expects INC with one operand");
            return result;
        }
        if (inst.operands[0].kind == IROperandKind::Register) {
            const auto reg = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
            if (!reg) {
                errors.push_back("x86 backend does not support that register in INC");
                return result;
            }
            if (is_64bit_mode()) {
                uint8_t rex = compute_rex(true, std::nullopt, reg);
                result.bytes = {rex, 0xFF, static_cast<uint8_t>(0xC0 | (*reg & 0x7))};
            } else {
                result.bytes = {static_cast<uint8_t>(0x40 + *reg)};
            }
            return result;
        }
        if (inst.operands[0].kind == IROperandKind::Memory) {
            const auto& mem = std::get<IRMemoryOperand>(inst.operands[0].value);
            warn_unsized_memory(mem, "INC [mem]");
            const auto encoded_mem = encode_memory_operand32(mem, 0, 0xFF, is_64bit_mode(), errors);
            if (!errors.empty()) return result;
            result.bytes = encoded_mem.bytes;
            return result;
        }
    }

    if (mnemonic == "DEC") {
        if (inst.operands.size() != 1) {
            errors.push_back("x86 backend expects DEC with one operand");
            return result;
        }
        if (inst.operands[0].kind == IROperandKind::Register) {
            const auto reg = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
            if (!reg) {
                errors.push_back("x86 backend does not support that register in DEC");
                return result;
            }
            if (is_64bit_mode()) {
                uint8_t rex = compute_rex(true, std::nullopt, reg);
                result.bytes = {rex, 0xFF, static_cast<uint8_t>(0xC8 | (*reg & 0x7))};
            } else {
                result.bytes = {static_cast<uint8_t>(0x48 + *reg)};
            }
            return result;
        }
        if (inst.operands[0].kind == IROperandKind::Memory) {
            const auto& mem = std::get<IRMemoryOperand>(inst.operands[0].value);
            warn_unsized_memory(mem, "DEC [mem]");
            const auto encoded_mem = encode_memory_operand32(mem, 1, 0xFF, is_64bit_mode(), errors);
            if (!errors.empty()) return result;
            result.bytes = encoded_mem.bytes;
            return result;
        }
    }

    if (mnemonic == "ADD") {
        if (inst.operands.size() != 2) {
            errors.push_back("x86 backend expects ADD with two operands");
            return result;
        }
        const auto& dst = inst.operands[0];
        const auto& src = inst.operands[1];

        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Register) {
            const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(dst.value).name);
            const auto src_reg = encode_register_id(std::get<IRRegisterOperand>(src.value).name);
            if (!dst_reg || !src_reg) {
                errors.push_back("x86 backend does not support that register in ADD reg, reg");
                return result;
            }
            if (is_64bit_mode()) result.bytes.push_back(compute_rex(true, dst_reg, src_reg));
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
            warn_unsized_memory(std::get<IRMemoryOperand>(dst.value), "ADD [mem], reg");
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
        if (inst.operands.size() != 2) {
            errors.push_back("x86 backend expects SUB with two operands");
            return result;
        }
        const auto& dst = inst.operands[0];
        const auto& src = inst.operands[1];

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
            warn_unsized_memory(std::get<IRMemoryOperand>(dst.value), "SUB [mem], reg");
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
        if (inst.operands.size() != 2) {
            errors.push_back("x86 backend expects CMP with two operands");
            return result;
        }
        const auto& dst = inst.operands[0];
        const auto& src = inst.operands[1];

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
        if (inst.operands.size() != 2) {
            errors.push_back("x86 backend expects " + inst.mnemonic + " with two operands");
            return result;
        }
        const auto& dst = inst.operands[0];
        const auto& src = inst.operands[1];

        uint8_t reg_opcode = 0x31;
        uint8_t imm_opcode = 0xF0;
        if (mnemonic == "AND") { reg_opcode = 0x21; imm_opcode = 0xE0; }
        if (mnemonic == "OR")  { reg_opcode = 0x09; imm_opcode = 0xC8; }

        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Register) {
            const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(dst.value).name);
            const auto src_reg = encode_register_id(std::get<IRRegisterOperand>(src.value).name);
            if (!dst_reg || !src_reg) {
                errors.push_back("bad register in " + inst.mnemonic + " reg, reg");
                return result;
            }
            if (is_64bit_mode()) result.bytes.push_back(compute_rex(true, dst_reg, src_reg));
            result.bytes.push_back(reg_opcode);
            result.bytes.push_back(static_cast<uint8_t>(0xC0 | ((*src_reg & 0x7) << 3) | (*dst_reg & 0x7)));
            return result;
        }

        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Immediate) {
            const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(dst.value).name);
            if (!dst_reg) { errors.push_back("bad register in " + inst.mnemonic + " reg, imm"); return result; }
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
            if (!src_reg) { errors.push_back("bad src register in " + inst.mnemonic + " [mem], reg"); return result; }
            const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(dst.value), *src_reg, reg_opcode, is_64bit_mode(), errors);
            if (!errors.empty()) return result;
            result.bytes = encoded_mem.bytes;
            return result;
        }
    }

    if (mnemonic == "NOT") {
        if (inst.operands.size() != 1 ||
            inst.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects NOT reg");
            return result;
        }
        const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
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
        if (inst.operands.size() != 2 ||
            inst.operands[0].kind != IROperandKind::Register ||
            inst.operands[1].kind != IROperandKind::Memory) {
            errors.push_back("x86 backend expects LEA reg, [mem]");
            return result;
        }
        const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
        if (!dst_reg) {
            errors.push_back("x86 backend does not support that register in LEA");
            return result;
        }

        const auto& mem = std::get<IRMemoryOperand>(inst.operands[1].value);

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
                relocation.kind = encoded_mem.is_pc_relative ? IRRelocationKind::PcRelative32
                    : (is_64bit_mode() ? IRRelocationKind::Absolute32S : IRRelocationKind::Absolute32);
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
        if (inst.operands.size() != 1) {
            errors.push_back("x86 backend expects NEG with one operand");
            return result;
        }
        if (inst.operands[0].kind == IROperandKind::Register) {
            const auto reg = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
            if (!reg) { errors.push_back("bad register in NEG"); return result; }
            if (is_64bit_mode()) result.bytes.push_back(compute_rex(true, std::nullopt, reg));
            result.bytes.push_back(0xF7);
            result.bytes.push_back(static_cast<uint8_t>(0xD8 | (*reg & 0x7)));
            return result;
        }
        if (inst.operands[0].kind == IROperandKind::Memory) {
            const auto& mem = std::get<IRMemoryOperand>(inst.operands[0].value);
            const auto encoded_mem = encode_memory_operand32(mem, 3, 0xF7, is_64bit_mode(), errors);
            if (!errors.empty()) return result;
            result.bytes = encoded_mem.bytes;
            return result;
        }
        errors.push_back("x86 backend expects NEG reg or [mem]");
        return result;
    }

    if (mnemonic == "TEST") {
        if (inst.operands.size() != 2) {
            errors.push_back("x86 backend expects TEST with two operands");
            return result;
        }
        const auto& dst = inst.operands[0];
        const auto& src = inst.operands[1];

        // TEST reg, reg
        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Register) {
            const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(dst.value).name);
            const auto src_reg = encode_register_id(std::get<IRRegisterOperand>(src.value).name);
            if (!dst_reg || !src_reg) { errors.push_back("bad register in TEST"); return result; }
            if (is_64bit_mode()) result.bytes.push_back(compute_rex(true, dst_reg, src_reg));
            result.bytes.push_back(0x85);
            result.bytes.push_back(static_cast<uint8_t>(0xC0 | ((*src_reg & 0x7) << 3) | (*dst_reg & 0x7)));
            return result;
        }
        // TEST reg, imm
        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Immediate) {
            const auto dst_reg = encode_register_id(std::get<IRRegisterOperand>(dst.value).name);
            if (!dst_reg) { errors.push_back("bad register in TEST"); return result; }
            int64_t imm = std::get<IRImmediateOperand>(src.value).value;
            if (is_64bit_mode()) result.bytes.push_back(compute_rex(true, std::nullopt, dst_reg));
            if (fits_i8(imm)) {
                result.bytes.push_back(0xF6);
                result.bytes.push_back(static_cast<uint8_t>(0xC0 | (*dst_reg & 0x7)));
                result.bytes.push_back(static_cast<uint8_t>(imm));
            } else {
                result.bytes.push_back(0xF7);
                result.bytes.push_back(static_cast<uint8_t>(0xC0 | (*dst_reg & 0x7)));
                append_u32(result.bytes, static_cast<uint32_t>(imm));
            }
            return result;
        }
        // TEST [mem], reg
        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Register) {
            const auto src_reg = encode_register_id(std::get<IRRegisterOperand>(src.value).name);
            if (!src_reg) { errors.push_back("bad register in TEST"); return result; }
            const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(dst.value), *src_reg, 0x85, is_64bit_mode(), errors);
            if (!errors.empty()) return result;
            result.bytes = encoded_mem.bytes;
            return result;
        }
        // TEST [mem], imm
        if (dst.kind == IROperandKind::Memory && src.kind == IROperandKind::Immediate) {
            int64_t imm = std::get<IRImmediateOperand>(src.value).value;
            const auto& mem = std::get<IRMemoryOperand>(dst.value);
            const auto encoded_mem = encode_memory_operand32(mem, 0, fits_i8(imm) ? 0xF6 : 0xF7, is_64bit_mode(), errors);
            if (!errors.empty()) return result;
            result.bytes = encoded_mem.bytes;
            if (fits_i8(imm)) result.bytes.push_back(static_cast<uint8_t>(imm));
            else append_u32(result.bytes, static_cast<uint32_t>(imm));
            return result;
        }
        errors.push_back("x86 backend: unsupported TEST operand combination");
        return result;
    }

    if (mnemonic == "SHL" || mnemonic == "SHR") {
        if (inst.operands.size() != 2 ||
            inst.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + inst.mnemonic + " reg, imm8 or reg, CL");
            return result;
        }
        const auto reg = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
        if (!reg) { errors.push_back("bad register in " + inst.mnemonic); return result; }
        const uint8_t subcode = mnemonic == "SHL" ? 0xE0 : 0xE8;

        if (inst.operands[1].kind == IROperandKind::Immediate) {
            if (is_64bit_mode()) result.bytes.push_back(0x48);
            result.bytes.push_back(0xC1);
            result.bytes.push_back(static_cast<uint8_t>(subcode | (*reg & 0x7)));
            result.bytes.push_back(static_cast<uint8_t>(std::get<IRImmediateOperand>(inst.operands[1].value).value & 0xFF));
            return result;
        }

        if (inst.operands[1].kind == IROperandKind::Register) {
            const auto cl = encode_register_id(std::get<IRRegisterOperand>(inst.operands[1].value).name);
            if (!cl || *cl != 1) { errors.push_back("x86 backend only supports ECX register for variable shift"); return result; }
            if (is_64bit_mode()) result.bytes.push_back(0x48);
            result.bytes.push_back(0xD3);
            result.bytes.push_back(static_cast<uint8_t>(subcode | (*reg & 0x7)));
            return result;
        }

        if (inst.operands[1].kind == IROperandKind::Symbol) {
            const auto& name = std::get<IRSymbolOperand>(inst.operands[1].value).name;
            if (upper_copy(name) != "CL") { errors.push_back("x86 backend only supports CL for variable shift"); return result; }
            if (is_64bit_mode()) result.bytes.push_back(0x48);
            result.bytes.push_back(0xD3);
            result.bytes.push_back(static_cast<uint8_t>(subcode | (*reg & 0x7)));
            return result;
        }
    }

    if (mnemonic == "MUL" || mnemonic == "DIV") {
        if (inst.operands.size() != 1 ||
            inst.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + inst.mnemonic + " reg");
            return result;
        }
        const auto reg = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
        if (!reg) { errors.push_back("bad register in " + inst.mnemonic); return result; }
        const uint8_t subcode = mnemonic == "MUL" ? 0xE0 : 0xF0;
        if (is_64bit_mode()) result.bytes.push_back(0x48);
        result.bytes.push_back(0xF7);
        result.bytes.push_back(static_cast<uint8_t>(subcode | (*reg & 0x7)));
        return result;
    }

    if (mnemonic == "IMUL") {
        if (inst.operands.size() == 1 &&
            inst.operands[0].kind == IROperandKind::Register) {
            const auto reg = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
            if (!reg) { errors.push_back("bad register in IMUL"); return result; }
            if (is_64bit_mode()) result.bytes.push_back(0x48);
            result.bytes.push_back(0xF7);
            result.bytes.push_back(static_cast<uint8_t>(0xE8 | (*reg & 0x7)));
            return result;
        }
        if (inst.operands.size() == 2 &&
            inst.operands[0].kind == IROperandKind::Register &&
            inst.operands[1].kind == IROperandKind::Register) {
            const auto dst = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
            const auto src = encode_register_id(std::get<IRRegisterOperand>(inst.operands[1].value).name);
            if (!dst || !src) { errors.push_back("bad register in IMUL"); return result; }
            if (is_64bit_mode()) result.bytes.push_back(compute_rex(true, src, dst));
            result.bytes.push_back(0x0F);
            result.bytes.push_back(0xAF);
            result.bytes.push_back(static_cast<uint8_t>(0xC0 | ((*dst & 0x7) << 3) | (*src & 0x7)));
            return result;
        }
        if (inst.operands.size() == 3 &&
            inst.operands[0].kind == IROperandKind::Register &&
            inst.operands[1].kind == IROperandKind::Register &&
            inst.operands[2].kind == IROperandKind::Immediate) {
            const auto dst = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
            const auto src = encode_register_id(std::get<IRRegisterOperand>(inst.operands[1].value).name);
            if (!dst || !src) { errors.push_back("bad register in IMUL"); return result; }
            const int64_t imm = std::get<IRImmediateOperand>(inst.operands[2].value).value;
            if (is_64bit_mode()) result.bytes.push_back(compute_rex(true, dst, src));
            if (fits_i8(imm)) {
                result.bytes.push_back(0x6B);
                result.bytes.push_back(static_cast<uint8_t>(0xC0 | ((*src & 0x7) << 3) | (*dst & 0x7)));
                result.bytes.push_back(static_cast<uint8_t>(static_cast<int8_t>(imm)));
            } else {
                result.bytes.push_back(0x69);
                result.bytes.push_back(static_cast<uint8_t>(0xC0 | ((*src & 0x7) << 3) | (*dst & 0x7)));
                append_u32(result.bytes, static_cast<uint32_t>(imm));
            }
            return result;
        }
    }

    if (mnemonic == "ADC" || mnemonic == "SBB") {
        if (inst.operands.size() != 2 ||
            inst.operands[0].kind != IROperandKind::Register ||
            inst.operands[1].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + inst.mnemonic + " reg, reg");
            return result;
        }
        const auto dst = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
        const auto src = encode_register_id(std::get<IRRegisterOperand>(inst.operands[1].value).name);
        if (!dst || !src) { errors.push_back("bad register in " + inst.mnemonic); return result; }
        const uint8_t opcode = mnemonic == "ADC" ? 0x11 : 0x19;
        if (is_64bit_mode()) result.bytes.push_back(compute_rex(true, dst, src));
        result.bytes.push_back(opcode);
        result.bytes.push_back(static_cast<uint8_t>(0xC0 | ((*src & 0x7) << 3) | (*dst & 0x7)));
        return result;
    }

    if (mnemonic == "MOVSX" || mnemonic == "MOVZX") {
        if (inst.operands.size() != 2) {
            errors.push_back("x86 backend expects " + inst.mnemonic + " with two operands");
            return result;
        }
        const auto& dst = inst.operands[0];
        const auto& src = inst.operands[1];
        const uint8_t subcode = mnemonic == "MOVSX" ? 0xBE : 0xB6;

        // MOVSX/MOVZX reg, reg
        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Register) {
            const auto d = encode_register_id(std::get<IRRegisterOperand>(dst.value).name);
            const auto s = encode_register_id(std::get<IRRegisterOperand>(src.value).name);
            if (!d || !s) { errors.push_back("bad register in " + inst.mnemonic); return result; }
            if (is_64bit_mode()) result.bytes.push_back(compute_rex(true, d, s));
            result.bytes.push_back(0x0F);
            result.bytes.push_back(subcode);
            result.bytes.push_back(static_cast<uint8_t>(0xC0 | ((*s & 0x7) << 3) | (*d & 0x7)));
            return result;
        }
        // MOVSX/MOVZX reg, [mem]
        if (dst.kind == IROperandKind::Register && src.kind == IROperandKind::Memory) {
            const auto d = encode_register_id(std::get<IRRegisterOperand>(dst.value).name);
            if (!d) { errors.push_back("bad register in " + inst.mnemonic); return result; }
            const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(src.value), *d, 0x0F, is_64bit_mode(), errors);
            if (!errors.empty()) return result;
            // Override the opcode byte: encode_memory_operand32 puts opcode first, but MOVSX/MOVZX use 0F prefix
            if (encoded_mem.bytes.size() > 0) {
                // Insert 0x0F prefix before the memory operand opcode
                result.bytes = encoded_mem.bytes;
                result.bytes.insert(result.bytes.begin() + (is_64bit_mode() ? 1 : 0), 0x0F);
                result.bytes.insert(result.bytes.begin() + (is_64bit_mode() ? 1 : 0), subcode);
                // Remove the original opcode byte that was used as placeholder
                size_t rex_offset = is_64bit_mode() ? 1 : 0;
                result.bytes.erase(result.bytes.begin() + rex_offset + 2); // remove placeholder opcode
            }
            return result;
        }
        errors.push_back("x86 backend expects " + inst.mnemonic + " reg, reg or reg, [mem]");
        return result;
    }

    if (mnemonic == "SAL" || mnemonic == "SAR") {
        if (inst.operands.size() != 2 ||
            inst.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + inst.mnemonic + " reg, imm8 or reg, CL");
            return result;
        }
        const auto reg = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
        if (!reg) { errors.push_back("bad register in " + inst.mnemonic); return result; }
        const uint8_t subcode = mnemonic == "SAL" ? 0xE0 : 0xF8;
        if (inst.operands[1].kind == IROperandKind::Immediate) {
            if (is_64bit_mode()) result.bytes.push_back(0x48);
            result.bytes.push_back(0xC1);
            result.bytes.push_back(static_cast<uint8_t>(subcode | (*reg & 0x7)));
            result.bytes.push_back(static_cast<uint8_t>(std::get<IRImmediateOperand>(inst.operands[1].value).value & 0xFF));
            return result;
        }
        if (inst.operands[1].kind == IROperandKind::Register || inst.operands[1].kind == IROperandKind::Symbol) {
            if (is_64bit_mode()) result.bytes.push_back(0x48);
            result.bytes.push_back(0xD3);
            result.bytes.push_back(static_cast<uint8_t>(subcode | (*reg & 0x7)));
            return result;
        }
    }

    if (mnemonic == "LOOP" || mnemonic == "LOOPE" || mnemonic == "LOOPNE") {
        if (inst.operands.size() != 1 || inst.operands[0].kind != IROperandKind::Symbol) {
            errors.push_back("x86 backend expects " + inst.mnemonic + " label");
            return result;
        }
        const auto& symbol = std::get<IRSymbolOperand>(inst.operands[0].value).name;
        uint8_t opcode = 0xE2;
        if (mnemonic == "LOOPE") opcode = 0xE1;
        if (mnemonic == "LOOPNE") opcode = 0xE0;
        result.bytes.push_back(opcode);
        auto it = text_symbol_offsets.find(symbol);
        if (it != text_symbol_offsets.end()) {
            const int64_t rel = static_cast<int64_t>(it->second) - static_cast<int64_t>(instruction_offset + 2);
            result.bytes.push_back(static_cast<uint8_t>(static_cast<int8_t>(rel)));
            return result;
        }
        result.bytes.push_back(0);
        IRRelocation relocation;
        relocation.section = IRSectionKind::Text;
        relocation.offset = instruction_offset + 1;
        relocation.symbol = symbol;
        relocation.kind = IRRelocationKind::PcRelative8;
        relocation.addend = 0;
        result.relocations.push_back(std::move(relocation));
        return result;
    }

    if (mnemonic.rfind("SET", 0) == 0) {
        if (inst.operands.size() != 1 ||
            inst.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects SETcc reg8");
            return result;
        }
        const auto reg = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
        if (!reg) { errors.push_back("bad register in " + inst.mnemonic); return result; }
        uint8_t cc = 0x94;
        if (mnemonic == "SETNZ" || mnemonic == "SETNE") cc = 0x95;
        else if (mnemonic == "SETC" || mnemonic == "SETB") cc = 0x92;
        else if (mnemonic == "SETNC" || mnemonic == "SETNB") cc = 0x93;
        else if (mnemonic == "SETO") cc = 0x90;
        else if (mnemonic == "SETNO") cc = 0x91;
        else if (mnemonic == "SETS") cc = 0x98;
        else if (mnemonic == "SETNS") cc = 0x99;
        else if (mnemonic == "SETG") cc = 0x9F;
        else if (mnemonic == "SETGE") cc = 0x9D;
        else if (mnemonic == "SETL") cc = 0x9C;
        else if (mnemonic == "SETLE") cc = 0x9E;
        result.bytes.push_back(0x0F);
        result.bytes.push_back(cc);
        result.bytes.push_back(static_cast<uint8_t>(0xC0 | (*reg & 0x7)));
        return result;
    }

    // Zero-operand instructions
    if (mnemonic == "CLC") { result.bytes = {0xF8}; return result; }
    if (mnemonic == "STC") { result.bytes = {0xF9}; return result; }
    if (mnemonic == "CMC") { result.bytes = {0xF5}; return result; }
    if (mnemonic == "CLD") { result.bytes = {0xFC}; return result; }
    if (mnemonic == "STD") { result.bytes = {0xFD}; return result; }
    if (mnemonic == "CBW" || mnemonic == "CWDE") { result.bytes = {0x98}; return result; }
    if (mnemonic == "CWD" || mnemonic == "CDQ") { result.bytes = {0x99}; return result; }
    if (mnemonic == "LAHF") { result.bytes = {0x9F}; return result; }
    if (mnemonic == "SAHF") { result.bytes = {0x9E}; return result; }
    if (mnemonic == "CPUID") { result.bytes = {0x0F, 0xA2}; return result; }
    if (mnemonic == "RDTSC") { result.bytes = {0x0F, 0x31}; return result; }
    if (mnemonic == "SYSCALL") { result.bytes = {0x0F, 0x05}; return result; }
    if (mnemonic == "SYSENTER") { result.bytes = {0x0F, 0x34}; return result; }
    if (mnemonic == "REP") { result.bytes = {0xF3}; return result; }

    // Rotates
    if (mnemonic == "ROL" || mnemonic == "ROR" || mnemonic == "RCL" || mnemonic == "RCR") {
        if (inst.operands.size() != 2 || inst.operands[0].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + inst.mnemonic + " reg, imm8 or reg, CL"); return result;
        }
        const auto reg = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
        if (!reg) { errors.push_back("bad register in " + inst.mnemonic); return result; }
        uint8_t sub = 0;
        if (mnemonic == "ROL") sub = 0x00; else if (mnemonic == "ROR") sub = 0x08;
        else if (mnemonic == "RCL") sub = 0x10; else sub = 0x18;
        if (inst.operands[1].kind == IROperandKind::Immediate) {
            if (is_64bit_mode()) result.bytes.push_back(0x48);
            result.bytes.push_back(0xC1); result.bytes.push_back(static_cast<uint8_t>(sub | (*reg & 0x7)));
            result.bytes.push_back(static_cast<uint8_t>(std::get<IRImmediateOperand>(inst.operands[1].value).value & 0xFF));
            return result;
        }
        if (is_64bit_mode()) result.bytes.push_back(0x48);
        result.bytes.push_back(0xD3); result.bytes.push_back(static_cast<uint8_t>(sub | (*reg & 0x7)));
        return result;
    }

    // IDIV
    if (mnemonic == "IDIV") {
        if (inst.operands.size() != 1) {
            errors.push_back("x86 backend expects IDIV reg"); return result;
        }
        const auto reg = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
        if (!reg) { errors.push_back("bad register in IDIV"); return result; }
        if (is_64bit_mode()) result.bytes.push_back(0x48);
        result.bytes.push_back(0xF7); result.bytes.push_back(static_cast<uint8_t>(0xF8 | (*reg & 0x7)));
        return result;
    }

    // BSWAP
    if (mnemonic == "BSWAP") {
        if (inst.operands.size() != 1) {
            errors.push_back("x86 backend expects BSWAP reg"); return result;
        }
        const auto reg = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
        if (!reg) { errors.push_back("bad register in BSWAP"); return result; }
        if (is_64bit_mode()) result.bytes.push_back(0x48);
        result.bytes.push_back(0x0F); result.bytes.push_back(static_cast<uint8_t>(0xC8 | (*reg & 0x7)));
        return result;
    }

    // XCHG
    if (mnemonic == "XCHG") {
        if (inst.operands.size() != 2) {
            errors.push_back("x86 backend expects XCHG with two operands"); return result;
        }
        const auto& op1 = inst.operands[0];
        const auto& op2 = inst.operands[1];

        // XCHG reg, reg
        if (op1.kind == IROperandKind::Register && op2.kind == IROperandKind::Register) {
            const auto d = encode_register_id(std::get<IRRegisterOperand>(op1.value).name);
            const auto s = encode_register_id(std::get<IRRegisterOperand>(op2.value).name);
            if (!d || !s) { errors.push_back("bad register in XCHG"); return result; }
            if (is_64bit_mode()) result.bytes.push_back(compute_rex(true, d, s));
            result.bytes.push_back(0x87);
            result.bytes.push_back(static_cast<uint8_t>(0xC0 | ((*s & 0x7) << 3) | (*d & 0x7)));
            return result;
        }
        // XCHG reg, [mem] or XCHG [mem], reg
        if (op1.kind == IROperandKind::Register && op2.kind == IROperandKind::Memory) {
            const auto reg = encode_register_id(std::get<IRRegisterOperand>(op1.value).name);
            if (!reg) { errors.push_back("bad register in XCHG"); return result; }
            const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(op2.value), *reg, 0x87, is_64bit_mode(), errors);
            if (!errors.empty()) return result;
            result.bytes = encoded_mem.bytes;
            return result;
        }
        if (op1.kind == IROperandKind::Memory && op2.kind == IROperandKind::Register) {
            const auto reg = encode_register_id(std::get<IRRegisterOperand>(op2.value).name);
            if (!reg) { errors.push_back("bad register in XCHG"); return result; }
            const auto encoded_mem = encode_memory_operand32(std::get<IRMemoryOperand>(op1.value), *reg, 0x87, is_64bit_mode(), errors);
            if (!errors.empty()) return result;
            result.bytes = encoded_mem.bytes;
            return result;
        }
        errors.push_back("x86 backend expects XCHG reg,reg or reg,[mem]");
        return result;
    }

    // ENTER
    if (mnemonic == "ENTER") {
        if (inst.operands.size() != 2 || inst.operands[0].kind != IROperandKind::Immediate
            || inst.operands[1].kind != IROperandKind::Immediate) {
            errors.push_back("x86 backend expects ENTER imm16, imm8"); return result;
        }
        auto fs = static_cast<uint16_t>(std::get<IRImmediateOperand>(inst.operands[0].value).value);
        auto nl = static_cast<uint8_t>(std::get<IRImmediateOperand>(inst.operands[1].value).value);
        result.bytes = {0xC8, static_cast<uint8_t>(fs & 0xFF), static_cast<uint8_t>((fs >> 8) & 0xFF), nl};
        return result;
    }

    // BT/BTS/BTR/BTC
    if (mnemonic == "BT" || mnemonic == "BTS" || mnemonic == "BTR" || mnemonic == "BTC") {
        if (inst.operands.size() != 2 || inst.operands[0].kind != IROperandKind::Register
            || inst.operands[1].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + inst.mnemonic + " reg, reg"); return result;
        }
        const auto d = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
        const auto s = encode_register_id(std::get<IRRegisterOperand>(inst.operands[1].value).name);
        if (!d || !s) { errors.push_back("bad register"); return result; }
        uint8_t op = 0xA3; if (mnemonic == "BTS") op = 0xAB; else if (mnemonic == "BTR") op = 0xB3; else if (mnemonic == "BTC") op = 0xBB;
        if (is_64bit_mode()) result.bytes.push_back(0x48);
        result.bytes.push_back(0x0F); result.bytes.push_back(op);
        result.bytes.push_back(static_cast<uint8_t>(0xC0 | ((*s & 0x7) << 3) | (*d & 0x7)));
        return result;
    }

    // CMPXCHG / XADD
    if (mnemonic == "CMPXCHG" || mnemonic == "XADD") {
        if (inst.operands.size() != 2 || inst.operands[0].kind != IROperandKind::Register
            || inst.operands[1].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + inst.mnemonic + " reg, reg"); return result;
        }
        const auto d = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
        const auto s = encode_register_id(std::get<IRRegisterOperand>(inst.operands[1].value).name);
        if (!d || !s) { errors.push_back("bad register"); return result; }
        uint8_t op = mnemonic == "CMPXCHG" ? 0xB1 : 0xC1;
        if (is_64bit_mode()) result.bytes.push_back(0x48);
        result.bytes.push_back(0x0F); result.bytes.push_back(op);
        result.bytes.push_back(static_cast<uint8_t>(0xC0 | ((*s & 0x7) << 3) | (*d & 0x7)));
        return result;
    }

    // CMOVcc (16 variants)
    if (mnemonic.rfind("CMOV", 0) == 0) {
        if (inst.operands.size() != 2 || inst.operands[0].kind != IROperandKind::Register
            || inst.operands[1].kind != IROperandKind::Register) {
            errors.push_back("x86 backend expects " + inst.mnemonic + " reg, reg"); return result;
        }
        const auto d = encode_register_id(std::get<IRRegisterOperand>(inst.operands[0].value).name);
        const auto s = encode_register_id(std::get<IRRegisterOperand>(inst.operands[1].value).name);
        if (!d || !s) { errors.push_back("bad register"); return result; }
        uint8_t cc = 0x44;
        if (mnemonic == "CMOVNZ" || mnemonic == "CMOVNE") cc = 0x45;
        else if (mnemonic == "CMOVC" || mnemonic == "CMOVB" || mnemonic == "CMOVNAE") cc = 0x42;
        else if (mnemonic == "CMOVNC" || mnemonic == "CMOVAE" || mnemonic == "CMOVNB") cc = 0x43;
        else if (mnemonic == "CMOVO") cc = 0x40;
        else if (mnemonic == "CMOVNO") cc = 0x41;
        else if (mnemonic == "CMOVS") cc = 0x48;
        else if (mnemonic == "CMOVNS") cc = 0x49;
        else if (mnemonic == "CMOVG" || mnemonic == "CMOVNLE") cc = 0x4F;
        else if (mnemonic == "CMOVGE" || mnemonic == "CMOVNL") cc = 0x4D;
        else if (mnemonic == "CMOVL" || mnemonic == "CMOVNGE") cc = 0x4C;
        else if (mnemonic == "CMOVLE" || mnemonic == "CMOVNG") cc = 0x4E;
        else if (mnemonic == "CMOVA" || mnemonic == "CMOVNBE") cc = 0x47;
        else if (mnemonic == "CMOVBE" || mnemonic == "CMOVNA") cc = 0x46;
        if (is_64bit_mode()) result.bytes.push_back(0x48);
        result.bytes.push_back(0x0F); result.bytes.push_back(cc);
        result.bytes.push_back(static_cast<uint8_t>(0xC0 | ((*s & 0x7) << 3) | (*d & 0x7)));
        return result;
    }

    // String ops
    if (mnemonic == "MOVSB") { result.bytes = {0xA4}; return result; }
    if (mnemonic == "MOVSW") { result.bytes = {0x66, 0xA5}; return result; }
    if (mnemonic == "MOVSD") { result.bytes = {0xA5}; return result; }
    if (mnemonic == "STOSB") { result.bytes = {0xAA}; return result; }
    if (mnemonic == "STOSW") { result.bytes = {0x66, 0xAB}; return result; }
    if (mnemonic == "STOSD") { result.bytes = {0xAB}; return result; }
    if (mnemonic == "LODSB") { result.bytes = {0xAC}; return result; }
    if (mnemonic == "LODSW") { result.bytes = {0x66, 0xAD}; return result; }
    if (mnemonic == "LODSD") { result.bytes = {0xAD}; return result; }

    errors.push_back("x86 backend does not yet support instruction: " + inst.mnemonic);
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