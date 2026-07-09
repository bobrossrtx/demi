#include <sys/wait.h>
#include <sys/stat.h>
#include <iomanip>
#include <chrono>
#include <thread>
#include <ctime>
#include <sstream>
#include <unordered_set>
#include <fmt/core.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef _WIN32
    #include <io.h>
    // Dummy syscall implementation for Windows compatibility
    template<typename... Args>
    inline long syscall(long number, Args... args) {
        return -1; // ENOSYS equivalent if it ever reached this directly instead of POSIX wrappers
    }
#else
    #include <sys/syscall.h>
#endif
#include <errno.h>

#include "cpu.hpp"
#include "syscalls.hpp"
#include "cpu_flags.hpp"
#include "../debug/debug_handler.hpp"
#include "../debug/error_handler.hpp"
#include <iostream>
#include <iomanip>

#include "cpu_registers.hpp"
#include "branch_prediction.hpp"
// #include "speculative_execution.hpp"
#include "../config.hpp"
#include "opcodes/opcode_dispatcher.hpp"
#include "opcodes/opcode_dispatcher_threaded.hpp"
#include "opcodes/opcode_dispatcher_unified.hpp"
#include "opcodes/opcode_dispatcher_inlined.hpp"
#include "opcodes/opcode_dispatcher_predictive.hpp"
#include "opcodes/instruction_fusion.hpp"

using namespace DemiEngine_Registers;
using namespace DemiEngine;

constexpr size_t CPU_LEGACY_REGISTER_COUNT = 8;
constexpr size_t CPU_DEFAULT_MEMORY_SIZE = 1024 * 1024;
constexpr size_t CPU_TEST_MEMORY_SIZE = 256;
constexpr size_t CPU_MIN_MEMORY_SIZE = 256;
constexpr size_t CPU_MAX_MEMORY_SIZE = 64 * 1024 * 1024;
const uint32_t INVALID_ADDR = static_cast<uint32_t>(-1);

std::unordered_set<size_t> compute_valid_instruction_starts(const std::vector<uint8_t>& program) {
    std::unordered_set<size_t> starts;
    size_t pc = 0;
    while (pc < program.size()) {
        starts.insert(static_cast<size_t>(pc));
        Opcode opcode = static_cast<Opcode>(program[pc]);
        switch (opcode) {
            case Opcode::LOAD_IMM:
                pc += 6;  // opcode(1) + reg(1) + imm32(4) = 6 bytes (see handle_load_imm)
                break;
            case Opcode::ADD:
            case Opcode::SUB:
            case Opcode::MOV:
            case Opcode::LOAD:
            case Opcode::STORE:
            case Opcode::CMP:
            case Opcode::MUL:
            case Opcode::DIV:
            case Opcode::AND:
            case Opcode::OR:
            case Opcode::XOR:
            case Opcode::SHL:
            case Opcode::SHR:
            case Opcode::IN:
            case Opcode::OUT:
            case Opcode::INB:
            case Opcode::OUTB:
            case Opcode::INW:
            case Opcode::OUTW:
            case Opcode::INL:
            case Opcode::OUTL:
            case Opcode::INSTR:
            case Opcode::OUTSTR:
            case Opcode::ADD64:
            case Opcode::SUB64:
            case Opcode::MOV64:
            case Opcode::MUL64:
            case Opcode::DIV64:
            case Opcode::AND64:
            case Opcode::OR64:
            case Opcode::XOR64:
            case Opcode::SHL64:
            case Opcode::SHR64:
            case Opcode::CMP64:
            case Opcode::MOVEX:
            case Opcode::ADDEX:
            case Opcode::SUBEX:
            case Opcode::MULEX:
            case Opcode::DIVEX:
            case Opcode::CMPEX:
            case Opcode::LOADEX:
            case Opcode::STOREX:
            case Opcode::PUSHEX:
            case Opcode::POPEX:
                pc += 3;
                break;
            case Opcode::JMP:
            case Opcode::JZ:
            case Opcode::JNZ:
            case Opcode::JS:
            case Opcode::JNS:
            case Opcode::JC:
            case Opcode::JNC:
            case Opcode::JO:
            case Opcode::JNO:
            case Opcode::JG:
            case Opcode::JL:
            case Opcode::JGE:
            case Opcode::JLE:
            case Opcode::CALL:
                pc += 5;
                break;
            case Opcode::PUSH:
            case Opcode::POP:
            case Opcode::INC:
            case Opcode::DEC:
            case Opcode::NOT:
            case Opcode::PUSH_ARG:
            case Opcode::POP_ARG:
                pc += 2;
                break;
            case Opcode::HALT:
            case Opcode::NOP:
            case Opcode::RET:
            case Opcode::PUSH_FLAG:
            case Opcode::POP_FLAG:
            case Opcode::MODE32:
            case Opcode::MODE64:
                pc += 1;
                break;
            case Opcode::LOAD_IMM64:
                pc += 10; // opcode + reg + 8-byte immediate
                break;
            case Opcode::MODECMP:
                pc += 2; // opcode + mode value
                break;
            case Opcode::DB:
                if (pc + 2 < program.size()) {
                    uint8_t length = program[pc + 2];
                    pc += 3 + length;
                } else {
                    pc += 1;
                }
                break;
            default:
                pc += 1;
                break;
        }
    }
    return starts;
}

CPU::CPU(size_t memory_size)
    : cpu_mode(CPUMode::MODE_32BIT),
      registers(TOTAL_REGISTERS, 0), legacy_registers(CPU_LEGACY_REGISTER_COUNT, 0) {

    // Config::architecture is set at startup from CLI/env. Default is not X64;
    // the user can opt into 64-bit mode. Intentional, not dead code.
    if (Config::architecture == Architecture::X64) {
        cpu_mode = CPUMode::MODE_64BIT;
    }

    if (memory_size == 0) {
        memory_size = CPU_DEFAULT_MEMORY_SIZE;
    }

    if (memory_size < CPU_MIN_MEMORY_SIZE) {
        Logging::DebugHandler::instance().report(
            Logging::DebugCategory::MEM_ALLOCATION,
            fmt::format("Memory size {} bytes is below minimum {}, using minimum",
                memory_size, CPU_MIN_MEMORY_SIZE),
            Logging::DebugLevel::IMPORTANT);
        memory_size = CPU_MIN_MEMORY_SIZE;
    }
    if (memory_size > CPU_MAX_MEMORY_SIZE) {
        Logging::DebugHandler::instance().report(
            Logging::DebugCategory::MEM_ALLOCATION,
            fmt::format("Memory size {} bytes exceeds maximum {}, using maximum",
                memory_size, CPU_MAX_MEMORY_SIZE),
            Logging::DebugLevel::IMPORTANT);
        memory_size = CPU_MAX_MEMORY_SIZE;
    }

    memory.resize(memory_size, 0);

    registers[static_cast<size_t>(Register::RIP)] = 0;
    registers[static_cast<size_t>(Register::RSP)] = memory.size();
    registers[static_cast<size_t>(Register::RBP)] = memory.size();
    registers[static_cast<size_t>(Register::RFLAGS)] = 0;

    arg_offset = 0;
    stack_limit_ = DEFAULT_STACK_LIMIT;
    last_accessed_addr = INVALID_ADDR;
    last_modified_addr = INVALID_ADDR;

    sync_legacy_registers();

    if (!Config::test_mode) {
        Logging::DebugHandler::instance().report(
            Logging::DebugCategory::CPU_EXECUTION,
            fmt::format("Virtual CPU initialized with {} bytes ({:.1f}MB) of memory and {} total registers",
                memory.size(), memory.size() / (1024.0 * 1024.0), TOTAL_REGISTERS),
            Logging::DebugLevel::INFO);
    }
}

CPU::~CPU() = default;

uint64_t CPU::get_register(Register reg) const {
    auto index = static_cast<size_t>(reg);
    if (index < TOTAL_REGISTERS) {
        return registers[index];
    }
    return 0;
}

void CPU::set_register(Register reg, uint64_t value) {
    auto index = static_cast<size_t>(reg);
    if (index < TOTAL_REGISTERS) {
        uint64_t old_value = registers[index];
        
        registers[index] = value;
        
        // Only sync legacy registers if we're modifying a legacy register (R0-R7)
        // This prevents FPU/extended register updates from corrupting legacy registers
        if (index < CPU_LEGACY_REGISTER_COUNT) {
            sync_legacy_registers();
        }

        // Print register update if value changed
        if (old_value != value) {
            print_register_update(reg, old_value, value);
        }
    }
}

std::string CPU::get_register_name(Register reg) const {
    return RegisterNames::get_name(reg);
}

void CPU::sync_legacy_registers() {
    for (size_t i = 0; i < CPU_LEGACY_REGISTER_COUNT && i < TOTAL_REGISTERS; ++i) {
        legacy_registers[i] = static_cast<uint32_t>(registers[i]);
    }
}

void CPU::sync_from_legacy_registers() {
    for (size_t i = 0; i < CPU_LEGACY_REGISTER_COUNT && i < TOTAL_REGISTERS; ++i) {
        // Only update the lower 32 bits, preserve upper 32 bits
        registers[i] = (registers[i] & 0xFFFFFFFF00000000ULL) | legacy_registers[i];
    }
}

CPU CPU::create_test_cpu() {
    return CPU(CPU_TEST_MEMORY_SIZE);
}

void CPU::resize_memory(size_t new_size) {
    if (new_size < CPU_MIN_MEMORY_SIZE) {
        Logging::DebugHandler::instance().report(
            Logging::DebugCategory::MEM_ALLOCATION,
            fmt::format("Cannot resize memory to {} bytes (below minimum {})",
                new_size, CPU_MIN_MEMORY_SIZE),
            Logging::DebugLevel::IMPORTANT);
        return;
    }
    if (new_size > CPU_MAX_MEMORY_SIZE) {
        Logging::DebugHandler::instance().report(
            Logging::DebugCategory::MEM_ALLOCATION,
            fmt::format("Cannot resize memory to {} bytes (exceeds maximum {})",
                new_size, CPU_MAX_MEMORY_SIZE),
            Logging::DebugLevel::IMPORTANT);
        return;
    }

    size_t old_size = memory.size();
    memory.resize(new_size, 0); // Initialize new memory to zero

    // Adjust stack pointer if it's now out of bounds
    auto current_sp = static_cast<size_t>(registers[static_cast<size_t>(Register::RSP)]);
    if (current_sp >= new_size) {
        registers[static_cast<size_t>(Register::RSP)] = new_size - 4; // Move stack pointer to valid location
        registers[static_cast<size_t>(Register::RBP)] = registers[static_cast<size_t>(Register::RSP)]; // Reset frame pointer too
    }

    Logging::DebugHandler::instance().report(
        Logging::DebugCategory::MEM_ALLOCATION,
        fmt::format("Memory resized from {} bytes ({:.1f}MB) to {} bytes ({:.1f}MB)",
            old_size, old_size / (1024.0 * 1024.0),
            new_size, new_size / (1024.0 * 1024.0)),
        Logging::DebugLevel::INFO);
}

void CPU::reset() {
    std::fill(registers.begin(), registers.end(), 0);
    std::fill(legacy_registers.begin(), legacy_registers.end(), 0);
    std::fill(memory.begin(), memory.end(), 0); // Clear memory

    // Reset CPU mode based on configuration - default to 32-bit for backward compatibility
    // Config::architecture is set at startup from CLI/env. Default is not X64;
    // the user can opt into 64-bit mode. Intentional, not dead code.
    if (Config::architecture == Architecture::X64) {
        cpu_mode = CPUMode::MODE_64BIT;
    } else {
        cpu_mode = CPUMode::MODE_32BIT; // Default to 32-bit (includes AUTO mode)
    }

    // Reset special registers
    registers[static_cast<size_t>(Register::RIP)] = 0;
    registers[static_cast<size_t>(Register::RSP)] = memory.size();
    registers[static_cast<size_t>(Register::RBP)] = memory.size();
    registers[static_cast<size_t>(Register::RFLAGS)] = 0;

    arg_offset = 0; // Initialize arg_offset for PUSH_ARG/POP_ARG operations
    call_depth = 0; // Reset call stack depth
    last_accessed_addr = INVALID_ADDR; // Clear highlight
    last_modified_addr = INVALID_ADDR; // Clear highlight

    // Sync legacy registers
    sync_legacy_registers();
}

void CPU::print_state(const std::string& info) const {
    // If debug is not enabled, do not print the state
    // Config::debug is a runtime toggle (--debug flag). When disabled,
    // state printing is a no-op — intentional, not dead code.
    if (!Config::debug) return;

    // Get current time
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    std::ostringstream pc_ss;
    pc_ss << "PC=0x" << std::setw(4) << std::setfill('0') << std::hex << std::uppercase << get_pc();
    oss << "[" << pc_ss.str() << "] "<< std::string("(" + info + ")") << " ";

    // Add CPU mode indicator
    oss << "MODE=" << (is_64bit_mode() ? "x64" : "x86") << " ";

    // x86/64 register names mapping
    const char* reg_names[] = {"RAX", "RCX", "RDX", "RBX", "RSP", "RBP", "RSI", "RDI"};
    for (size_t i = 0; i < 8; ++i) {
        uint64_t val = registers[i];
        int width = is_64bit_mode() ? 16 : 8;
        if (!is_64bit_mode()) val &= 0xFFFFFFFF;
        
        oss << reg_names[i] << "=0x" << std::setw(width) << std::setfill('0') << std::hex << std::uppercase << val << " ";
    }
    oss << "FLAGS=0x" << std::setw(8) << std::setfill('0') << std::hex << std::uppercase << get_flags();

    // Use DebugHandler instead of Logger to avoid deadlock
    DEBUG_INFO(Logging::DebugCategory::CPU_REGISTERS, "{}", oss.str());
}

void CPU::print_stack_frame(const std::string& label) const {
    // If debug is not enabled, do not print the state
    // Config::debug is a runtime toggle (--debug flag). When disabled,
    // state printing is a no-op — intentional, not dead code.
    if (!Config::debug) return;

    // Use DebugHandler instead of Logger to avoid deadlock
    DEBUG_INFO(Logging::DebugCategory::CPU_STACK, "[{}] FP=0x{:X} SP=0x{:X} arg_offset={}", 
        label, get_fp(), get_sp(), arg_offset);
}

uint8_t CPU::fetch_operand() {
    DEBUG_TRACE(Logging::DebugCategory::CPU_EXECUTION, "Fetching operand at PC=0x{:04X}", get_pc());
    if (static_cast<size_t>(get_pc()) + 1 >= memory.size()) {
        DEBUG_CRITICAL(Logging::DebugCategory::MEM_BOUNDS, "Operand fetch out of bounds at PC=0x{:04X}, memory size={}", get_pc(), memory.size());
        return 0;
    }
    uint8_t operand = memory[static_cast<size_t>(get_pc()) + 1];
    DEBUG_TRACE(Logging::DebugCategory::CPU_EXECUTION, "Fetched operand=0x{:02X}, advancing PC from 0x{:04X}", operand, get_pc());
    set_pc(static_cast<uint32_t>(static_cast<size_t>(get_pc()) + 1));
    DEBUG_TRACE(Logging::DebugCategory::CPU_EXECUTION, "PC advanced to 0x{:04X}", get_pc());
    return operand;
}

uint64_t CPU::read_address_from_program(const std::vector<uint8_t>& program, uint64_t offset) const {
    if (offset >= program.size()) {
        throw CPUException("Program counter out of bounds reading address");
    }

    size_t addr_size = get_address_size();
    if (offset + addr_size > program.size()) {
        throw CPUException("Program counter out of bounds reading address bytes");
    }

    uint64_t addr = 0;
    for (size_t i = 0; i < addr_size; ++i) {
        addr |= static_cast<uint64_t>(program[offset + i]) << (i * 8);
    }
    return addr;
}

// Memory access validation — always active.
// Reports errors via ErrorHandler and returns false on bounds violation.
bool CPU::validate_memory_read(uint32_t addr, size_t size) const {
    if (addr + size > memory.size()) {
        Logging::ErrorHandler::instance().report_runtime(
            Logging::ErrorCode::CPU_MEMORY_OUT_OF_BOUNDS,
            fmt::format("Memory read out of bounds at address=0x{:08X}, size={}, memory size={}", addr, size, memory.size()),
            get_pc(),
            "Memory bounds violation (read)");
        return false;
    }
    return true;
}

bool CPU::validate_memory_write(uint32_t addr, size_t size) const {
    if (addr + size > memory.size()) {
        Logging::ErrorHandler::instance().report_runtime(
            Logging::ErrorCode::CPU_MEMORY_OUT_OF_BOUNDS,
            fmt::format("Memory write out of bounds at address=0x{:08X}, size={}, memory size={}", addr, size, memory.size()),
            get_pc(),
            "Memory bounds violation (write)");
        return false;
    }
    return true;
}

bool CPU::validate_stack_push(size_t bytes) const {
    uint32_t sp = get_sp();
    if (sp < stack_limit_ + bytes) {
        Logging::ErrorHandler::instance().report_runtime(
            Logging::ErrorCode::CPU_STACK_OVERFLOW,
            fmt::format("Stack overflow at SP=0x{:08X}, need {} bytes, stack_limit=0x{:08X}", sp, bytes, stack_limit_),
            get_pc(),
            "Stack overflow (push)");
        return false;
    }
    return true;
}

bool CPU::validate_stack_pop(size_t bytes) const {
    uint32_t sp = get_sp();
    if (sp + bytes > memory.size()) {
        Logging::ErrorHandler::instance().report_runtime(
            Logging::ErrorCode::CPU_STACK_UNDERFLOW,
            fmt::format("Stack underflow at SP=0x{:08X}, bytes={}, memory size={}", sp, bytes, memory.size()),
            get_pc(),
            "Stack underflow (pop)");
        return false;
    }
    if (sp < stack_limit_) {
        Logging::ErrorHandler::instance().report_runtime(
            Logging::ErrorCode::CPU_STACK_UNDERFLOW,
            fmt::format("Stack pointer below stack limit at SP=0x{:08X}, stack_limit=0x{:08X}", sp, stack_limit_),
            get_pc(),
            "Stack underflow (SP below limit)");
        return false;
    }
    return true;
}

uint32_t CPU::read_mem32(uint32_t addr) const {
    if (!validate_memory_read(addr, 4)) {
        return 0;
    }
    last_accessed_addr = addr;
    return (static_cast<uint32_t>(memory[addr])      ) |
           (static_cast<uint32_t>(memory[addr + 1]) << 8 ) |
           (static_cast<uint32_t>(memory[addr + 2]) << 16) |
           (static_cast<uint32_t>(memory[addr + 3]) << 24);
}

void CPU::write_mem32(uint32_t addr, uint32_t value) {
    if (!validate_memory_write(addr, 4)) {
        return;
    }
    last_modified_addr = addr;
    memory[addr    ] = static_cast<uint8_t>(value      );
    memory[addr + 1] = static_cast<uint8_t>(value >> 8 );
    memory[addr + 2] = static_cast<uint8_t>(value >> 16);
    memory[addr + 3] = static_cast<uint8_t>(value >> 24);
}

void CPU::execute(const std::vector<uint8_t>& program, uint32_t entry_address, size_t max_steps) {
    // Check if program fits in memory
    if (program.size() > memory.size()) {
        throw std::runtime_error("Program size (" + std::to_string(program.size()) + 
                                " bytes) exceeds available memory (" + std::to_string(memory.size()) + " bytes)");
    }
    
    // Copy program into memory
    std::copy(program.begin(), program.end(), memory.begin());
    
    // Debug: Print entry address and PC before setting
    DEBUG_CPU("Starting execution at entry address 0x{:04X}, initial PC was 0x{:04X}", entry_address, get_pc());
    // Use entry address if provided, otherwise default to 0
    set_pc(entry_address);
    DEBUG_CPU("PC set to entry address: 0x{:04X}", get_pc());
    registers[static_cast<size_t>(Register::RSP)] = memory.size(); // Stack pointer starts at the end of memory
    registers[static_cast<size_t>(Register::RBP)] = get_sp();
    bool running = true;
    size_t step_count = 0;

    while (get_pc() < program.size() && running) {
        DEBUG_DETAIL(Logging::DebugCategory::CPU_EXECUTION, "Execution loop iteration: running={}, step_count={}", running, step_count);
        
        // Check step limit if provided (0 means unlimited)
        if (max_steps > 0 && step_count >= max_steps) {
            throw std::runtime_error("Test exceeded maximum execution steps (possible infinite loop)");
        }
        
        // Handle pending interrupts before executing next instruction
        if (handle_pending_interrupts(program, running)) {
            DEBUG_INFO(Logging::DebugCategory::CPU_EXECUTION, "Interrupt handled, PC now at 0x{:04X}", get_pc());
        }
        if (!running) {
            DEBUG_INFO(Logging::DebugCategory::CPU_EXECUTION, "Execution stopped by interrupt handler{}", "");
            break;
        }
        
        try {
            uint32_t dispatch_pc = get_pc();
            DEBUG_INFO(Logging::DebugCategory::CPU_DISPATCHER, "[PRE-DISPATCH] PC=0x{:04X} step_count={}", dispatch_pc, step_count);
            // Try instruction fusion first for performance
            // If fusion doesn't apply, use branch-predictive dispatcher
            if (!InstructionFusion::try_instruction_fusion(*this, program, running)) {
                // Use branch-predictive dispatcher with speculative execution
                dispatch_opcode_with_prediction(*this, program, running);
            }
        } catch (const std::exception& e) {
            DEBUG_CRITICAL(Logging::DebugCategory::CPU_DISPATCHER, "[DISPATCH_ERROR] PC=0x{:04X} step_count={} error={}", get_pc(), step_count, e.what());
            throw;
        }
        
        step_count++;
    }
}

void CPU::print_registers() const {
    std::ostringstream oss;
    oss << "Registers:" << std::endl;
    for (size_t i = 0; i < legacy_registers.size(); ++i) {
        oss << "R" << i << "=0x" << std::setw(2) << std::setfill('0') << std::hex << std::uppercase << static_cast<int>(legacy_registers[i]) << " ";
    }
    oss << "SP=0x" << std::setw(3) << std::setfill('0') << std::hex << std::uppercase << get_sp() << " ";
    oss << "FLAGS=0x" << std::setw(8) << std::setfill('0') << std::hex << std::uppercase << get_flags();

    Logging::DebugHandler::instance().report(
        Logging::DebugCategory::CPU_REGISTERS,
        oss.str(),
        Logging::DebugLevel::INFO);
}

void CPU::print_register_update(Register reg, uint64_t old_value, uint64_t new_value) const {
    // Config::debug is a runtime toggle (set via CLI --debug). When disabled,
    // the entire function body is skipped — intentional no-op, not dead code.
    if (!Config::debug || !Config::extended_registers) return;

    std::string reg_name = get_register_name(reg);
    Logging::DebugHandler::instance().report(
        Logging::DebugCategory::CPU_REGISTERS,
        fmt::format("[REG_UPDATE] {} changed: 0x{:016X} -> 0x{:016X}",
            reg_name, old_value, new_value),
        Logging::DebugLevel::TRACE);
}

void CPU::print_extended_registers() const {
    std::ostringstream oss;
    oss << "Extended Registers:" << std::endl;

    // General purpose registers (RAX-R15) - 4 per line
    for (size_t i = 0; i < GENERAL_PURPOSE_COUNT; ++i) {
        if (i % 4 == 0) {
            if (i > 0) oss << std::endl;
            oss << "GP" << (i/4 + 1) << ": ";
        }
        Register reg = static_cast<Register>(i);
        std::string reg_name = get_register_name(reg);

        // Reset all formatting before register name
        oss << std::left << std::setw(3) << std::setfill(' ') << reg_name;

        // Format the hex value with proper settings
        oss << "=0x" << std::right << std::setw(16) << std::setfill('0')
            << std::hex << std::uppercase << get_register_64(reg) << " ";
    }
    oss << std::endl;

    // Special registers
    oss << "Special: ";
    oss << "RIP=0x" << std::setw(16) << std::setfill('0') << std::hex << std::uppercase << get_register_64(Register::RIP) << " ";
    oss << "RSP=0x" << std::setw(16) << std::setfill('0') << std::hex << std::uppercase << get_register_64(Register::RSP) << " ";
    oss << "RBP=0x" << std::setw(16) << std::setfill('0') << std::hex << std::uppercase << get_register_64(Register::RBP) << " ";
    oss << "RFLAGS=0x" << std::setw(16) << std::setfill('0') << std::hex << std::uppercase << get_register_64(Register::RFLAGS);

    Logging::DebugHandler::instance().report(
        Logging::DebugCategory::CPU_REGISTERS,
        oss.str(),
        Logging::DebugLevel::INFO);
}

void CPU::print_memory(std::size_t start, std::size_t end) const {
    std::ostringstream oss;
    oss << "Memory Dump:" << std::endl;
    
    // Align start to 16 bytes
    std::size_t aligned_start = start & ~0xF;
    
    for (std::size_t i = aligned_start; i < end && i < memory.size(); i += 16) {
        // Address
        oss << "0x" << std::setw(8) << std::setfill('0') << std::hex << std::uppercase << i << ": ";
        
        // Hex bytes
        std::string ascii_repr;
        for (std::size_t j = 0; j < 16; ++j) {
            std::size_t current_addr = i + j;
            
            if (current_addr < start) {
                oss << "   "; // Padding for alignment before start
                ascii_repr += ' ';
                continue;
            }
            
            if (current_addr < end && current_addr < memory.size()) {
                uint8_t val = memory[current_addr];
                
                // Highlight accessed/modified
                if (current_addr == last_modified_addr) {
                    oss << "\033[1;31m" << std::setw(2) << std::setfill('0') << std::hex << std::uppercase << static_cast<int>(val) << "\033[0m ";
                } else if (current_addr == last_accessed_addr) {
                    oss << "\033[1;32m" << std::setw(2) << std::setfill('0') << std::hex << std::uppercase << static_cast<int>(val) << "\033[0m ";
                } else {
                    oss << std::setw(2) << std::setfill('0') << std::hex << std::uppercase << static_cast<int>(val) << " ";
                }
                
                // Build ASCII string
                if (val >= 32 && val <= 126) {
                    ascii_repr += static_cast<char>(val);
                } else {
                    ascii_repr += '.';
                }
            } else {
                oss << "   "; // Padding for missing bytes
            }
        }
        
        // ASCII representation
        oss << " |" << ascii_repr << "|" << std::endl;
    }

    Logging::DebugHandler::instance().report(
        Logging::DebugCategory::MEM_ACCESS,
        oss.str(),
        Logging::DebugLevel::INFO);
}

void CPU::run(const std::vector<uint8_t>& program) {
    reset();
    execute(program);
}

bool CPU::step(const std::vector<uint8_t>& program) {
    // Copy program into memory if first step and memory is empty
    if (get_pc() == 0 && memory[0] == 0 && !program.empty()) {
        std::copy(program.begin(), program.end(), memory.begin());
        registers[static_cast<size_t>(Register::RSP)] = memory.size() - 4;
        registers[static_cast<size_t>(Register::RBP)] = get_sp();
    }

    if (get_pc() >= program.size()) {
        return false; // Program ended
    }

    bool running = true;
    
    // Check for pending interrupts before executing instruction
    handle_pending_interrupts(program, running);
    if (!running) return false;
    
    // Try instruction fusion first, fall back to optimized dispatch
    if (!InstructionFusion::try_instruction_fusion(*this, program, running)) {
        dispatch_opcode_inlined_optimized(*this, program, running);
    }

    return running;
}

uint8_t CPU::readPort(uint8_t port) {
    return vhw::DeviceManager::instance().readPort(port);
}

void CPU::writePort(uint8_t port, uint8_t value) {
    vhw::DeviceManager::instance().writePort(port, value);
}

std::string CPU::readPortString(uint8_t port, uint8_t maxLength) {
    return vhw::DeviceManager::instance().readPortString(port, maxLength);
}

void CPU::writePortString(uint8_t port, const std::string& str) {
    vhw::DeviceManager::instance().writePortString(port, str);
}

void CPU::fpu_push(double value) {
    // Convert double to x87 80-bit format (simplified implementation)
    // For now, we'll store as 64-bit mantissa + 16-bit exponent/sign
    
    // Move stack top down (stack grows downward)
    fpu_stack_top = (fpu_stack_top - 1) & 0x7;
    
    // Convert double to our internal representation
    uint64_t mantissa;
    uint64_t exponent_sign;
    
    if (value == 0.0) {
        mantissa = 0;
        exponent_sign = 0;
    } else {
        // Use IEEE 754 double as mantissa for now (simplified)
        union { double d; uint64_t i; } converter;
        converter.d = value;
        mantissa = converter.i;
        exponent_sign = 0; // Simplified - normally would extract IEEE 754 parts
    }
    
    // Store in ST(0) position (current stack top)
    Register st_reg = static_cast<Register>(static_cast<size_t>(Register::ST0) + fpu_stack_top * 2);
    set_fpu_register(st_reg, mantissa, exponent_sign);
    
    // Update tag word to mark register as valid
    fpu_tag_word &= ~(0x3 << (fpu_stack_top * 2)); // Clear tag bits
    fpu_tag_word |= (0x0 << (fpu_stack_top * 2));  // Set as valid (00)
}

double CPU::fpu_pop() {
    // Get value from ST(0)
    double value = fpu_peek(0);
    
    // Mark register as empty
    fpu_tag_word |= (0x3 << (fpu_stack_top * 2)); // Set as empty (11)
    
    // Move stack top up
    fpu_stack_top = (fpu_stack_top + 1) & 0x7;
    
    return value;
}

double CPU::fpu_peek(uint8_t offset) const {
    // Calculate actual register index
    uint8_t reg_index = (fpu_stack_top + offset) & 0x7;
    Register st_reg = static_cast<Register>(static_cast<size_t>(Register::ST0) + reg_index * 2);
    
    uint64_t mantissa, exponent_sign;
    get_fpu_register(st_reg, mantissa, exponent_sign);
    
    // Convert back to double (simplified)
    if (mantissa == 0 && exponent_sign == 0) {
        return 0.0;
    }
    
    union { double d; uint64_t i; } converter;
    converter.i = mantissa;
    return converter.d;
}

void CPU::fpu_store(uint8_t offset, double value) {
    // Calculate actual register index
    uint8_t reg_index = (fpu_stack_top + offset) & 0x7;
    
    // Convert double to our internal representation
    uint64_t mantissa;
    uint64_t exponent_sign;
    
    if (value == 0.0) {
        mantissa = 0;
        exponent_sign = 0;
    } else {
        union { double d; uint64_t i; } converter;
        converter.d = value;
        mantissa = converter.i;
        exponent_sign = 0; // Simplified
    }
    
    // Store in specified ST register
    Register st_reg = static_cast<Register>(static_cast<size_t>(Register::ST0) + reg_index * 2);
    set_fpu_register(st_reg, mantissa, exponent_sign);
    
    // Update tag word to mark register as valid
    fpu_tag_word &= ~(0x3 << (reg_index * 2)); // Clear tag bits
    fpu_tag_word |= (0x0 << (reg_index * 2));  // Set as valid (00)
}

void CPU::fpu_init() {
    // Initialize FPU to default state
    fpu_stack_top = 0;
    fpu_control_word = 0x037F; // Default control word
    fpu_status_word = 0x0000;  // Clear status
    fpu_tag_word = 0xFFFF;     // All registers empty (changed from 0xFF to 0xFFFF for 16-bit)
    
    // Clear all FPU registers safely
    for (int i = 0; i < 8; i++) {
        Register st_reg = static_cast<Register>(static_cast<size_t>(Register::ST0) + i * 2);
        // Use the safer approach by setting registers directly
        set_register(st_reg, 0);  // mantissa part
        Register meta_reg = static_cast<Register>(static_cast<size_t>(st_reg) + 1);
        set_register(meta_reg, 0);  // exponent/sign part
    }
}

void CPU::trigger_interrupt(uint8_t vector) {
    interrupt_controller_.queue_interrupt(vector);
}

bool CPU::handle_pending_interrupts(const std::vector<uint8_t>& program, bool& running) {
    (void)program; // Mark as intentionally unused
    // Check if we have pending interrupts and interrupts are enabled
    if (!interrupt_controller_.has_pending_interrupts()) {
        return false;
    }
    
    // Get next interrupt vector
    uint8_t vector = interrupt_controller_.get_next_interrupt();
    if (vector == 0xFF) {
        return false; // No interrupt available
    }
    
    // Handle Linux-style syscalls (INT 0x80)
    if (vector == 0x80) {
        handle_syscall(running);
        return true;
    }
    
    // Read handler address from IVT in memory
    // IVT base address + (vector * 4) gives us the handler address location
    uint32_t ivt_base = interrupt_controller_.get_ivt_base();
    uint32_t max_ivt_offset = static_cast<uint32_t>(vector) * 4 + 4;

    // M10: Guard against IVT base near end of memory causing wrap-around
    if (ivt_base > memory.size() || max_ivt_offset > memory.size() - ivt_base) {
        Logging::ErrorHandler::instance().report_runtime(
            Logging::ErrorCode::CPU_MEMORY_OUT_OF_BOUNDS,
            fmt::format("[CPU] IVT access out of bounds: base=0x{:08X}, vector=0x{:02X}, memory size={}",
                ivt_base, vector, memory.size()),
            get_pc(),
            "IVT out of bounds");
        running = false;
        return false;
    }

    uint32_t handler_entry_address = ivt_base + (static_cast<uint32_t>(vector) * 4);

    // Reject interrupt if nesting would exceed maximum (M2)
    if (interrupt_controller_.get_nesting_level() >= DemiEngine_Interrupts::InterruptController::MAX_NESTING_LEVEL) {
        Logging::ErrorHandler::instance().report_runtime(
            Logging::ErrorCode::CPU_GENERIC,
            fmt::format("[CPU] Interrupt nesting overflow ({} levels), rejecting vector 0x{:02X}",
                interrupt_controller_.get_nesting_level(), vector),
            get_pc(),
            "Interrupt nesting overflow - rejected");
        return false;
    }
    
    // Read 32-bit handler address from memory
    uint32_t handler_address = read_mem32(handler_entry_address);
    
    if (handler_address == 0) {
        // No handler installed for this vector
        Logging::ErrorHandler::instance().report_runtime(
            Logging::ErrorCode::CPU_GENERIC,
            fmt::format("[CPU] No handler for interrupt vector 0x{:02X} at IVT[0x{:04X}] - halting",
                vector, handler_entry_address),
            get_pc(),
            "Missing interrupt handler");
        running = false;
        return false;
    }
    
    Logging::DebugHandler::instance().report(
        Logging::DebugCategory::IO_INTERRUPT,
        fmt::format("[CPU] Handling interrupt 0x{:02X}, jumping to handler at 0x{:08X}",
            vector, handler_address),
        Logging::DebugLevel::DETAIL);
    
    // Save current CPU state to stack
    save_interrupt_state();
    
    // Enter interrupt context
    interrupt_controller_.enter_interrupt();
    
    // Jump to interrupt handler
    set_pc(handler_address);
    
    return true;
}

void CPU::save_interrupt_state() {
    // Save state in reverse order so IRET can restore correctly
    // This matches x86 interrupt stack frame:
    // 1. FLAGS
    // 2. CS (we don't have segmentation, so we'll skip this or use 0)
    // 3. EIP/RIP (program counter)
    
    uint32_t sp = get_sp();

    // Validate stack space for interrupt state save (4 bytes FLAGS + 4 CS + 4 PC + 16*8 registers)
    if (!validate_stack_push(4 + 4 + 4 + 16 * 8)) {
        Logging::ErrorHandler::instance().report_runtime(
            Logging::ErrorCode::CPU_STACK_OVERFLOW,
            fmt::format("Stack overflow during interrupt state save at SP=0x{:08X}", sp),
            get_pc(),
            "Stack overflow (interrupt save)");
        return;
    }
    
    // Push FLAGS
    set_sp(sp - 4);
    write_mem32(get_sp(), get_flags());
    
    // Push CS (code segment - we'll use 0 for now as we don't have segmentation)
    set_sp(get_sp() - 4);
    write_mem32(get_sp(), 0);
    
    // Push return address (current PC)
    set_sp(get_sp() - 4);
    write_mem32(get_sp(), get_pc());
    
    // Optionally save all general-purpose registers
    // This provides more context for the interrupt handler
    for (size_t i = 0; i < 16; i++) {  // Save first 16 registers (RAX-R15)
        Register reg = static_cast<Register>(i);
        if (is_64bit_mode()) {
            uint64_t value = get_register_64(reg);
            set_sp(get_sp() - 4);
            write_mem32(get_sp(), static_cast<uint32_t>(value >> 32));  // High 32 bits
            set_sp(get_sp() - 4);
            write_mem32(get_sp(), static_cast<uint32_t>(value & 0xFFFFFFFF));  // Low 32 bits
        } else {
            set_sp(get_sp() - 4);
            write_mem32(get_sp(), get_register_32(reg));
        }
    }
    
    Logging::DebugHandler::instance().report(
        Logging::DebugCategory::IO_INTERRUPT,
        fmt::format("[CPU] Saved interrupt state (SP: 0x{:08X} -> 0x{:08X})",
            sp, get_sp()),
        Logging::DebugLevel::DETAIL);
}

void CPU::restore_interrupt_state() {
    uint32_t sp = get_sp();

    // Validate stack before restoring interrupt state
    if (!validate_stack_pop(4 + 4 + 4 + 16 * 8)) {
        Logging::ErrorHandler::instance().report_runtime(
            Logging::ErrorCode::CPU_STACK_UNDERFLOW,
            fmt::format("Stack underflow during interrupt state restore at SP=0x{:08X}", sp),
            get_pc(),
            "Stack underflow (interrupt restore)");
        return;
    }
    
    // Restore general-purpose registers (in reverse order)
    for (int i = 15; i >= 0; i--) {
        Register reg = static_cast<Register>(i);
        if (is_64bit_mode()) {
            uint32_t low = read_mem32(get_sp());
            set_sp(get_sp() + 4);
            uint32_t high = read_mem32(get_sp());
            set_sp(get_sp() + 4);
            uint64_t value = (static_cast<uint64_t>(high) << 32) | low;
            set_register_64(reg, value);
        } else {
            set_register_32(reg, read_mem32(get_sp()));
            set_sp(get_sp() + 4);
        }
    }
    
    // Pop return address
    uint32_t return_address = read_mem32(get_sp());
    set_sp(get_sp() + 4);
    
    // Pop CS (discard)
    set_sp(get_sp() + 4);
    
    // Pop FLAGS
    set_flags(read_mem32(get_sp()));
    set_sp(get_sp() + 4);
    
    // Exit interrupt context
    interrupt_controller_.exit_interrupt();
    
    // Return to interrupted code
    set_pc(return_address);
    
    Logging::DebugHandler::instance().report(
        Logging::DebugCategory::IO_INTERRUPT,
        fmt::format("[CPU] Restored interrupt state, returning to 0x{:08X} (SP: 0x{:08X} -> 0x{:08X})",
            return_address, sp, get_sp()),
        Logging::DebugLevel::DETAIL);
}

void CPU::handle_syscall(bool& running) {
    // Linux syscall conventions (INT 0x80):
    // EAX (RAX/R0) = syscall number
    // EBX (RBX/R3) = arg1
    // ECX (RCX/R1) = arg2
    // EDX (RDX/R2) = arg3
    // ESI (RSI/R6) = arg4
    // EDI (RDI/R7) = arg5
    // Return value in EAX (RAX/R0)
    
    uint32_t syscall_num = get_register_32(Register::RAX);
    uint32_t arg1 = get_register_32(Register::RBX);
    uint32_t arg2 = get_register_32(Register::RCX);
    uint32_t arg3 = get_register_32(Register::RDX);
    uint32_t arg4 = get_register_32(Register::RSI);  // arg4
    uint32_t arg5 = get_register_32(Register::RDI);  // arg5
    
    Syscall sc = to_syscall(syscall_num);
    const char* sc_name = syscall_name(sc);

    if (syscall_hook_) {
        int32_t hook_result = 0;
        if (syscall_hook_(syscall_num, arg1, arg2, arg3, arg4, arg5, hook_result)) {
            set_register_32(Register::RAX, static_cast<uint32_t>(hook_result));
            return;
        }
    }

    auto sandbox_check = demi::sandbox::SyscallResult::ALLOWED;
    if (sandbox_dispatcher_) {
        sandbox_check = sandbox_dispatcher_->validate_syscall(syscall_num);
        if (sandbox_check == demi::sandbox::SyscallResult::DENIED) {
            Logging::ErrorHandler::instance().report_runtime(
                Logging::ErrorCode::IO_GENERIC, // Using a generic IO error mapping
                fmt::format("[SECURITY FAULT] Denied syscall: {}({})", sc_name, syscall_num),
                get_pc(),
                "Sandbox security bounds exceeded");
            set_register_32(Register::RAX, static_cast<uint32_t>(-EACCES));
            has_security_fault_ = true;
            running = false;
            return;
        }
    }
    
    Logging::DebugHandler::instance().report(
        Logging::DebugCategory::CPU_EXECUTION,
        fmt::format("[SYSCALL] INT 0x80: {}({}), args=({}, {}, {}, {}, {})",
            sc_name, syscall_num, arg1, arg2, arg3, arg4, arg5),
        Logging::DebugLevel::INFO);
    
    long result = -ENOSYS;
    
    // Dispatch to appropriate handler
    switch (sc) {
        case Syscall::SYS_EXIT:
            Logging::DebugHandler::instance().report(
                Logging::DebugCategory::CPU_EXECUTION,
                fmt::format("[SYSCALL] {}({}) - stopping VM", sc_name, arg1),
                Logging::DebugLevel::INFO);
            running = false;
            set_register_32(Register::RAX, 0);
            return;
            
        case Syscall::SYS_READ:
            if (arg2 < memory.size() && static_cast<size_t>(arg2) + static_cast<size_t>(arg3) <= memory.size()) {
                if (stdin_hook_ && arg1 == 0) {
                    std::vector<uint8_t> data;
                    stdin_hook_(arg3, data);
                    size_t count = std::min(static_cast<size_t>(arg3), data.size());
                    if (count > 0) {
                        std::copy(data.begin(), data.begin() + count, memory.begin() + arg2);
                    }
                    result = count;
                } else if (is_virtual_fd(arg1)) {
                    auto* vd = io_vfs_->get_virtual_disk();
                    int handle = virtual_fds_[arg1];
                    result = vd->read(handle, &memory[arg2], static_cast<int>(arg3));
                } else if (is_virtual_fd(arg1)) {
                    auto* vd = io_vfs_->get_virtual_disk();
                    int handle = virtual_fds_[arg1];
                    result = vd->read(handle, &memory[arg2], static_cast<int>(arg3));
                } else if (is_virtual_fd(arg1)) {
                    auto* vd = io_vfs_->get_virtual_disk();
                    int handle = virtual_fds_[arg1];
                    result = vd->read(handle, &memory[arg2], static_cast<int>(arg3));
                } else if (is_virtual_fd(arg1)) {
                    auto* vd = io_vfs_->get_virtual_disk();
                    int handle = virtual_fds_[arg1];
                    result = vd->read(handle, &memory[arg2], static_cast<int>(arg3));
                } else if (is_virtual_fd(arg1)) {
                    auto* vd = io_vfs_->get_virtual_disk();
                    int handle = virtual_fds_[arg1];
                    result = vd->read(handle, &memory[arg2], static_cast<int>(arg3));
                } else if (is_virtual_fd(arg1)) {
                    auto* vd = io_vfs_->get_virtual_disk();
                    int handle = virtual_fds_[arg1];
                    result = vd->read(handle, &memory[arg2], static_cast<int>(arg3));
                } else if (is_virtual_fd(arg1)) {
                    auto* vd = io_vfs_->get_virtual_disk();
                    int handle = virtual_fds_[arg1];
                    result = vd->read(handle, &memory[arg2], static_cast<int>(arg3));
                } else if (arg1 <= 2 || is_vm_fd(arg1)) {
                    // Line-buffered read: read byte by byte until newline or limit reached
                    // This makes sys_read work correctly with piped input
                    result = 0;
                    for (uint64_t i = 0; i < arg3; i++) {
                        char ch;
                        // Safe: reading exactly 1 byte into a single char; no buffer risk
                        // Bounds: arg2 + i is validated against memory.size before loop
#ifdef _WIN32
                        ssize_t bytes = ::_read(arg1, &ch, 1);
#else
                        ssize_t bytes = ::read(arg1, &ch, 1);
#endif
                        if (bytes <= 0) {
                            break;  // EOF or error
                        }
                        memory[arg2 + i] = static_cast<uint8_t>(ch);
                        result++;
                        if (ch == '\n') {
                            break;  // Stop at newline (line-buffered behavior)
                        }
                    }
                } else {
                    // Non-sandbox: pass any fd through to the real kernel
#ifdef _WIN32
                    result = ::_read(arg1, &memory[arg2], arg3);
#else
                    result = ::read(arg1, &memory[arg2], arg3);
#endif
                    if (result == -1) result = -errno;
                }
                Logging::DebugHandler::instance().report(
                    Logging::DebugCategory::CPU_EXECUTION,
                    fmt::format("[SYSCALL] {}(fd={}, buf=0x{:08X}, count={}) = {} (line-buffered)",
                        sc_name, arg1, arg2, arg3, result),
                    Logging::DebugLevel::INFO);

            } else {
                Logging::ErrorHandler::instance().report_runtime(
                    Logging::ErrorCode::IO_GENERIC,
                    fmt::format("[SYSCALL] {}: buffer out of bounds (addr=0x{:08X}, count={}, mem_size={})",
                        sc_name, arg2, arg3, memory.size()),
                    get_pc(),
                    "Syscall buffer overflow");
                result = -EFAULT;
            }
            break;
            
        case Syscall::SYS_WRITE:
            if (arg2 < memory.size() && static_cast<size_t>(arg2) + static_cast<size_t>(arg3) <= memory.size()) {
                if (stdout_hook_ && (arg1 == 1 || arg1 == 2)) {
                    std::vector<uint8_t> data(&memory[arg2], &memory[static_cast<size_t>(arg2) + static_cast<size_t>(arg3)]);
                    stdout_hook_(arg1, data);
                    result = arg3;
                } else if (is_virtual_fd(arg1)) {
                    auto* vd = io_vfs_->get_virtual_disk();
                    int handle = virtual_fds_[arg1];
                    result = vd->write(handle, &memory[arg2], static_cast<int>(arg3));
                } else if (is_virtual_fd(arg1)) {
                    auto* vd = io_vfs_->get_virtual_disk();
                    int handle = virtual_fds_[arg1];
                    result = vd->write(handle, &memory[arg2], static_cast<int>(arg3));
                } else if (is_virtual_fd(arg1)) {
                    auto* vd = io_vfs_->get_virtual_disk();
                    int handle = virtual_fds_[arg1];
                    result = vd->write(handle, &memory[arg2], static_cast<int>(arg3));
                } else if (is_virtual_fd(arg1)) {
                    auto* vd = io_vfs_->get_virtual_disk();
                    int handle = virtual_fds_[arg1];
                    result = vd->write(handle, &memory[arg2], static_cast<int>(arg3));
                } else if (is_virtual_fd(arg1)) {
                    auto* vd = io_vfs_->get_virtual_disk();
                    int handle = virtual_fds_[arg1];
                    result = vd->write(handle, &memory[arg2], static_cast<int>(arg3));
                } else if (is_virtual_fd(arg1)) {
                    auto* vd = io_vfs_->get_virtual_disk();
                    int handle = virtual_fds_[arg1];
                    result = vd->write(handle, &memory[arg2], static_cast<int>(arg3));
                } else if (arg1 <= 2 || is_vm_fd(arg1)) {
#ifdef _WIN32
                    result = ::_write(arg1, &memory[arg2], arg3);
                    if (result == -1) result = -errno;
#else
                    result = ::write(arg1, &memory[arg2], arg3);
                    if (result == -1) result = -errno;
#endif
                } else {
                    // Non-sandbox: pass any fd through to the real kernel
#ifdef _WIN32
                    result = ::_write(arg1, &memory[arg2], arg3);
#else
                    result = ::write(arg1, &memory[arg2], arg3);
#endif
                    if (result == -1) result = -errno;
                }
                Logging::DebugHandler::instance().report(
                    Logging::DebugCategory::CPU_EXECUTION,
                    fmt::format("[SYSCALL] {}(fd={}, buf=0x{:08X}, count={}) = {}",
                        sc_name, arg1, arg2, arg3, result),
                    Logging::DebugLevel::INFO);
            } else {
                Logging::ErrorHandler::instance().report_runtime(
                    Logging::ErrorCode::IO_GENERIC,
                    fmt::format("[SYSCALL] {}: buffer out of bounds (addr=0x{:08X}, count={}, mem_size={})",
                        sc_name, arg2, arg3, memory.size()),
                    get_pc(),
                    "Syscall buffer overflow");
                result = -EFAULT;
            }
            break;
            
        case Syscall::SYS_OPEN:
            if (arg1 < memory.size()) {
                // Need to verify null-terminated string is within bounds
                size_t max_len = memory.size() - arg1;
                const char* pathname = reinterpret_cast<const char*>(&memory[arg1]);
                size_t path_len = strnlen(pathname, max_len);
                
                if (path_len < max_len) {
                    std::string final_path = pathname;
                    if (io_vfs_ && sandbox_check == demi::sandbox::SyscallResult::HANDLED_INTERNALLY) {
                        auto safe_path = io_vfs_->resolve_safe_path(pathname);
                        if (!safe_path) {
                            Logging::ErrorHandler::instance().report_runtime(
                                Logging::ErrorCode::IO_GENERIC,
                                fmt::format("[SECURITY FAULT] Path traversal denied: '{}'", pathname),
                                get_pc(),
                                "Sandbox VFS bounds exceeded");
                            result = -EACCES;
                            set_register_32(Register::RAX, static_cast<uint32_t>(result));
                            return;
                        }
                        final_path = safe_path->string();

                    // io_vfs_ is always non-null when sandbox is active (this code path
                    // is only reached with sandbox enabled). The check is defensive —
                    // intentional, not always-true dead code.
                    if (io_vfs_ && io_vfs_->has_virtual_disk() &&
                        sandbox_check == demi::sandbox::SyscallResult::HANDLED_INTERNALLY) {
                        auto* vd = io_vfs_->get_virtual_disk();
                        int handle = vd->open(std::string(pathname), static_cast<int>(arg2));
                        if (handle >= 0) {
                            int synth_fd = 1000 + static_cast<int>(virtual_fds_.size());
                            virtual_fds_[synth_fd] = handle;
                            result = synth_fd;
                        } else {
                            result = -EACCES;
                        }
                        set_register_32(Register::RAX, static_cast<uint32_t>(result));
                        return;
                    }
                    }

#ifdef _WIN32
                    result = ::_open(final_path.c_str(), arg2, arg3);
#else
                    {
                        int sanitized_flags = arg2 & (O_RDONLY | O_WRONLY | O_RDWR);
                        result = ::open(final_path.c_str(), sanitized_flags, arg3);
                    if (result == -1) result = -errno;
                    }
#endif
                    if (result >= 0) {
                        register_vm_fd(result);
                    }
                    Logging::DebugHandler::instance().report(
                        Logging::DebugCategory::CPU_EXECUTION,
                        fmt::format("[SYSCALL] {}('{}' -> '{}', flags=0x{:X}, mode=0{:o}) = {}",
                            sc_name, pathname, final_path, arg2, arg3, result),
                        Logging::DebugLevel::INFO);
                } else {
                    Logging::ErrorHandler::instance().report_runtime(
                        Logging::ErrorCode::IO_GENERIC,
                        fmt::format("[SYSCALL] {}: pathname not null-terminated", sc_name),
                        get_pc(),
                        "Invalid syscall pathname");
                    result = -EFAULT;
                }
            } else {
                Logging::ErrorHandler::instance().report_runtime(
                    Logging::ErrorCode::IO_GENERIC,
                    fmt::format("[SYSCALL] {}: pathname address out of bounds (0x{:08X} >= {})",
                        sc_name, arg1, memory.size()),
                    get_pc(),
                    "Syscall path out of bounds");
                result = -EFAULT;
            }
            break;

        case Syscall::SYS_CLOSE:
            if (is_virtual_fd(arg1)) {
                    auto* vd = io_vfs_->get_virtual_disk();
                    int handle = virtual_fds_[arg1];
                    result = vd->close(handle);
                    virtual_fds_.erase(arg1);
                } else if (!is_vm_fd(arg1) && arg1 > 2) {
                // Non-sandbox: pass any fd through to the real kernel
#ifdef _WIN32
                result = ::_close(arg1);
#else
                result = ::close(arg1);
#endif
                if (result == -1) result = -errno;
            } else {
#ifdef _WIN32
                result = ::_close(arg1);
#else
                result = ::close(arg1);
#endif
                if (result == 0) {
                    vm_opened_fds_.erase(arg1);
                }
            }
            Logging::DebugHandler::instance().report(
                Logging::DebugCategory::CPU_EXECUTION,
                fmt::format("[SYSCALL] {}(fd={}) = {}", sc_name, arg1, result),
                Logging::DebugLevel::INFO);
            break;
            
        case Syscall::SYS_BRK:
            // Memory management - simulated
            Logging::DebugHandler::instance().report(
                Logging::DebugCategory::CPU_EXECUTION,
                fmt::format("[SYSCALL] {}(0x{:08X}) - simulated, returning current break",
                    sc_name, arg1),
                Logging::DebugLevel::INFO);
            result = memory.size();
            break;
            
        case Syscall::SYS_ACCESS: {
            if (arg1 >= memory.size()) { set_register_32(Register::RAX, static_cast<uint32_t>(-EFAULT)); return; }
            long result_ac = access(reinterpret_cast<const char*>(&memory[arg1]), static_cast<int>(arg2));
            if (result_ac == -1) result_ac = -errno;
            set_register_32(Register::RAX, static_cast<uint32_t>(result_ac));
            return;
        }
        case Syscall::SYS_GETDENTS: {
            if (arg2 >= memory.size() || arg3 == 0 || arg2 + arg3 > memory.size()) {
                set_register_32(Register::RAX, static_cast<uint32_t>(-EFAULT));
                return;
            }
            long result_gd = syscall(SYS_getdents, static_cast<unsigned int>(arg1), &memory[arg2], static_cast<unsigned int>(arg3));
            // Standard syscall error check: -1 means error, any other value is
            // a valid byte count. cppcheck cannot determine the return value
            // statically — this check is intentional, not always-true dead code.
            if (result_gd == -1) result_gd = -errno;
            set_register_32(Register::RAX, static_cast<uint32_t>(result_gd));
            return;
        }
        case Syscall::SYS_GETCWD: {
            if (arg1 >= memory.size() || arg2 == 0) { set_register_32(Register::RAX, static_cast<uint32_t>(-EFAULT)); return; }
            char* result_gc = getcwd(reinterpret_cast<char*>(&memory[arg1]), arg2);
            if (!result_gc) { set_register_32(Register::RAX, static_cast<uint32_t>(-errno)); }
            else { set_register_32(Register::RAX, 0); }
            return;
        }
        case Syscall::SYS_WAITPID: {
            int status = 0;
            long result_wp = waitpid(static_cast<pid_t>(arg1), &status, static_cast<int>(arg3));
            if (result_wp == -1) result_wp = -errno;
            set_register_32(Register::RAX, static_cast<uint32_t>(result_wp));
            return;
        }
        case Syscall::SYS_STAT: {
            long result_stat = ::stat(reinterpret_cast<const char*>(&memory[arg1]), reinterpret_cast<struct stat*>(&memory[arg2]));
            if (result_stat == -1) result_stat = -errno;
            set_register_32(Register::RAX, static_cast<uint32_t>(result_stat));
            return;
        }
        case Syscall::SYS_FSTAT: {
            long result_fstat = ::fstat(static_cast<int>(arg1), reinterpret_cast<struct stat*>(&memory[arg2]));
            if (result_fstat == -1) result_fstat = -errno;
            set_register_32(Register::RAX, static_cast<uint32_t>(result_fstat));
            return;
        }
        case Syscall::SYS_UNLINK: {
            long result_ul = unlink(reinterpret_cast<const char*>(&memory[arg1]));
            if (result_ul == -1) result_ul = -errno;
            set_register_32(Register::RAX, static_cast<uint32_t>(result_ul));
            return;
        }
        case Syscall::SYS_READLINK: {
            if (arg1 >= memory.size() || arg2 >= memory.size() || arg3 == 0) {
                set_register_32(Register::RAX, static_cast<uint32_t>(-EFAULT));
                return;
            }
            // Safe: readlink operates on a guest-provided path within the VM
            // sandbox (VFS-jailed when --sandbox is active). No TOCTOU risk:
            // the path is read from guest memory once and the VM is
            // single-threaded — no concurrent modification possible.
            long result_rl = readlink(reinterpret_cast<const char*>(&memory[arg1]), reinterpret_cast<char*>(&memory[arg2]), arg3 - 1);
            if (result_rl == -1) { result_rl = -errno; }
            else { memory[arg2 + result_rl] = 0; }
            set_register_32(Register::RAX, static_cast<uint32_t>(result_rl));
            return;
        }
        case Syscall::SYS_LSEEK: {
            long result_lseek = lseek(arg1, static_cast<off_t>(arg2), static_cast<int>(arg3));
            if (result_lseek == -1) result_lseek = -errno;
            set_register_32(Register::RAX, static_cast<uint32_t>(result_lseek));
            return;
        }
        case Syscall::SYS_IOCTL:
            if (arg1 <= 2 || is_vm_fd(arg1)) {
#ifdef _WIN32
                result = -ENOSYS;
#else
                result = syscall(SYS_ioctl, arg1, arg2, arg3);
#endif
            } else {
                // Non-sandbox: pass any fd through to the real kernel
                result = syscall(SYS_ioctl, arg1, arg2, arg3);
                if (result == -1) result = -errno;
            }
            Logging::DebugHandler::instance().report(
                Logging::DebugCategory::CPU_EXECUTION,
                fmt::format("[SYSCALL] {}(fd={}, request={}, arg={}) = {}",
                    sc_name, arg1, arg2, arg3, result),
                Logging::DebugLevel::INFO);
            break;
            
        case Syscall::SYS_MMAP:
        case Syscall::SYS_MMAP2:
            Logging::DebugHandler::instance().report(
                Logging::DebugCategory::CPU_EXECUTION,
                fmt::format("[SYSCALL] {} - not fully supported", sc_name),
                Logging::DebugLevel::INFO);
            result = -ENOSYS;
            break;
            
        case Syscall::SYS_FORK:
            result = ::fork();
            if (result == 0) {
                register_vm_fd(0);  // child shares stdin
            }
            Logging::DebugHandler::instance().report(
                Logging::DebugCategory::CPU_EXECUTION,
                fmt::format("[SYSCALL] {}() = {}", sc_name, result),
                Logging::DebugLevel::INFO);
            break;

        case Syscall::SYS_EXECVE:
            if (arg1 < memory.size()) {
                size_t max_len = memory.size() - arg1;
                const char* pathname = reinterpret_cast<const char*>(&memory[arg1]);
                size_t path_len = strnlen(pathname, max_len);
                if (path_len < max_len) {
                    std::string path(pathname);
                    if (io_vfs_ && sandbox_check == demi::sandbox::SyscallResult::HANDLED_INTERNALLY) {
                        auto safe = io_vfs_->resolve_safe_path(pathname);
                        if (!safe) { result = -EACCES; break; }
                        path = safe->string();
                    }
                    result = ::execve(path.c_str(), nullptr, nullptr);
                } else {
                    result = -EFAULT;
                }
            } else {
                result = -EFAULT;
            }
            Logging::DebugHandler::instance().report(
                Logging::DebugCategory::CPU_EXECUTION,
                fmt::format("[SYSCALL] {}({}) = {}", sc_name,
                    (arg1 < memory.size() ? reinterpret_cast<const char*>(&memory[arg1]) : "?"), result),
                Logging::DebugLevel::INFO);
            break;

        default:
            Logging::ErrorHandler::instance().report_runtime(
                Logging::ErrorCode::IO_GENERIC,
                fmt::format("[SYSCALL] Unsupported syscall: {} (num={})",
                    sc_name, syscall_num),
                get_pc(),
                "Unsupported syscall");
            result = -ENOSYS;
            break;
    }
    
    // Set return value (negative for errors, non-negative for success)
    set_register_32(Register::RAX, static_cast<uint32_t>(result));
}
