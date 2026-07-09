#include "shr64.hpp"
#include "../cpu_flags.hpp"
#include "../../debug/debug_handler.hpp"
#include <fmt/core.h>

void handle_shr64(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    if (pc + 2 >= program.size()) { running = false; return; }
    uint8_t reg = program[pc + 1];
    uint8_t shift = program[pc + 2];
    uint64_t value = cpu.get_registers_64()[reg];
    uint64_t result = value >> shift;
    cpu.get_registers_64()[reg] = result;
    if (reg < 8) cpu.get_registers()[reg] = static_cast<uint32_t>(result);
    uint32_t flags = cpu.get_flags();
    flags &= ~(FLAG_ZERO | FLAG_SIGN | FLAG_OVERFLOW | FLAG_CARRY);
    if (result == 0) flags |= FLAG_ZERO;
    if (static_cast<int64_t>(result) < 0) flags |= FLAG_SIGN;
    cpu.set_flags(flags);
    cpu.set_pc(pc + 3);
    cpu.print_state("SHR64");
}
