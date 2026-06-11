#pragma once

#include "backend.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Assembler {

struct EncodedInstructionResult {
    std::vector<uint8_t> bytes;
    std::vector<IRRelocation> relocations;
};

enum class X86BackendMode {
    X86_32,
    X86_64
};

class X86Backend : public AssemblerBackend {
public:
    explicit X86Backend(X86BackendMode mode);

    IRTarget target() const override;
    BackendArtifact emit(const IRProgram& program) override;

private:
    std::optional<uint8_t> encode_register_id(const std::string& name) const;
    size_t estimate_instruction_size(const IRInstruction& instruction, std::vector<std::string>& errors) const;
    EncodedInstructionResult encode_instruction(
        const IRInstruction& instruction,
        uint64_t instruction_offset,
        const std::unordered_map<std::string, uint64_t>& text_symbol_offsets,
        std::vector<std::string>& errors,
        std::vector<std::string>& warnings) const;
    IRProgram adjust_program_for_encoded_text(
        const IRProgram& program,
        const std::unordered_map<uint64_t, uint64_t>& text_offset_map) const;
    bool is_64bit_mode() const;

    X86BackendMode mode_;
};

} // namespace Assembler