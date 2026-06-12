#pragma once

#include "ir.hpp"
#include <cstdint>
#include <vector>
#include <string>

namespace Assembler {

// Convert a relocatable ELF32 .o to a standalone executable
// by adding program headers, resolving relocations, and fixing
// virtual addresses.
std::vector<uint8_t> make_elf32_executable(
    const std::vector<uint8_t>& relocatable,
    const IRProgram& program,
    std::vector<std::string>& errors);

} // namespace Assembler
