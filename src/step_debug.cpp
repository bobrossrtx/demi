#include "config.hpp"
#include "engine/cpu.hpp"
#include "engine/cpu_flags.hpp"
#include "engine/cpu_registers.hpp"
// Use consolidated dispatcher (single-instruction, no loop)
extern void dispatch_opcode(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>

using namespace DemiEngine_Registers;

static const char* reg_names[] = {
    "RAX","RCX","RDX","RBX","RSP","RBP","RSI","RDI",
    "R8","R9","R10","R11","R12","R13","R14","R15"
};

static void print_regs(CPU& cpu) {
    std::cout << "  PC=0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::dec;
    std::cout << "  SP=0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_sp() << std::dec;
    uint32_t flags = cpu.get_flags();
    std::cout << "  FLAGS=" << (flags & FLAG_ZERO ? "Z" : "-")
              << (flags & FLAG_SIGN ? "S" : "-")
              << (flags & FLAG_CARRY ? "C" : "-")
              << (flags & FLAG_OVERFLOW ? "O" : "-")
              << (flags & FLAG_DIRECTION ? "D" : "-")
              << (flags & FLAG_INTERRUPT ? "I" : "-");
    std::cout << std::endl << "  ";
    for (int i = 0; i < 8; i++)
        std::cout << reg_names[i] << "=0x" << std::hex << std::setw(8) << std::setfill('0')
                  << cpu.get_register_32(static_cast<Register>(i)) << std::dec << " ";
    std::cout << std::endl;
}

// Single-step: dispatch exactly one instruction, bypassing fusion
static bool step_one(CPU& cpu, const std::vector<uint8_t>& program) {
    if (cpu.get_pc() >= program.size()) return false;
    bool running = true;
    cpu.handle_pending_interrupts(program, running);
    if (!running) return false;
    dispatch_opcode(cpu, program, running);
    return running;
}

void run_step_debug(const std::vector<uint8_t>& program, uint32_t entry_addr) {
    CPU cpu;
    cpu.reset();
    initialize_devices(&cpu);
    
    auto& mem = cpu.get_memory();
    std::copy(program.begin(), program.end(), mem.begin());
    cpu.set_pc(entry_addr);
    cpu.set_sp(mem.size());
    cpu.set_fp(mem.size());
    
    std::cout << "\n=== Step Debugger ===" << std::endl;
    std::cout << "Program: " << program.size() << " bytes, entry=0x"
              << std::hex << entry_addr << std::dec << std::endl;
    std::cout << "[Enter]=step  [c]=continue  [r]=regs  [q]=quit  [h]=help" << std::endl << std::endl;
    
    size_t step_count = 0;
    bool running = true;
    std::string cmd;
    
    while (running && cpu.get_pc() < program.size()) {
        if (step_count == 0 || cmd == "" || cmd == "s") {
            running = step_one(cpu, program);
            step_count++;
            print_regs(cpu);
        }
        if (!running) { std::cout << "Program halted at step " << step_count << std::endl; break; }
        std::cout << "debug> " << std::flush;
        if (!std::getline(std::cin, cmd)) break;
        if (cmd == "q" || cmd == "quit" || cmd == "exit") { std::cout << "Debugger exit." << std::endl; break; }
        if (cmd == "c" || cmd == "continue") {
            for (size_t i = 0; i < 10000 && running; i++) { running = step_one(cpu, program); step_count++; }
            print_regs(cpu);
            std::cout << "Program finished after " << step_count << " steps." << std::endl;
            break;
        }
        if (cmd == "r" || cmd == "regs") { print_regs(cpu); continue; }
        if (cmd == "h" || cmd == "help") {
            std::cout << "  Enter/s  = single step" << std::endl;
            std::cout << "  c        = continue (run to end)" << std::endl;
            std::cout << "  r        = show registers" << std::endl;
            std::cout << "  m ADDR N = dump N bytes of memory at ADDR" << std::endl;
            std::cout << "  q        = quit debugger" << std::endl;
            continue;
        }
        if (cmd.substr(0,1) == "m") {
            std::istringstream iss(cmd); std::string m; uint32_t addr = 0; int count = 16;
            iss >> m >> std::hex >> addr >> count;
            auto& mref = cpu.get_memory();
            std::cout << "  mem[0x" << std::hex << addr << "]: ";
            for (int i = 0; i < count && (addr+i) < mref.size(); i++)
                std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)mref[addr+i] << " ";
            std::cout << std::dec << std::endl;
            continue;
        }
    }
}
