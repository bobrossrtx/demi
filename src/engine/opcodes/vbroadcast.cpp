#include "vbroadcast.hpp"
#include "../cpu.hpp"
#include "../../debug/debug_handler.hpp"
#include "../cpu_registers.hpp"
#include <fmt/format.h>

using namespace DemiEngine_Registers;

void handle_VBROADCAST(CPU& cpu, [[maybe_unused]] const std::vector<uint8_t>& program, [[maybe_unused]] bool& running) {
    // VBROADCAST uses fixed registers R0 (src) → R4-R7 (dest) — no operands needed

    // Broadcast value from R0 to R4, R5, R6, R7
    auto& regs = cpu.get_registers(); // Use legacy register system like LOAD_IMM
    
    uint32_t value = regs[static_cast<uint8_t>(R0)];
    
    regs[static_cast<uint8_t>(R4)] = value;
    regs[static_cast<uint8_t>(R5)] = value;
    regs[static_cast<uint8_t>(R6)] = value;
    regs[static_cast<uint8_t>(R7)] = value;
    
    Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION,
        fmt::format("VBROADCAST: Broadcasting {} to R4,R5,R6,R7", value), Logging::DebugLevel::DETAIL);
    
    cpu.set_pc(cpu.get_pc() + 1);
}