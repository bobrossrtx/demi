#pragma once

#include "ir.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace Assembler {

class ELF64ObjectWriter {
public:
    std::vector<uint8_t> write_object(
        const IRProgram& program,
        std::vector<std::string>& errors,
        const std::vector<uint8_t>* text_bytes = nullptr,
        const std::vector<IRRelocation>* text_relocations = nullptr);
};

} // namespace Assembler