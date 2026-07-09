#include "or64.hpp"
#include "../cpu_flags.hpp"
#include "../../debug/debug_handler.hpp"
#include <fmt/core.h>

void handle_or64(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    if (pc + 3 >= program.size()) {
        running = false;
        return;
    }
    uint8_t dest = program[pc + 1];
    uint8_t src1 = program[pc + 2];
    uint8_t src2 = program[pc + 3];
    uint64_t result = cpu.get_registers_64()[src1] | cpu.get_registers_64()[src2];
    cpu.get_registers_64()[dest] = result;
    if (dest < 8) cpu.get_registers()[dest] = static_cast<uint32_t>(result);
    uint32_t flags = cpu.get_flags();
    flags &= ~(FLAG_ZERO | FLAG_SIGN | FLAG_OVERFLOW | FLAG_CARRY);
    if (result == 0) flags |= FLAG_ZERO;
    if (static_cast<int64_t>(result) < 0) flags |= FLAG_SIGN;
    cpu.set_flags(flags);
    cpu.set_pc(pc + 4);
    cpu.print_state("OR64");
}
