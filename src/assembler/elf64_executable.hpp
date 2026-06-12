#pragma once
#include "ir.hpp"
#include <cstdint>
#include <vector>
#include <string>

namespace Assembler {

std::vector<uint8_t> make_elf64_executable(
    const std::vector<uint8_t>& relocatable,
    const IRProgram& program,
    std::vector<std::string>& errors);

} // namespace Assembler
