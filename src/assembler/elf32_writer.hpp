#pragma once
#include "ir.hpp"
#include <cstdint>
#include <vector>
#include <string>

namespace Assembler {

class ELF32ObjectWriter {
public:
    std::vector<uint8_t> write_object(
        const IRProgram& program,
        std::vector<std::string>& errors,
        const std::vector<uint8_t>* text_bytes = nullptr,
        const std::vector<IRRelocation>* text_relocations = nullptr);

    std::vector<uint8_t> write_executable(
        const IRProgram& program,
        std::vector<std::string>& errors,
        const std::vector<uint8_t>* text_bytes = nullptr,
        const std::vector<uint8_t>* data_bytes = nullptr,
        const std::vector<uint8_t>* rodata_bytes = nullptr,
        const std::vector<IRRelocation>* text_relocations = nullptr);
};

} // namespace Assembler
