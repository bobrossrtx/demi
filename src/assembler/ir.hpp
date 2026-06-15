#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Assembler {

enum class IRTarget {
    DemiBytecode,
    X86Elf32,
    X86Elf64
};

enum class IRSectionKind {
    Text,
    Data,
    Bss,
    Rodata,
    Custom
};

enum class IRSymbolBinding {
    Local,
    Global,
    External
};

enum class IRRelocationKind {
    Absolute32,
    Absolute32S,   // R_X86_64_32S — sign-extended 32-bit, preferred for x86-64
    Absolute64,
    PcRelative32,
    PcRelative8,
    PcRelative64,
    SectionRelative
};

enum class IROperandKind {
    Register,
    Immediate,
    FloatImmediate,
    Memory,
    Symbol,
    STRegister
};

struct IRRegisterOperand {
    std::string name;
};

struct IRImmediateOperand {
    int64_t value = 0;
};

struct IRFloatImmediateOperand {
    double value = 0.0;
};

struct IRSymbolOperand {
    std::string name;
};

struct IRSTRegisterOperand {
    uint8_t index = 0;
};

struct IRMemoryOperand {
    std::optional<std::string> base;
    std::optional<std::string> index;
    uint8_t scale = 1;
    int64_t displacement = 0;
    std::optional<std::string> symbol;
    std::optional<uint8_t> width_bits;
};

using IROperandValue = std::variant<
    IRRegisterOperand,
    IRImmediateOperand,
    IRFloatImmediateOperand,
    IRMemoryOperand,
    IRSymbolOperand,
    IRSTRegisterOperand>;

struct IROperand {
    IROperandKind kind;
    IROperandValue value;
};

struct IRInstruction {
    std::string mnemonic;
    std::vector<IROperand> operands;
    size_t line = 0;
    size_t column = 0;
    IRSectionKind section = IRSectionKind::Text;
};

struct IRDataRecord {
    IRSectionKind section = IRSectionKind::Data;
    std::string directive;
    std::vector<IROperand> values;
    size_t line = 0;
    size_t column = 0;
};

struct IRSymbol {
    std::string name;
    IRSectionKind section = IRSectionKind::Text;
    uint64_t offset = 0;
    IRSymbolBinding binding = IRSymbolBinding::Local;
    bool defined = false;
    bool is_function = false;
    uint64_t size = 0;
    std::optional<int64_t> equ_value;  // .equ constant value
};

struct IRRelocation {
    IRSectionKind section = IRSectionKind::Text;
    uint64_t offset = 0;
    size_t operand_index = 0;
    std::string symbol;
    IRRelocationKind kind = IRRelocationKind::Absolute32;
    int64_t addend = 0;
};

struct IRProgram {
    IRTarget target = IRTarget::DemiBytecode;
    std::vector<IRInstruction> instructions;
    std::vector<IRDataRecord> data_records;
    std::vector<IRSymbol> symbols;
    std::vector<IRRelocation> relocations;
    std::unordered_map<std::string, int64_t> equ_constants;
    std::string entry_symbol = "_start";
};

} // namespace Assembler