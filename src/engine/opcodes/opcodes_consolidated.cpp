// Consolidated opcode implementations for standalone builds
// This file includes all opcode implementations in a single compilation unit
// to reduce compilation time for standalone executables

#include "opcode_dispatcher.hpp"
#include <iostream>
#include "../cpu.hpp"
#include "../cpu_flags.hpp"
#include "../../assembler/opcodes.hpp"
#include "../../debug/debug_handler.hpp"
#include "../../debug/error_handler.hpp"
#include <fmt/core.h>
#include <iomanip>
#include <cmath>
#include "../safe_memcpy.hpp"

using Logging::DebugHandler;
using Logging::DebugCategory;
using Logging::DebugLevel;
using Logging::ErrorHandler;
using Logging::ErrorCode;

// Forward declarations for handlers referenced before definition
void handle_mod64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);

// Include all opcode header files
#include "add.hpp"
#include "and.hpp"
#include "call.hpp"
#include "cmp.hpp"
#include "db.hpp"
#include "dec.hpp"
#include "div.hpp"
#include "fadd.hpp"
#include "fsub.hpp"
#include "fmul.hpp"
#include "fdiv.hpp"
#include "fild.hpp"
#include "fist.hpp"
#include "finit.hpp"
#include "fistp.hpp"
#include "fld.hpp"
#include "fst.hpp"
#include "fstp.hpp"
#include "fabs.hpp"
#include "fchs.hpp"
#include "fsqrt.hpp"
#include "fsin.hpp"
#include "fcos.hpp"
#include "ftan.hpp"
#include "fcompp.hpp"
#include "fucompp.hpp"
#include "fclex.hpp"
#include "fstcw.hpp"
#include "fldcw.hpp"
#include "fstsw.hpp"
#include "halt.hpp"
#include "cli.hpp"
#include "sti.hpp"
#include "int.hpp"
#include "iret.hpp"
#include "inb.hpp"
#include "inc.hpp"
#include "in.hpp"
#include "inl.hpp"
#include "instr.hpp"
#include "inw.hpp"
#include "jmp.hpp"
#include "jns.hpp"
#include "jnz.hpp"
#include "js.hpp"
#include "jz.hpp"
#include "jc.hpp"
#include "jnc.hpp"
#include "jo.hpp"
#include "jno.hpp"
#include "lea.hpp"
#include "load.hpp"
#include "load_imm.hpp"
#include "loadr.hpp"
#include "mov.hpp"
#include "mul.hpp"
#include "nop.hpp"
#include "not.hpp"
#include "opcode_handler.hpp"
#include "or.hpp"
#include "outb.hpp"
#include "out.hpp"
#include "outl.hpp"
#include "outstr.hpp"
#include "outw.hpp"
#include "pop_arg.hpp"
#include "pop_flag.hpp"
#include "pop.hpp"
#include "push_arg.hpp"
#include "push_flag.hpp"
#include "push.hpp"
#include "ret.hpp"
#include "shl.hpp"
#include "shr.hpp"
#include "store.hpp"
#include "sub.hpp"
#include "swap.hpp"
#include "xor.hpp"

// High-priority missing x86-equivalent opcodes
#include "adc.hpp"
#include "sbb.hpp"
#include "imul.hpp"
#include "idiv.hpp"
#include "sal.hpp"
#include "sar.hpp"

// Medium-priority x86-equivalent opcodes
#include "clc.hpp"
#include "stc.hpp"
#include "cmc.hpp"
#include "cld.hpp"
#include "std.hpp"
#include "lahf.hpp"
#include "sahf.hpp"
#include "cbw.hpp"
#include "cwde.hpp"
#include "cwd.hpp"
#include "cdq.hpp"
#include "rol.hpp"
#include "ror.hpp"
#include "loop.hpp"
#include "loope.hpp"
#include "loopne.hpp"
#include "rcl.hpp"
#include "rcr.hpp"
#include "setz.hpp"
#include "setnz.hpp"
#include "setc.hpp"
#include "setnc.hpp"
#include "seto.hpp"
#include "setno.hpp"
#include "sets.hpp"
#include "setns.hpp"
#include "setg.hpp"
#include "setge.hpp"
#include "setl.hpp"
#include "setle.hpp"
#include "xchg.hpp"
#include "bswap.hpp"
#include "movsx.hpp"
#include "movzx.hpp"
#include "cmovo.hpp"
#include "cmovno.hpp"
#include "cmovc.hpp"
#include "cmovnc.hpp"
#include "cmovz.hpp"
#include "cmovnz.hpp"
#include "cmovs.hpp"
#include "cmovns.hpp"
#include "cmovg.hpp"
#include "cmovge.hpp"
#include "cmovl.hpp"
#include "cmovle.hpp"
#include "cmova.hpp"
#include "cmovbe.hpp"
#include "movsb.hpp"
#include "movsw.hpp"
#include "movsd.hpp"
#include "stosb.hpp"
#include "stosw.hpp"
#include "stosd.hpp"
#include "lodsb.hpp"
#include "lodsw.hpp"
#include "lodsd.hpp"
#include "bt.hpp"
#include "bts.hpp"
#include "btr.hpp"
#include "btc.hpp"
#include "cmpxchg.hpp"
#include "xadd.hpp"
#include "cpuid.hpp"
#include "rdtsc.hpp"
#include "syscall.hpp"
#include "sysenter.hpp"
#include "enter.hpp"
#include "rep.hpp"

// Extended 64-bit register operation headers
#include "add64.hpp"
#include "sub64.hpp"
#include "mov64.hpp"
#include "load_imm64.hpp"
#include "mul64.hpp"
#include "div64.hpp"
#include "and64.hpp"
#include "or64.hpp"
#include "xor64.hpp"
#include "cmp64.hpp"
#include "movex.hpp"
#include "addex.hpp"
#include "subex.hpp"
#include "loadex.hpp"
#include "storex.hpp"

// CPU mode control headers
#include "mode32.hpp"
#include "mode64.hpp"
#include "modecmp.hpp"

// SIMD instruction headers
#include "vadd.hpp"
#include "vmul.hpp"
#include "vdot.hpp"
#include "vmax.hpp"
#include "vbroadcast.hpp"
#include "vcmpgt.hpp"
#include "packb.hpp"
#include "unpackb.hpp"

// Consolidated implementations of all opcodes

// Implementation from add.cpp
void handle_add(CPU& cpu, const std::vector<uint8_t>& program, [[maybe_unused]] bool& running) {
    if (cpu.get_pc() + 2 < program.size()) {
        uint8_t reg1 = program[cpu.get_pc() + 1];
        uint8_t reg2 = program[cpu.get_pc() + 2];
        
        // For regular ADD, validate against legacy register bounds (0-7)
        // Extended operations should use their own handlers (ADDEX, ADD64, etc.)
        const size_t MAX_LEGACY_REG = 7;
        if (reg1 > MAX_LEGACY_REG || reg2 > MAX_LEGACY_REG) {
            // Check if this might be an extended register operation that should use a different handler
            DebugHandler::instance().report(DebugCategory::CPU_REGISTERS, fmt::format(
                "[PC=0x{:04X}] [ADD] Register access beyond legacy range: R{}, R{} (max for ADD: R{}). Consider using ADDEX for extended registers.", 
                cpu.get_pc(), reg1, reg2, MAX_LEGACY_REG
            ), DebugLevel::IMPORTANT);
            
            // For backward compatibility, allow access to the full register array but warn
            // This prevents crashes while highlighting that extended operations should be used
            if (reg1 >= DemiEngine_Registers::TOTAL_REGISTERS || reg2 >= DemiEngine_Registers::TOTAL_REGISTERS) {
                std::string context = fmt::format("Register R{} or R{} out of range (max: R{})", reg1, reg2, DemiEngine_Registers::TOTAL_REGISTERS - 1);
                std::string message = "Invalid register access in ADD instruction";
                ErrorHandler::instance().report_runtime(ErrorCode::CPU_INVALID_REGISTER, message, cpu.get_pc(), context);
                cpu.set_pc(cpu.get_pc() + 3);
                cpu.print_state("ADD");
                return;
            }
        }
        
        DEBUG_INSTRUCTION("ADD", cpu.get_pc(), fmt::format("R{} += R{}", reg1, reg2), "");

        uint64_t val1 = cpu.get_register_mode_aware(static_cast<Register>(reg1));
        uint64_t val2 = cpu.get_register_mode_aware(static_cast<Register>(reg2));
        
        uint64_t result = val1 + val2;
        uint64_t mask = cpu.get_operand_mask();
        uint64_t masked_result = result & mask;

        uint32_t current_flags = cpu.get_flags();

        // Zero Flag
        if (masked_result == 0) {
            current_flags |= FLAG_ZERO;
        } else {
            current_flags &= ~FLAG_ZERO;
        }

        // Carry Flag
        bool carry = false;
        if (cpu.is_32bit_mode()) {
            carry = (result > 0xFFFFFFFF);
        } else {
            carry = (result < val1); // Overflow check for 64-bit
        }

        if (carry) {
            current_flags |= FLAG_CARRY;
        } else {
            current_flags &= ~FLAG_CARRY;
        }

        // Overflow Flag (Signed)
        bool sign1, sign2, signR;
        if (cpu.is_32bit_mode()) {
            sign1 = (val1 & 0x80000000) != 0;
            sign2 = (val2 & 0x80000000) != 0;
            signR = (masked_result & 0x80000000) != 0;
        } else {
            sign1 = (val1 & 0x8000000000000000ULL) != 0;
            sign2 = (val2 & 0x8000000000000000ULL) != 0;
            signR = (masked_result & 0x8000000000000000ULL) != 0;
        }

        if ((sign1 == sign2) && (sign1 != signR)) {
            current_flags |= FLAG_OVERFLOW;
        } else {
            current_flags &= ~FLAG_OVERFLOW;
        }
        
        // Sign Flag
        if (cpu.is_32bit_mode()) {
             if ((masked_result & 0x80000000) != 0) current_flags |= FLAG_SIGN; else current_flags &= ~FLAG_SIGN;
        } else {
             if ((masked_result & 0x8000000000000000ULL) != 0) current_flags |= FLAG_SIGN; else current_flags &= ~FLAG_SIGN;
        }

        cpu.set_flags(current_flags);
        cpu.set_register_mode_aware(static_cast<Register>(reg1), result);
        
        DEBUG_DETAIL(Logging::DebugCategory::CPU_REGISTERS, "R{}: {} + {} = {} (carry={}, overflow={})", 
                    reg1, val1, val2, masked_result, (current_flags & FLAG_CARRY) ? 1 : 0, 
                    (current_flags & FLAG_OVERFLOW) ? 1 : 0);
        
        DEBUG_DETAIL(Logging::DebugCategory::CPU_REGISTERS, "R{}: {} + {} = {} (carry={}, overflow={})", 
                    reg1, val1, val2, masked_result, (current_flags & FLAG_CARRY) ? 1 : 0, 
                    (current_flags & FLAG_OVERFLOW) ? 1 : 0);
    }
    cpu.set_pc(cpu.get_pc() + 3);
    cpu.print_state("ADD");
}

// Implementation from and.cpp
void handle_and(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg1 = program[pc + 1];
        uint8_t reg2 = program[pc + 2];

        if (reg1 < DemiEngine_Registers::TOTAL_REGISTERS && reg2 < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val1 = cpu.get_register_mode_aware(static_cast<Register>(reg1));
            uint64_t val2 = cpu.get_register_mode_aware(static_cast<Register>(reg2));
            uint64_t result = val1 & val2;
            uint64_t mask = cpu.get_operand_mask();
            uint64_t masked_result = result & mask;

            // Flags
            uint32_t current_flags = cpu.get_flags();
            if (masked_result == 0) {
                current_flags |= FLAG_ZERO;
            } else {
                current_flags &= ~FLAG_ZERO;
            }
            // AND clears Carry and Overflow
            current_flags &= ~FLAG_CARRY;
            current_flags &= ~FLAG_OVERFLOW;
            
            cpu.set_flags(current_flags);
            cpu.set_register_mode_aware(static_cast<Register>(reg1), result);
        }

        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }

    cpu.print_state("AND");
}

// Implementation from call.cpp
void handle_call(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    // Check for call stack overflow (too many nested calls)
    size_t max_depth = cpu.get_effective_max_call_depth();
    if (cpu.get_call_depth() >= max_depth) {
        std::string context = fmt::format("Call depth: {} (max: {})", cpu.get_call_depth(), max_depth);
        std::string message = fmt::format("Call stack overflow: maximum nesting depth exceeded");
        ErrorHandler::instance().report_runtime(ErrorCode::CPU_CALL_STACK_OVERFLOW, message, pc, context);
        running = false;
        throw CPUException(message);
    }

    // Check for stack overflow (SP going below reasonable minimum for push of 8 bytes)
    if (cpu.get_sp() < 12) {
        std::string context = fmt::format("Stack pointer: 0x{:X}, minimum safe SP: 0x000C (needs 8 bytes for FP + return addr)", cpu.get_sp());
        std::string message = "Stack overflow during CALL: insufficient space";
        ErrorHandler::instance().report_runtime(ErrorCode::CPU_STACK_OVERFLOW, message, pc, context);
        running = false;
        throw CPUException(message);
    }

    if (!cpu.validate_stack_push(8)) {
        running = false;
        throw CPUException("Stack overflow during CALL");
    }

    // Reset offset at each call
    cpu.set_arg_offset(8);

    // Read 4-byte address
    uint32_t addr = 0;
    if (pc + 4 < program.size()) {
        addr = static_cast<uint32_t>(program[pc + 1]) |
               (static_cast<uint32_t>(program[pc + 2]) << 8) |
               (static_cast<uint32_t>(program[pc + 3]) << 16) |
               (static_cast<uint32_t>(program[pc + 4]) << 24);
    } else {
        running = false;
        return;
    }

    // Push old FP
    uint32_t sp = cpu.get_sp() - 4;
    cpu.set_sp(sp);
    cpu.get_registers()[4] = sp;
    cpu.write_mem32(sp, cpu.get_fp());

    // Push return address (pc + 5 for 5-byte CALL instruction)
    sp -= 4;
    cpu.set_sp(sp);
    cpu.get_registers()[4] = sp;
        uint32_t return_addr = pc + 5;
        cpu.write_mem32(sp, return_addr);    // Set new FP
    cpu.set_fp(sp);
    
    // Increment call depth
    cpu.increment_call_depth();
    
    cpu.print_stack_frame("CALL");
    cpu.set_pc(addr);
    cpu.print_state("CALL");
}

// Implementation from cmp.cpp
void handle_cmp(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 < program.size()) {
        uint8_t reg1 = program[cpu.get_pc() + 1];
        uint8_t reg2 = program[cpu.get_pc() + 2];
        if (reg1 < DemiEngine_Registers::TOTAL_REGISTERS && reg2 < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val1 = cpu.get_register_mode_aware(static_cast<Register>(reg1));
            uint64_t val2 = cpu.get_register_mode_aware(static_cast<Register>(reg2));
            
            uint64_t result = val1 - val2;
            uint64_t mask = cpu.get_operand_mask();
            uint64_t masked_result = result & mask;
            
            uint32_t flags = cpu.get_flags();
            
            // Zero Flag
            if (masked_result == 0) {
                flags |= FLAG_ZERO;
            } else {
                flags &= ~FLAG_ZERO;
            }
            
            // Sign Flag (MSB)
            bool sign = false;
            if (cpu.is_32bit_mode()) {
                sign = (masked_result & 0x80000000) != 0;
            } else {
                sign = (masked_result & 0x8000000000000000ULL) != 0;
            }
            
            if (sign) {
                flags |= FLAG_SIGN;
            } else {
                flags &= ~FLAG_SIGN;
            }
            
            // Carry Flag (Borrow)
            bool borrow = false;
            if (cpu.is_32bit_mode()) {
                borrow = (static_cast<uint32_t>(val1) < static_cast<uint32_t>(val2));
            } else {
                borrow = (val1 < val2);
            }
            
            if (borrow) {
                flags |= FLAG_CARRY;
            } else {
                flags &= ~FLAG_CARRY;
            }
            
            // Overflow Flag (Signed)
            bool sign1, sign2, signR;
            if (cpu.is_32bit_mode()) {
                sign1 = (val1 & 0x80000000) != 0;
                sign2 = (val2 & 0x80000000) != 0;
                signR = (masked_result & 0x80000000) != 0;
            } else {
                sign1 = (val1 & 0x8000000000000000) != 0;
                sign2 = (val2 & 0x8000000000000000) != 0;
                signR = (masked_result & 0x8000000000000000) != 0;
            }
            
            if ((sign1 != sign2) && (sign1 != signR)) {
                flags |= FLAG_OVERFLOW;
            } else {
                flags &= ~FLAG_OVERFLOW;
            }
            
            DEBUG_DETAIL(Logging::DebugCategory::CPU_FLAGS, "CMP R{}={} R{}={} Result={} SF={} ZF={} OF={}", 
                (int)reg1, val1, (int)reg2, val2, (int64_t)masked_result,
                ((flags & FLAG_SIGN) ? 1 : 0), ((flags & FLAG_ZERO) ? 1 : 0), ((flags & FLAG_OVERFLOW) ? 1 : 0));



            cpu.set_flags(flags);
        }
        cpu.set_pc(cpu.get_pc() + 3);
    } else {
        running = false;
    }
    cpu.print_state("CMP");
}

// Implementation from db.cpp
void handle_db(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    // DB opcode: Define bytes - copy data to specified address
    if (pc + 2 < program.size()) {
        uint8_t target_addr = program[pc + 1]; // Target memory address
        uint8_t length = program[pc + 2]; // Number of data bytes

        DebugHandler::instance().report(DebugCategory::MEM_ACCESS, fmt::format(
            "[PC=0x{:04X}] [DB] Copying {} data bytes to address 0x{:02X}",
            pc, length, target_addr
        ), DebugLevel::DETAIL);

        // Copy data bytes to memory starting at target_addr
        for (uint8_t i = 0; i < length && (pc + 3 + i) < program.size() && (target_addr + i) < cpu.get_memory().size(); ++i) {
            cpu.get_memory()[target_addr + i] = program[pc + 3 + i];

            DebugHandler::instance().report(DebugCategory::MEM_ACCESS, fmt::format(
                "[PC=0x{:04X}] [DB] memory[0x{:02X}] = 0x{:02X} ('{}')",
                pc, target_addr + i, program[pc + 3 + i],
                (program[pc + 3 + i] >= 32 && program[pc + 3 + i] <= 126) ? static_cast<char>(program[pc + 3 + i]) : '.'
            ), DebugLevel::DETAIL);
        }

        cpu.set_pc(pc + 3 + length); // Skip opcode + target_addr + length + data bytes
    } else {
        running = false;
    }

    cpu.print_state("DB");
}

// Implementation from dec.cpp
void handle_dec(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 1 < program.size()) {
        uint8_t reg = program[cpu.get_pc() + 1];
        DEBUG_INSTRUCTION("DEC", cpu.get_pc(), fmt::format("R{}", static_cast<int>(reg)), "");
        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val = cpu.get_register_mode_aware(static_cast<Register>(reg));
            uint64_t result = val - 1;
            uint64_t mask = cpu.get_operand_mask();
            uint64_t masked_result = result & mask;
            
            // Flags
            uint32_t current_flags = cpu.get_flags();
            if (masked_result == 0) {
                current_flags |= FLAG_ZERO;
            } else {
                current_flags &= ~FLAG_ZERO;
            }
            
            // Overflow (Signed)
            bool overflow = false;
            if (cpu.is_32bit_mode()) {
                overflow = (val == 0x80000000);
            } else {
                overflow = (val == 0x8000000000000000ULL);
            }
            
            if (overflow) {
                current_flags |= FLAG_OVERFLOW;
            } else {
                current_flags &= ~FLAG_OVERFLOW;
            }
            
            cpu.set_flags(current_flags);
            cpu.set_register_mode_aware(static_cast<Register>(reg), result);
            
            DEBUG_DETAIL(Logging::DebugCategory::CPU_REGISTERS, "R{}: {} - 1 = {}", static_cast<int>(reg), val, masked_result);
        }
        cpu.set_pc(cpu.get_pc() + 2);
    } else {
        running = false;
    }
    cpu.print_state("DEC");
}

// Implementation from div.cpp
void handle_div(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 < program.size()) {
        uint8_t reg1 = program[cpu.get_pc() + 1];
        uint8_t reg2 = program[cpu.get_pc() + 2];
        DEBUG_INSTRUCTION("DIV", cpu.get_pc(), fmt::format("R{} /= R{}", reg1, reg2), "");
        if (reg1 < DemiEngine_Registers::TOTAL_REGISTERS && reg2 < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val1 = cpu.get_register_mode_aware(static_cast<Register>(reg1));
            uint64_t val2 = cpu.get_register_mode_aware(static_cast<Register>(reg2));

            if (val2 == 0) {
                std::string context = fmt::format("R{} = {}, R{} = 0", reg1, val1, reg2);
                std::string message = "Division by zero: attempted to divide by register with value 0";
                ErrorHandler::instance().report_runtime(ErrorCode::CPU_DIVISION_BY_ZERO, message, cpu.get_pc(), context);
                // Trigger divide-by-zero exception (vector 0x00) via interrupt controller
                // If no handler is installed, handle_pending_interrupts will halt the VM
                cpu.get_interrupt_controller().trigger_exception(
                    DemiEngine_Interrupts::CPUException::DIVIDE_BY_ZERO);
                cpu.set_pc(cpu.get_pc() + 3);
                return;
            }
            
            uint64_t quotient = val1 / val2;
            uint64_t remainder = val1 % val2;
            cpu.set_register_mode_aware(static_cast<Register>(reg1), quotient);
            // Set remainder in RDX (register 2) to match x86 DIV behavior
            cpu.set_register_mode_aware(static_cast<Register>(2), remainder);

            uint32_t flags = cpu.get_flags();
            flags &= ~(FLAG_ZERO | FLAG_SIGN | FLAG_CARRY | FLAG_OVERFLOW);
            if (quotient == 0) flags |= FLAG_ZERO;
            if (static_cast<int64_t>(quotient) < 0) flags |= FLAG_SIGN;
            cpu.set_flags(flags);
            
            DEBUG_DETAIL(Logging::DebugCategory::CPU_REGISTERS, "R{}: {} / {} = {} (remainder in RDX: {})", reg1, val1, val2, quotient, remainder);
        }
        cpu.set_pc(cpu.get_pc() + 3);
    } else {
        running = false;
    }
    cpu.print_state("DIV");
}

// Implementation for MOD
void handle_mod(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 < program.size()) {
        uint8_t reg1 = program[cpu.get_pc() + 1];
        uint8_t reg2 = program[cpu.get_pc() + 2];
        DEBUG_INSTRUCTION("MOD", cpu.get_pc(), fmt::format("R{} %= R{}", reg1, reg2), "");
        if (reg1 < DemiEngine_Registers::TOTAL_REGISTERS && reg2 < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val1 = cpu.get_register_mode_aware(static_cast<Register>(reg1));
            uint64_t val2 = cpu.get_register_mode_aware(static_cast<Register>(reg2));

            if (val2 == 0) {
                std::string context = fmt::format("R{} = {}, R{} = 0", reg1, val1, reg2);
                std::string message = "Modulo by zero: attempted to compute modulo with divisor of 0";
                ErrorHandler::instance().report_runtime(ErrorCode::CPU_MODULO_BY_ZERO, message, cpu.get_pc(), context);
                running = false;
                throw CPUException(message);
            }
            
            uint64_t result = val1 % val2;
            cpu.set_register_mode_aware(static_cast<Register>(reg1), result);
            
            DEBUG_DETAIL(Logging::DebugCategory::CPU_REGISTERS, "R{}: {} % {} = {}", reg1, val1, val2, result);
        }
        cpu.set_pc(cpu.get_pc() + 3);
    } else {
        running = false;
    }
    cpu.print_state("MOD");
}

// Implementation from halt.cpp
void handle_halt(CPU& cpu, [[maybe_unused]] const std::vector<uint8_t>& program, [[maybe_unused]] bool& running) {
    // FIXED: Remove Logger call to prevent deadlock
    // ) << std::endl;
    running = false;
    cpu.set_pc(cpu.get_pc() + 1);
    cpu.print_state("HALT");
}

// Implementation for INT opcode - Software interrupt
void handle_int(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    
    if (pc + 1 < program.size()) {
        uint8_t vector = program[pc + 1];
        
        DebugHandler::instance().report(DebugCategory::IO_INTERRUPT, fmt::format(
            "[PC=0x{:04X}] [INT] Triggering software interrupt vector 0x{:02X}",
            pc, vector
        ), DebugLevel::INFO);
        
        cpu.trigger_interrupt(vector);
        cpu.set_pc(pc + 2);
    } else {
        running = false;
    }
    
    cpu.print_state("INT");
}

// Implementation for IRET opcode - Return from interrupt
void handle_iret(CPU& cpu, [[maybe_unused]] const std::vector<uint8_t>& program, [[maybe_unused]] bool& running) {
    uint32_t pc = cpu.get_pc();
    
    DebugHandler::instance().report(DebugCategory::IO_INTERRUPT, fmt::format(
        "[PC=0x{:04X}] [IRET] Returning from interrupt",
        pc
    ), DebugLevel::INFO);
    
    // Use the full restore_interrupt_state which properly unwinds the
    // interrupt stack frame (flags + CS + return address + 16 GPRs)
    cpu.restore_interrupt_state();
    
    cpu.print_state("IRET");
}

// Implementation for CLI opcode - Clear interrupt flag (disable interrupts)
void handle_cli(CPU& cpu, [[maybe_unused]] const std::vector<uint8_t>& program, [[maybe_unused]] bool& running) {
    uint32_t pc = cpu.get_pc();
    
    DebugHandler::instance().report(DebugCategory::IO_INTERRUPT, 
        "[CLI] Disabling interrupts",
        DebugLevel::INFO);
    
    cpu.get_interrupt_controller().disable_interrupts();
    cpu.set_flags(cpu.get_flags() & ~FLAG_INTERRUPT);
    cpu.set_pc(pc + 1);
    
    cpu.print_state("CLI");
}

// Implementation for STI opcode - Set interrupt flag (enable interrupts)
void handle_sti(CPU& cpu, [[maybe_unused]] const std::vector<uint8_t>& program, [[maybe_unused]] bool& running) {
    uint32_t pc = cpu.get_pc();
    
    DebugHandler::instance().report(DebugCategory::IO_INTERRUPT, 
        "[STI] Enabling interrupts",
        DebugLevel::INFO);
    
    cpu.get_interrupt_controller().enable_interrupts();
    cpu.set_flags(cpu.get_flags() | FLAG_INTERRUPT);
    cpu.set_pc(pc + 1);
    
    cpu.print_state("STI");
}

// Implementation from inb.cpp
void handle_inb(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg = program[pc + 1];
        uint8_t port = program[pc + 2];

        DebugHandler::instance().report(DebugCategory::IO_DEVICE, fmt::format(
            "[PC=0x{:04X}] [INB] PC={} R{} <- port {}",
            pc, pc, reg, port
        ), DebugLevel::DETAIL);

        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint8_t value = cpu.read_port(port);
            cpu.get_registers()[reg] = value;

            DebugHandler::instance().report(DebugCategory::IO_DEVICE, fmt::format(
                "[PC=0x{:04X}] [INB] R{} = {}",
                pc, reg, value
            ), DebugLevel::DETAIL);
        }

        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }

    cpu.print_state("INB");
}

// Implementation from inc.cpp
void handle_inc(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 1 < program.size()) {
        uint8_t reg = program[cpu.get_pc() + 1];
        DEBUG_INSTRUCTION("INC", cpu.get_pc(), fmt::format("R{}", static_cast<int>(reg)), "");
        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val = cpu.get_register_mode_aware(static_cast<Register>(reg));
            uint64_t result = val + 1;
            uint64_t mask = cpu.get_operand_mask();
            uint64_t masked_result = result & mask;
            
            
            // Flags
            uint32_t current_flags = cpu.get_flags();
            if (masked_result == 0) {
                current_flags |= FLAG_ZERO;
            } else {
                current_flags &= ~FLAG_ZERO;
            }
            
            // Overflow (Signed)
            bool overflow = false;
            if (cpu.is_32bit_mode()) {
                overflow = (val == 0x7FFFFFFF);
            } else {
                overflow = (val == 0x7FFFFFFFFFFFFFFFULL);
            }
            
            if (overflow) {
                current_flags |= FLAG_OVERFLOW;
            } else {
                current_flags &= ~FLAG_OVERFLOW;
            }
            
            cpu.set_flags(current_flags);
            cpu.set_register_mode_aware(static_cast<Register>(reg), result);
            
            DEBUG_DETAIL(Logging::DebugCategory::CPU_REGISTERS, "R{}: {} + 1 = {}", static_cast<int>(reg), val, masked_result);
        }
        cpu.set_pc(cpu.get_pc() + 2);
    } else {
        running = false;
    }
    cpu.print_state("INC");
}

// Implementation from in.cpp
void handle_in(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg = program[pc + 1];
        uint8_t port = program[pc + 2];

        DebugHandler::instance().report(DebugCategory::IO_DEVICE, fmt::format(
            "[PC=0x{:04X}] [IN] PC={} R{} <- port {}",
            pc, pc, reg, port
        ), DebugLevel::DETAIL);

        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint8_t value = cpu.read_port(port);
            cpu.get_registers()[reg] = value;

            DebugHandler::instance().report(DebugCategory::IO_DEVICE, fmt::format(
                "[PC=0x{:04X}] [IN] R{} = {}",
                pc, reg, value
            ), DebugLevel::DETAIL);
        }

        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }

    cpu.print_state("IN");
}

// Implementation from inl.cpp
void handle_inl(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg = program[pc + 1];
        uint8_t port = program[pc + 2];

        DebugHandler::instance().report(DebugCategory::IO_DEVICE, fmt::format(
            "[PC=0x{:04X}] [INL] PC={} R{} <- port {} (dword)",
            pc, pc, reg, port
        ), DebugLevel::DETAIL);

        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint32_t value = cpu.read_port_dword(port);
            // Store bytes in reg, reg+1, reg+2, reg+3 if available
            for (size_t i = 0; i < 4 && (reg + i) < DemiEngine_Registers::TOTAL_REGISTERS; ++i) {
                cpu.get_registers()[reg + i] = static_cast<uint8_t>((value >> (8 * i)) & 0xFF);
            }
        }

        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }

    cpu.print_state("INL");
}

// Implementation from instr.cpp
void handle_instr(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg = program[pc + 1];
        uint8_t port = program[pc + 2];

        DebugHandler::instance().report(DebugCategory::IO_DEVICE, fmt::format(
            "[PC=0x{:04X}] [INSTR] PC={} R{} <- port {} (string)",
            pc, pc, reg, port
        ), DebugLevel::DETAIL);

        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint8_t maxLength = cpu.get_registers()[reg]; // Use register value as max length
            std::string value = cpu.read_port_string(port, maxLength);

            DebugHandler::instance().report(DebugCategory::IO_DEVICE, fmt::format(
                "[PC=0x{:04X}] [INSTR] Read string: '{}'",
                pc, value
            ), DebugLevel::DETAIL);

            // Store string length in register
            cpu.get_registers()[reg] = static_cast<uint8_t>(value.length());
        }

        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }

    cpu.print_state("INSTR");
}

// Implementation from inw.cpp
void handle_inw(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg = program[pc + 1];
        uint8_t port = program[pc + 2];

        DebugHandler::instance().report(DebugCategory::IO_DEVICE, fmt::format(
            "[PC=0x{:04X}] [INW] PC={} R{} <- port {} (word)",
            pc, pc, reg, port
        ), DebugLevel::DETAIL);

        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint16_t value = cpu.read_port_word(port);
            // Store lower 8 bits in reg, upper 8 bits in next reg (if exists)
            cpu.get_registers()[reg] = static_cast<uint8_t>(value & 0xFF);
            if (static_cast<size_t>(reg + 1) < DemiEngine_Registers::TOTAL_REGISTERS) {
                cpu.get_registers()[reg + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
            }

            DebugHandler::instance().report(DebugCategory::IO_DEVICE, fmt::format(
                "[PC=0x{:04X}] [INW] R{} = {}, R{} = {}",
                pc, reg, cpu.get_registers()[reg], reg + 1,
                (static_cast<size_t>(reg + 1) < DemiEngine_Registers::TOTAL_REGISTERS ? cpu.get_registers()[reg + 1] : 0)
            ), DebugLevel::DETAIL);
        }

        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }

    cpu.print_state("INW");
}

// Implementation from jmp.cpp
void handle_jmp(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 4 < program.size()) {
        uint32_t addr = static_cast<uint32_t>(program[cpu.get_pc() + 1]) |
                       (static_cast<uint32_t>(program[cpu.get_pc() + 2]) << 8) |
                       (static_cast<uint32_t>(program[cpu.get_pc() + 3]) << 16) |
                       (static_cast<uint32_t>(program[cpu.get_pc() + 4]) << 24);
        
        // Simple validation - check if address is within program bounds
        if (addr >= program.size()) {
            std::string context = fmt::format("Jump target: 0x{:X}, program size: 0x{:X}", addr, program.size());
            std::string message = "Invalid jump address: target exceeds program bounds";
            ErrorHandler::instance().report_runtime(ErrorCode::CPU_INVALID_JUMP, message, cpu.get_pc(), context);
            running = false;
            throw CPUException(message);
        }
        cpu.set_pc(addr);
    } else {
        running = false;
    }
    cpu.print_state("JMP");
}

// Implementation from jns.cpp
void handle_jns(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 4 < program.size()) {
        uint32_t addr = static_cast<uint32_t>(program[pc + 1]) |
                       (static_cast<uint32_t>(program[pc + 2]) << 8) |
                       (static_cast<uint32_t>(program[pc + 3]) << 16) |
                       (static_cast<uint32_t>(program[pc + 4]) << 24);

        if (!(cpu.get_flags() & FLAG_SIGN)) {
            // TODO: Add valid instruction start validation
            cpu.set_pc(addr);
        } else {
            cpu.set_pc(pc + 5);
        }
    } else {
        running = false;
    }

    cpu.print_state("JNS");
}

// Implementation from jnz.cpp
void handle_jnz(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 4 < program.size()) {
        uint32_t addr = static_cast<uint32_t>(program[pc + 1]) |
                       (static_cast<uint32_t>(program[pc + 2]) << 8) |
                       (static_cast<uint32_t>(program[pc + 3]) << 16) |
                       (static_cast<uint32_t>(program[pc + 4]) << 24);

        if (!(cpu.get_flags() & FLAG_ZERO)) {
            // TODO: Add valid instruction start validation
            cpu.set_pc(addr);
        } else {
            cpu.set_pc(pc + 5);
        }
    } else {
        running = false;
    }

    cpu.print_state("JNZ");
}

// Implementation from js.cpp
void handle_js(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 4 < program.size()) {
        uint32_t addr = static_cast<uint32_t>(program[pc + 1]) |
                       (static_cast<uint32_t>(program[pc + 2]) << 8) |
                       (static_cast<uint32_t>(program[pc + 3]) << 16) |
                       (static_cast<uint32_t>(program[pc + 4]) << 24);

        if (cpu.get_flags() & FLAG_SIGN) {
            // Simple validation - check if address is within program bounds
            if (addr >= program.size()) {
                std::string context = fmt::format("Jump target: 0x{:X}, program size: 0x{:X}", addr, program.size());
                std::string message = "Invalid jump address in JS instruction: target exceeds program bounds";
                ErrorHandler::instance().report_runtime(ErrorCode::CPU_INVALID_JUMP, message, cpu.get_pc(), context);
                running = false;
                throw CPUException(message);
            }
            cpu.set_pc(addr);
        } else {
            cpu.set_pc(pc + 5);
        }
    } else {
        running = false;
    }

    cpu.print_state("JS");
}

// Implementation from jz.cpp
void handle_jz(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 4 < program.size()) {
        uint32_t addr = static_cast<uint32_t>(program[cpu.get_pc() + 1]) |
                       (static_cast<uint32_t>(program[cpu.get_pc() + 2]) << 8) |
                       (static_cast<uint32_t>(program[cpu.get_pc() + 3]) << 16) |
                       (static_cast<uint32_t>(program[cpu.get_pc() + 4]) << 24);
        
        if (cpu.get_flags() & FLAG_ZERO) {
            // Simple validation - check if address is within program bounds
            if (addr >= program.size()) {
                std::string context = fmt::format("Jump target: 0x{:X}, program size: 0x{:X}", addr, program.size());
                std::string message = "Invalid jump address in JZ instruction: target exceeds program bounds";
                ErrorHandler::instance().report_runtime(ErrorCode::CPU_INVALID_JUMP, message, cpu.get_pc(), context);
                running = false;
                throw CPUException(message);
            }
            cpu.set_pc(addr);
        } else {
            cpu.set_pc(cpu.get_pc() + 5);
        }
    } else {
        running = false;
    }
    cpu.print_state("JZ");
}

// Implementation from load.cpp
void handle_load(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    // LOAD reg, addr
    // Supports variable address size based on CPU mode (4 bytes for 32-bit, 8 bytes for 64-bit)
    size_t addr_size = cpu.get_address_size();
    
    if (cpu.get_pc() + 1 + addr_size < program.size()) {
        uint8_t reg = program[cpu.get_pc() + 1];
        
        // Read address using helper that respects CPU mode
        uint64_t addr = cpu.read_address_from_program(program, cpu.get_pc() + 2);
        
        // Check register bounds
        if (reg >= DemiEngine_Registers::TOTAL_REGISTERS) {
            std::string context = fmt::format("Register R{} out of range (0-{})", reg, DemiEngine_Registers::TOTAL_REGISTERS - 1);
            std::string message = "Invalid register in LOAD instruction";
            ErrorHandler::instance().report_runtime(ErrorCode::CPU_INVALID_REGISTER, message, cpu.get_pc(), context);
            running = false;
            return;
        }
        // Check memory bounds
        if (addr >= cpu.get_memory().size()) {
            std::string context = fmt::format("Attempted read at 0x{:X}, memory range: 0x0000-0x{:X}", addr, cpu.get_memory().size() - 1);
            std::string message = "Memory read out of bounds in LOAD instruction";
            ErrorHandler::instance().report_runtime(ErrorCode::CPU_MEMORY_OUT_OF_BOUNDS, message, cpu.get_pc(), context);
            running = false;
            return;
        }
        // Debug-only additional validation
        cpu.validate_memory_read(static_cast<uint32_t>(addr), 1);
        
        // Load value from memory address into destination register
        cpu.set_register_mode_aware(static_cast<Register>(reg), cpu.get_memory()[addr]);
        
        cpu.set_pc(cpu.get_pc() + 2 + addr_size);
    } else {
        running = false;
    }
    cpu.print_state("LOAD");
}

// Implementation of LOADR - Load value from memory to register (indirect addressing)
void handle_loadr(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    // LOADR dest_reg, addr_reg
    // Load value from memory address stored in addr_reg into dest_reg
    if (cpu.get_pc() + 2 < program.size()) {
        uint8_t dest_reg = program[cpu.get_pc() + 1];
        uint8_t addr_reg = program[cpu.get_pc() + 2];
        
        // Check destination register bounds (use 64-bit register array for extended support)
        if (dest_reg >= DemiEngine_Registers::TOTAL_REGISTERS) {
            std::string context = fmt::format("Register R{} out of range (0-{})", dest_reg, DemiEngine_Registers::TOTAL_REGISTERS - 1);
            std::string message = "Invalid destination register in LOADR instruction";
            ErrorHandler::instance().report_runtime(ErrorCode::CPU_INVALID_REGISTER, message, cpu.get_pc(), context);
            running = false;
            return;
        }
        
        // Check address register bounds
        if (addr_reg >= DemiEngine_Registers::TOTAL_REGISTERS) {
            std::string context = fmt::format("Register R{} out of range (0-{})", addr_reg, DemiEngine_Registers::TOTAL_REGISTERS - 1);
            std::string message = "Invalid address register in LOADR instruction";
            ErrorHandler::instance().report_runtime(ErrorCode::CPU_INVALID_REGISTER, message, cpu.get_pc(), context);
            running = false;
            return;
        }
        
        // Get the address from the address register
        uint32_t addr = cpu.get_registers()[addr_reg];
        
        // Check memory bounds
        if (addr >= cpu.get_memory().size()) {
            std::string context = fmt::format("Attempted read at 0x{:X}, memory range: 0x0000-0x{:X}", addr, cpu.get_memory().size() - 1);
            std::string message = "Memory read out of bounds in LOADR instruction";
            ErrorHandler::instance().report_runtime(ErrorCode::CPU_MEMORY_OUT_OF_BOUNDS, message, cpu.get_pc(), context);
            running = false;
            return;
        }
        // Debug-only additional validation
        cpu.validate_memory_read(addr, 1);
        
        // Load value from memory address into destination register
        // LOADR loads a single byte, so we need to zero-extend it properly
        uint8_t byte_value = cpu.get_memory()[addr];
        cpu.set_register_mode_aware(static_cast<Register>(dest_reg), static_cast<uint32_t>(byte_value));
        cpu.set_pc(cpu.get_pc() + 3);
        
        DebugHandler::instance().report(DebugCategory::MEM_ACCESS, fmt::format(
            "[PC=0x{:04X}] [LOADR] R{} = memory[R{}] = memory[0x{:X}] = 0x{:02X}", 
            cpu.get_pc() - 3, dest_reg, addr_reg, addr, cpu.get_memory()[addr]
        ), DebugLevel::DETAIL);
    } else {
        running = false;
    }
    cpu.print_state("LOADR");
}

// Implementation of STORER - Store value from register to memory (indirect addressing)
void handle_storer(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    // STORER addr_reg, value_reg
    // Store value from value_reg into memory address stored in addr_reg
    if (cpu.get_pc() + 2 < program.size()) {
        uint8_t addr_reg = program[cpu.get_pc() + 1];
        uint8_t value_reg = program[cpu.get_pc() + 2];
        
        // Check address register bounds
        if (addr_reg >= DemiEngine_Registers::TOTAL_REGISTERS) {
            std::string context = fmt::format("Register R{} out of range (0-{})", addr_reg, DemiEngine_Registers::TOTAL_REGISTERS - 1);
            std::string message = "Invalid address register in STORER instruction";
            ErrorHandler::instance().report_runtime(ErrorCode::CPU_INVALID_REGISTER, message, cpu.get_pc(), context);
            running = false;
            return;
        }

        // Check value register bounds
        if (value_reg >= DemiEngine_Registers::TOTAL_REGISTERS) {
            std::string context = fmt::format("Register R{} out of range (0-{})", value_reg, DemiEngine_Registers::TOTAL_REGISTERS - 1);
            std::string message = "Invalid value register in STORER instruction";
            ErrorHandler::instance().report_runtime(ErrorCode::CPU_INVALID_REGISTER, message, cpu.get_pc(), context);
            running = false;
            return;
        }
        
        // Get the address from the address register
        uint32_t addr = cpu.get_registers()[addr_reg];
        
        // Check memory bounds
        if (addr >= cpu.get_memory().size()) {
            std::string context = fmt::format("Attempted write at 0x{:X}, memory range: 0x0000-0x{:X}", addr, cpu.get_memory().size() - 1);
            std::string message = "Memory write out of bounds in STORER instruction";
            ErrorHandler::instance().report_runtime(ErrorCode::CPU_MEMORY_OUT_OF_BOUNDS, message, cpu.get_pc(), context);
            running = false;
            return;
        }
        // Debug-only additional validation
        cpu.validate_memory_write(addr, 1);
        
        // Store value from value_reg into memory address
        cpu.get_memory()[addr] = static_cast<uint8_t>(cpu.get_register_mode_aware(static_cast<Register>(value_reg)) & 0xFF);
        cpu.set_pc(cpu.get_pc() + 3);
        
        DebugHandler::instance().report(DebugCategory::MEM_ACCESS, fmt::format(
            "[PC=0x{:04X}] [STORER] memory[R{}] = memory[0x{:X}] = R{} = 0x{:02X}", 
            cpu.get_pc() - 3, addr_reg, addr, value_reg, cpu.get_memory()[addr]
        ), DebugLevel::DETAIL);
    } else {
        running = false;
    }
    cpu.print_state("STORER");
}

// Implementation from load_imm.cpp
void handle_load_imm(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    // LOAD_IMM always uses 4-byte immediate for all registers (6 bytes total)
    // This matches the current assembler behavior
    
    if (cpu.get_pc() + 5 >= program.size()) {
        std::string message = "Unexpected end of program reading LOAD_IMM";
        ErrorHandler::instance().report_runtime(ErrorCode::CPU_MEMORY_OUT_OF_BOUNDS, message, cpu.get_pc(), "");
        running = false;
        return;
    }
    
    uint8_t reg = program[cpu.get_pc() + 1];
    
    if (reg < cpu.get_registers_64().size()) {
        uint32_t value = static_cast<uint32_t>(program[cpu.get_pc() + 2]) |
                        (static_cast<uint32_t>(program[cpu.get_pc() + 3]) << 8) |
                        (static_cast<uint32_t>(program[cpu.get_pc() + 4]) << 16) |
                        (static_cast<uint32_t>(program[cpu.get_pc() + 5]) << 24);
        
        DEBUG_INSTRUCTION("LOAD_IMM", cpu.get_pc(), fmt::format("R{}, 0x{:08X}", reg, value), "");
        
        cpu.set_register_mode_aware(static_cast<Register>(reg), value);
        cpu.sync_legacy_registers();
        cpu.set_pc(cpu.get_pc() + 6);
    } else {
        running = false;
    }
    
    cpu.print_state("LOAD_IMM");
}

// Implementation for LEA (Load Effective Address)
void handle_lea(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    // LEA always uses 32-bit addresses (4 bytes) regardless of CPU mode
    const size_t addr_size = 4;
    
    if (cpu.get_pc() + 1 + addr_size < program.size()) {
        uint8_t reg = program[cpu.get_pc() + 1];
        
        // Read 32-bit address
        uint32_t addr = 0;
        for (size_t i = 0; i < 4; i++) {
            addr |= (static_cast<uint32_t>(program[cpu.get_pc() + 2 + i]) << (i * 8));
        }
        
        DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
            "[PC=0x{:04X}] [LEA] PC={} Loading address 0x{:X} into R{}",
            cpu.get_pc(), cpu.get_pc(), addr, reg
        ), DebugLevel::DETAIL);
        
        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            cpu.set_register_mode_aware(static_cast<Register>(reg), static_cast<uint64_t>(addr));  // Load the address itself
            DebugHandler::instance().report(DebugCategory::CPU_REGISTERS, fmt::format(
                "[PC=0x{:04X}] [LEA] R{} = 0x{:X} (address)",
                cpu.get_pc(), reg, addr
            ), DebugLevel::DETAIL);
        }
        cpu.set_pc(cpu.get_pc() + 2 + addr_size);
    } else {
        running = false;
    }
    cpu.print_state("LEA");
}

// Implementation from mov.cpp
void handle_mov(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 < program.size()) {
        uint8_t reg1 = program[cpu.get_pc() + 1];
        uint8_t reg2 = program[cpu.get_pc() + 2];
        if (cpu.is_valid_register(static_cast<Register>(reg1)) && cpu.is_valid_register(static_cast<Register>(reg2))) {
            uint64_t value = cpu.get_register_mode_aware(static_cast<Register>(reg2));
            cpu.set_register_mode_aware(static_cast<Register>(reg1), value);
        }
        cpu.set_pc(cpu.get_pc() + 3);
    } else {
        running = false;
    }
    cpu.print_state("MOV");
}

// Implementation from mul.cpp
void handle_mul(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 < program.size()) {
        uint8_t reg1 = program[cpu.get_pc() + 1];
        uint8_t reg2 = program[cpu.get_pc() + 2];
        DEBUG_INSTRUCTION("MUL", cpu.get_pc(), fmt::format("R{} *= R{}", reg1, reg2), "");
        if (reg1 < DemiEngine_Registers::TOTAL_REGISTERS && reg2 < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val1 = cpu.get_register_mode_aware(static_cast<Register>(reg1));
            uint64_t val2 = cpu.get_register_mode_aware(static_cast<Register>(reg2));

            // Use 128-bit arithmetic for full precision with compiler intrinsic or portable fallback
#if defined(__GNUC__) || defined(__clang__)
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wpedantic"
            unsigned __int128 result128 = static_cast<unsigned __int128>(val1) * static_cast<unsigned __int128>(val2);
            uint64_t result = static_cast<uint64_t>(result128);
            #pragma GCC diagnostic pop
#else
            // Portable fallback for non-GCC compilers
            uint64_t result = val1 * val2; // May overflow, but that's expected behavior
#endif
            uint64_t mask = cpu.get_operand_mask();
            uint64_t masked_result = result & mask;

            uint32_t current_flags = cpu.get_flags();

            // Zero Flag
            if (masked_result == 0) {
                current_flags |= FLAG_ZERO;
            } else {
                current_flags &= ~FLAG_ZERO;
            }

            // Carry and Overflow Flags
            // MUL sets CF and OF if the upper half of the result is non-zero
            bool overflow = false;
            if (cpu.is_32bit_mode()) {
                overflow = (result128 > 0xFFFFFFFF);
            } else {
                overflow = (result128 > 0xFFFFFFFFFFFFFFFFULL);
            }

            if (overflow) {
                current_flags |= FLAG_CARRY;
                current_flags |= FLAG_OVERFLOW;
            } else {
                current_flags &= ~FLAG_CARRY;
                current_flags &= ~FLAG_OVERFLOW;
            }

            cpu.set_flags(current_flags);
            cpu.set_register_mode_aware(static_cast<Register>(reg1), result);
            DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
                "[PC=0x{:04X}] [MUL] R{}: {} * {} = {} (carry={}, overflow={})",
                cpu.get_pc(), reg1, val1, val2, result,
                (cpu.get_flags() & FLAG_CARRY) ? 1 : 0,
                (cpu.get_flags() & FLAG_OVERFLOW) ? 1 : 0
            ), DebugLevel::DETAIL);
        }
        cpu.set_pc(cpu.get_pc() + 3);
    } else {
        running = false;
    }
    cpu.print_state("MUL");
}

// Implementation from nop.cpp
void handle_nop(CPU& cpu, [[maybe_unused]] const std::vector<uint8_t>& program, [[maybe_unused]] bool& running) {
    DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
        "[PC=0x{:04X}] [NOP] PC={}",
        cpu.get_pc(), cpu.get_pc()
    ), DebugLevel::DETAIL);
    cpu.set_pc(cpu.get_pc() + 1);
    cpu.print_state("NOP");
}

// Implementation from not.cpp
void handle_not(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 1 < program.size()) {
        uint8_t reg = program[pc + 1];

        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val = cpu.get_register_mode_aware(static_cast<Register>(reg));
            uint64_t result = ~val;
            cpu.set_register_mode_aware(static_cast<Register>(reg), result);
        }

        cpu.set_pc(pc + 2);
    } else {
        running = false;
    }

    cpu.print_state("NOT");
}

// Implementation from or.cpp
void handle_or(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg1 = program[pc + 1];
        uint8_t reg2 = program[pc + 2];

        if (reg1 < DemiEngine_Registers::TOTAL_REGISTERS && reg2 < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val1 = cpu.get_register_mode_aware(static_cast<Register>(reg1));
            uint64_t val2 = cpu.get_register_mode_aware(static_cast<Register>(reg2));
            uint64_t result = val1 | val2;
            uint64_t mask = cpu.get_operand_mask();
            uint64_t masked_result = result & mask;

            // Flags
            uint32_t current_flags = cpu.get_flags();
            if (masked_result == 0) {
                current_flags |= FLAG_ZERO;
            } else {
                current_flags &= ~FLAG_ZERO;
            }
            // OR clears Carry and Overflow
            current_flags &= ~FLAG_CARRY;
            current_flags &= ~FLAG_OVERFLOW;
            
            cpu.set_flags(current_flags);
            cpu.set_register_mode_aware(static_cast<Register>(reg1), result);
        }

        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }

    cpu.print_state("OR");
}

// Implementation from outb.cpp
void handle_outb(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg = program[pc + 1];
        uint8_t port = program[pc + 2];

        DebugHandler::instance().report(DebugCategory::IO_DEVICE, fmt::format(
            "[PC=0x{:04X}] [OUTB] PC={} port {} <- R{}={}",
            pc, pc, port, reg, cpu.get_registers()[reg]
        ), DebugLevel::DETAIL);

        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            cpu.write_port(port, cpu.get_registers()[reg]);
        }

        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }

    cpu.print_state("OUTB");
}

// Implementation from out.cpp
void handle_out(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg = program[pc + 1];
        uint8_t port = program[pc + 2];

        DebugHandler::instance().report(DebugCategory::IO_DEVICE, fmt::format(
            "[PC=0x{:04X}] [OUT] PC={} port {} <- R{}={}",
            pc, pc, port, reg, cpu.get_registers()[reg]
        ), DebugLevel::DETAIL);

        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            cpu.write_port(port, cpu.get_registers()[reg]);
        }

        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }

    cpu.print_state("OUT");
}

// Implementation from outl.cpp
void handle_outl(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg = program[pc + 1];
        uint8_t port = program[pc + 2];

        DebugHandler::instance().report(DebugCategory::IO_DEVICE, fmt::format(
            "[PC=0x{:04X}] [OUTL] PC={} port {} <- R{} (dword)",
            pc, pc, port, reg
        ), DebugLevel::DETAIL);

        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint32_t value = 0;
            for (size_t i = 0; i < 4 && (reg + i) < DemiEngine_Registers::TOTAL_REGISTERS; ++i) {
                value |= (static_cast<uint32_t>(cpu.get_registers()[reg + i]) << (8 * i));
            }
            cpu.write_port_dword(port, value);
        }

        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }

    cpu.print_state("OUTL");
}

// Implementation from outstr.cpp
void handle_outstr(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg = program[pc + 1];
        uint8_t port = program[pc + 2];

        DebugHandler::instance().report(DebugCategory::IO_DEVICE, fmt::format(
            "[PC=0x{:04X}] [OUTSTR] PC={} port {} <- string from memory",
            pc, pc, port
        ), DebugLevel::DETAIL);

        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            // Build string from memory starting at address in register
            uint32_t addr = cpu.get_register_32(static_cast<Register>(reg));
            std::string str;
            for (size_t i = addr; i < cpu.get_memory().size() && cpu.get_memory()[i] != 0; ++i) {
                str += static_cast<char>(cpu.get_memory()[i]);
            }

            DebugHandler::instance().report(DebugCategory::IO_DEVICE, fmt::format(
                "[PC=0x{:04X}] [OUTSTR] Writing string: '{}'",
                pc, str
            ), DebugLevel::DETAIL);

            cpu.write_port_string(port, str);
        }

        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }

    cpu.print_state("OUTSTR");
}

// Implementation from outw.cpp
void handle_outw(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg = program[pc + 1];
        uint8_t port = program[pc + 2];

        DebugHandler::instance().report(DebugCategory::IO_DEVICE, fmt::format(
            "[PC=0x{:04X}] [OUTW] PC={} port {} <- R{} (word)",
            pc, pc, port, reg
        ), DebugLevel::DETAIL);

        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint16_t value = cpu.get_registers()[reg];
            if (static_cast<size_t>(reg + 1) < DemiEngine_Registers::TOTAL_REGISTERS) {
                value |= (static_cast<uint16_t>(cpu.get_registers()[reg + 1]) << 8);
            }
            cpu.write_port_word(port, value);
        }

        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }

    cpu.print_state("OUTW");
}

// Implementation from pop_arg.cpp
void handle_pop_arg(CPU& cpu, [[maybe_unused]] const std::vector<uint8_t>& program, [[maybe_unused]] bool& running) {
    uint32_t pc = cpu.get_pc();
    uint8_t reg = cpu.fetch_operand();

    // Check if we're in a function call context by checking if arg_offset has been set
    // In function context, arg_offset is set to 8 by CALL
    // In standalone context, arg_offset remains 0 (initialized value)
    if (cpu.get_arg_offset() > 0) {
        // Function context: use frame pointer + offset
        cpu.get_registers()[reg] = cpu.read_mem32(cpu.get_fp() + cpu.get_arg_offset());

        DebugHandler::instance().report(DebugCategory::CPU_STACK, fmt::format(
            "[PC=0x{:04X}] [POP_ARG] Function context: FP={} arg_offset={} addr={} value={}",
            pc, cpu.get_fp(), cpu.get_arg_offset(),
            (cpu.get_fp() + cpu.get_arg_offset()), cpu.get_registers()[reg]
        ), DebugLevel::DETAIL);

        cpu.set_arg_offset(cpu.get_arg_offset() + 4);
    } else {
        // Standalone context: pop from stack like regular POP
        // Check for stack underflow
        if (cpu.get_sp() >= cpu.get_memory().size()) {
            std::string context = fmt::format("Stack pointer: 0x{:X}, memory size: 0x{:X}", cpu.get_sp(), cpu.get_memory().size());
            std::string message = "Stack underflow in POP_ARG instruction";
            ErrorHandler::instance().report_runtime(ErrorCode::CPU_STACK_UNDERFLOW, message, pc, context);
            running = false;
            throw CPUException(message);
        }
        
        cpu.set_register_mode_aware(static_cast<Register>(reg), cpu.read_mem32(cpu.get_sp()));
        cpu.set_sp(cpu.get_sp() + 4);

        DebugHandler::instance().report(DebugCategory::CPU_STACK, fmt::format(
            "[PC=0x{:04X}] [POP_ARG] Standalone context: popped from SP={} value={}",
            pc, cpu.get_sp() - 4, cpu.get_register_mode_aware(static_cast<Register>(reg))
        ), DebugLevel::DETAIL);
    }

    // Don't set PC - fetch_operand already advanced it
    cpu.print_state("POP_ARG");
}

// Implementation from pop.cpp
void handle_pop(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 1 < program.size()) {
        uint8_t reg = program[cpu.get_pc() + 1];
        
        // Check for stack underflow (SP at or beyond initial position)
        // Stack starts at memory.size() - 4, so if SP >= memory.size(), stack is empty
        if (cpu.get_sp() >= cpu.get_memory().size()) {
            std::string context = fmt::format("Stack pointer: 0x{:X}, memory size: 0x{:X}", cpu.get_sp(), cpu.get_memory().size());
            std::string message = "Stack underflow: not enough data to POP";
            ErrorHandler::instance().report_runtime(ErrorCode::CPU_STACK_UNDERFLOW, message, cpu.get_pc(), context);
            running = false;
            throw CPUException(message);
        }

#ifndef NDEBUG
        if (!cpu.validate_stack_pop(4)) {
            running = false;
            throw CPUException("Stack underflow during POP");
        }
#endif
        
        uint32_t value = cpu.read_mem32(cpu.get_sp());
        if (!cpu.is_valid_register(static_cast<Register>(reg))) {
            std::string context = fmt::format("Register R{} out of range", reg);
            std::string message = "Invalid register in POP instruction";
            ErrorHandler::instance().report_runtime(ErrorCode::CPU_INVALID_REGISTER, message, cpu.get_pc(), context);
            running = false;
            throw CPUException(message);
        }
        cpu.set_register_mode_aware(static_cast<Register>(reg), value);
        DebugHandler::instance().report(DebugCategory::CPU_STACK, fmt::format(
            "[PC=0x{:04X}] [POP] PC={} Popping to R{}={}",
            cpu.get_pc(), cpu.get_pc(), static_cast<int>(reg), value
        ), DebugLevel::DETAIL);
        cpu.set_sp(cpu.get_sp() + 4);
        cpu.get_registers()[4] = cpu.get_sp();
        cpu.set_pc(cpu.get_pc() + 2);
    } else {
        running = false;
    }
    cpu.print_state("POP");
}

// Implementation from pop_flag.cpp
void handle_pop_flag(CPU& cpu, [[maybe_unused]] const std::vector<uint8_t>& program, [[maybe_unused]] bool& running) {
    // Check for stack underflow
    if (cpu.get_sp() >= cpu.get_memory().size()) {
        std::string context = fmt::format("Stack pointer: 0x{:X}, memory size: 0x{:X}", cpu.get_sp(), cpu.get_memory().size());
        std::string message = "Stack underflow: cannot pop flags";
        ErrorHandler::instance().report_runtime(ErrorCode::CPU_STACK_UNDERFLOW, message, cpu.get_pc(), context);
        running = false;
        throw CPUException(message);
    }
    
    DebugHandler::instance().report(DebugCategory::CPU_STACK, fmt::format(
        "[PC=0x{:04X}] [POPF] PC={} Popping FLAGS={:08X}",
        cpu.get_pc(), cpu.get_pc(), cpu.get_flags()
    ), DebugLevel::DETAIL);
    cpu.set_flags(cpu.read_mem32(cpu.get_sp()));
    cpu.set_sp(cpu.get_sp() + 4);
    cpu.set_pc(cpu.get_pc() + 1);  // POP_FLAG is a single-byte instruction
    cpu.print_state("POPF");
}

// Implementation from push_arg.cpp
void handle_push_arg(CPU& cpu, [[maybe_unused]] const std::vector<uint8_t>& program, [[maybe_unused]] bool& running) {
    uint32_t pc = cpu.get_pc();
    uint8_t reg = cpu.fetch_operand();

    // Check for stack overflow (SP going below reasonable minimum)
    if (cpu.get_sp() < 4) {
        std::string context = fmt::format("Stack pointer: 0x{:X}, minimum safe SP: 0x0004", cpu.get_sp());
        std::string message = "Stack overflow during PUSH_ARG: insufficient space";
        ErrorHandler::instance().report_runtime(ErrorCode::CPU_STACK_OVERFLOW, message, pc, context);
        running = false;
        throw CPUException(message);
    }

    DebugHandler::instance().report(DebugCategory::CPU_STACK, fmt::format(
        "[PC=0x{:04X}] [PUSH_ARG] SP={} Pushing R{}={}",
        pc, cpu.get_sp(), static_cast<int>(reg), cpu.get_registers()[reg]
    ), DebugLevel::DETAIL);

    uint32_t sp = cpu.get_sp() - 4;
    cpu.set_sp(sp);
    cpu.get_registers()[4] = sp;
    cpu.write_mem32(sp, cpu.get_registers()[reg]);

    // Don't set PC - fetch_operand already advanced it
    cpu.print_state("PUSH_ARG");
}

// Implementation from push.cpp
void handle_push(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 1 < program.size()) {
        uint8_t reg = program[cpu.get_pc() + 1];
        if (!cpu.is_valid_register(static_cast<Register>(reg))) {
            std::string context = fmt::format("Register R{} out of range", reg);
            std::string message = "Invalid register in PUSH instruction";
            ErrorHandler::instance().report_runtime(ErrorCode::CPU_INVALID_REGISTER, message, cpu.get_pc(), context);
            running = false;
            throw CPUException(message);
        }

        uint32_t value = static_cast<uint32_t>(cpu.get_register_mode_aware(static_cast<Register>(reg)) & 0xFFFFFFFFU);
        DebugHandler::instance().report(DebugCategory::CPU_STACK, fmt::format(
            "[PC=0x{:04X}] [PUSH] PC={} Pushing R{}={}",
            cpu.get_pc(), cpu.get_pc(), static_cast<int>(reg), value
        ), DebugLevel::DETAIL);
        
        // Check for stack overflow (SP going below reasonable minimum)
        if (cpu.get_sp() < 8) {
            std::string context = fmt::format("Stack pointer: 0x{:X}, minimum safe SP: 0x0008", cpu.get_sp());
            std::string message = "Stack overflow during PUSH: insufficient space";
            ErrorHandler::instance().report_runtime(ErrorCode::CPU_STACK_OVERFLOW, message, cpu.get_pc(), context);
            running = false;
            throw CPUException(message);
        }

#ifndef NDEBUG
        if (!cpu.validate_stack_push(4)) {
            running = false;
            throw CPUException("Stack overflow during PUSH");
        }
#endif
        
        cpu.set_sp(cpu.get_sp() - 4);
        // Sync only SP to legacy R4 (don't overwrite other legacy registers)
        cpu.get_registers()[4] = cpu.get_sp();
        cpu.write_mem32(cpu.get_sp(), value);
        cpu.set_pc(cpu.get_pc() + 2);
    } else {
        running = false;
    }
    cpu.print_state("PUSH");
}

// Implementation from push_flag.cpp
void handle_push_flag(CPU& cpu, [[maybe_unused]] const std::vector<uint8_t>& program, [[maybe_unused]] bool& running) {
    uint32_t pc = cpu.get_pc();

    DebugHandler::instance().report(DebugCategory::CPU_STACK, fmt::format(
        "[PC=0x{:04X}] [PUSHF] PC={} Pushing FLAGS={:08X}",
        pc, pc, cpu.get_flags()
    ), DebugLevel::DETAIL);

    // Check for stack overflow (SP going below reasonable minimum)
    if (cpu.get_sp() < 4) {
        std::string context = fmt::format("Stack pointer: 0x{:X}, minimum safe SP: 0x0004", cpu.get_sp());
        std::string message = "Stack overflow during PUSH_FLAG: insufficient space";
        ErrorHandler::instance().report_runtime(ErrorCode::CPU_STACK_OVERFLOW, message, pc, context);
        running = false;
        throw CPUException(message);
    }

    cpu.set_sp(cpu.get_sp() - 4);
    cpu.write_mem32(cpu.get_sp(), cpu.get_flags());
    cpu.set_pc(cpu.get_pc() + 1);  // PUSH_FLAG is a single-byte instruction
    cpu.print_state("PUSHF");
}

// Implementation from ret.cpp
void handle_ret(CPU& cpu, [[maybe_unused]] const std::vector<uint8_t>& program, [[maybe_unused]] bool& running) {
    uint32_t pc = cpu.get_pc();
    uint32_t sp = cpu.get_sp();

    DebugHandler::instance().report(DebugCategory::CPU_STACK, fmt::format(
        "[PC=0x{:04X}] [RET] SP={} Restoring FP and popping return address",
        pc, sp
    ), DebugLevel::DETAIL);

    // Check if RET is being called without a matching CALL
    // SP should be well below memory size if there's a valid call frame
    if (sp + 8 > cpu.get_memory().size()) {
        std::string context = fmt::format("Stack pointer: 0x{:X}, memory size: 0x{:X}", sp, cpu.get_memory().size());
        std::string message = "Invalid RET: stack underflow or no matching CALL";
        ErrorHandler::instance().report_runtime(ErrorCode::CPU_STACK_UNDERFLOW, message, pc, context);
        running = false;
        throw CPUException(message);
    }

#ifndef NDEBUG
    if (!cpu.validate_stack_pop(8)) {
        running = false;
        throw CPUException("Stack underflow during RET");
    }
#endif

    // Stack layout from CALL:
    // SP: return address
    // SP+4: old frame pointer
    uint32_t ret_addr = cpu.read_mem32(sp);      // return address at sp
    uint32_t old_fp = cpu.read_mem32(sp + 4);    // old frame pointer at sp+4

    // Unwind stack (pop 8 bytes: return address + old FP)
    sp += 8;
    cpu.set_sp(sp);
    cpu.get_registers()[4] = sp;
    cpu.set_fp(old_fp);
    
    // Decrement call depth
    cpu.decrement_call_depth();

    cpu.print_stack_frame("RET");
    cpu.set_pc(ret_addr);

    // Reset offset at each return
    cpu.set_arg_offset(0);

    cpu.print_state("RET");
}

// Implementation from shl.cpp
void handle_shl(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg = program[pc + 1];
        uint8_t imm = program[pc + 2];

        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val = cpu.get_register_mode_aware(static_cast<Register>(reg));
            uint64_t result = val << imm;
            uint64_t mask = cpu.get_operand_mask();
            uint64_t masked_result = result & mask;
            
            // Flags
            uint32_t current_flags = cpu.get_flags();
            if (masked_result == 0) {
                current_flags |= FLAG_ZERO;
            } else {
                current_flags &= ~FLAG_ZERO;
            }
            
            // Carry Flag (last bit shifted out)
            if (imm > 0) {
                bool carry = false;
                if (cpu.is_32bit_mode()) {
                    if (imm <= 32) {
                        carry = (val & (1ULL << (32 - imm))) != 0;
                    }
                } else {
                    if (imm <= 64) {
                        carry = (val & (1ULL << (64 - imm))) != 0;
                    }
                }
                if (carry) {
                    current_flags |= FLAG_CARRY;
                } else {
                    current_flags &= ~FLAG_CARRY;
                }
            }
            
            // Overflow Flag (only for 1-bit shifts)
            if (imm == 1) {
                bool msb_before, msb_after;
                if (cpu.is_32bit_mode()) {
                    msb_before = (val & 0x80000000) != 0;
                    msb_after = (masked_result & 0x80000000) != 0;
                } else {
                    msb_before = (val & 0x8000000000000000ULL) != 0;
                    msb_after = (masked_result & 0x8000000000000000ULL) != 0;
                }
                if (msb_before != msb_after) {
                    current_flags |= FLAG_OVERFLOW;
                } else {
                    current_flags &= ~FLAG_OVERFLOW;
                }
            }
            
            cpu.set_flags(current_flags);
            cpu.set_register_mode_aware(static_cast<Register>(reg), result);
        }

        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }

    cpu.print_state("SHL");
}

// Implementation from shr.cpp
void handle_shr(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg = program[pc + 1];
        uint8_t imm = program[pc + 2];

        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val = cpu.get_register_mode_aware(static_cast<Register>(reg));
            uint64_t result = val >> imm;
            
            // Flags
            uint32_t current_flags = cpu.get_flags();
            if (result == 0) {
                current_flags |= FLAG_ZERO;
            } else {
                current_flags &= ~FLAG_ZERO;
            }
            
            // Carry Flag (last bit shifted out)
            if (imm > 0) {
                bool carry = (val & (1ULL << (imm - 1))) != 0;
                if (carry) {
                    current_flags |= FLAG_CARRY;
                } else {
                    current_flags &= ~FLAG_CARRY;
                }
            }
            
            // Overflow Flag (only for 1-bit shifts)
            if (imm == 1) {
                bool msb;
                if (cpu.is_32bit_mode()) {
                    msb = (val & 0x80000000) != 0;
                } else {
                    msb = (val & 0x8000000000000000ULL) != 0;
                }
                if (msb) {
                    current_flags |= FLAG_OVERFLOW;
                } else {
                    current_flags &= ~FLAG_OVERFLOW;
                }
            }
            
            cpu.set_flags(current_flags);
            cpu.set_register_mode_aware(static_cast<Register>(reg), result);
        }

        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }

    cpu.print_state("SHR");
}

// Implementation from store.cpp
void handle_store(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    // STORE reg, addr
    size_t addr_size = cpu.get_address_size();
    
    if (cpu.get_pc() + 1 + addr_size < program.size()) {
        uint8_t reg = program[cpu.get_pc() + 1];
        uint64_t addr = cpu.read_address_from_program(program, cpu.get_pc() + 2);
        
        // Check register bounds
        if (reg >= DemiEngine_Registers::TOTAL_REGISTERS) {
            std::string context = fmt::format("Register R{} out of range (0-{})", reg, DemiEngine_Registers::TOTAL_REGISTERS - 1);
            std::string message = "Invalid register in STORE instruction";
            ErrorHandler::instance().report_runtime(ErrorCode::CPU_INVALID_REGISTER, message, cpu.get_pc(), context);
            running = false;
            return;
        }
        // Check memory bounds
        if (addr >= cpu.get_memory().size()) {
            std::string context = fmt::format("Attempted write at 0x{:X}, memory range: 0x0000-0x{:X}", addr, cpu.get_memory().size() - 1);
            std::string message = "Memory write out of bounds in STORE instruction";
            ErrorHandler::instance().report_runtime(ErrorCode::CPU_MEMORY_OUT_OF_BOUNDS, message, cpu.get_pc(), context);
            running = false;
            return;
        }
        // Debug-only additional validation
        cpu.validate_memory_write(static_cast<uint32_t>(addr), 1);
        
        // STORE writes the lower byte of the register to memory
        cpu.get_memory()[addr] = static_cast<uint8_t>(cpu.get_register_mode_aware(static_cast<Register>(reg)) & 0xFF);
        cpu.set_pc(cpu.get_pc() + 2 + addr_size);
    } else {
        running = false;
    }
    cpu.print_state("STORE");
}

// Implementation from sub.cpp
void handle_sub(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 < program.size()) {
        uint8_t reg1 = program[cpu.get_pc() + 1];
        uint8_t reg2 = program[cpu.get_pc() + 2];
        DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
            "[PC=0x{:04X}] [SUB] PC={} R{} -= R{}",
            cpu.get_pc(), cpu.get_pc(), reg1, reg2
        ), DebugLevel::DETAIL);
        if (reg1 < DemiEngine_Registers::TOTAL_REGISTERS && reg2 < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val1 = cpu.get_register_mode_aware(static_cast<Register>(reg1));
            uint64_t val2 = cpu.get_register_mode_aware(static_cast<Register>(reg2));
            uint64_t result = val1 - val2;
            uint64_t mask = cpu.get_operand_mask();
            uint64_t masked_result = result & mask;

            uint32_t current_flags = cpu.get_flags();

            // Zero Flag
            if (masked_result == 0) {
                current_flags |= FLAG_ZERO;
            } else {
                current_flags &= ~FLAG_ZERO;
            }

            // Carry Flag (Borrow)
            bool borrow = false;
            if (cpu.is_32bit_mode()) {
                // In 32-bit mode, we compare the 32-bit values
                borrow = (static_cast<uint32_t>(val1) < static_cast<uint32_t>(val2));
            } else {
                borrow = (val1 < val2);
            }

            if (borrow) {
                current_flags |= FLAG_CARRY;
            } else {
                current_flags &= ~FLAG_CARRY;
            }

            // Overflow Flag (Signed)
            bool sign1, sign2, signR;
            if (cpu.is_32bit_mode()) {
                sign1 = (val1 & 0x80000000) != 0;
                sign2 = (val2 & 0x80000000) != 0;
                signR = (masked_result & 0x80000000) != 0;
            } else {
                sign1 = (val1 & 0x8000000000000000) != 0;
                sign2 = (val2 & 0x8000000000000000) != 0;
                signR = (masked_result & 0x8000000000000000) != 0;
            }

            // Overflow occurs when subtracting a positive from a negative yields a positive
            // or subtracting a negative from a positive yields a negative
            // i.e. sign1 != sign2 AND sign1 != signR
            if ((sign1 != sign2) && (sign1 != signR)) {
                current_flags |= FLAG_OVERFLOW;
            } else {
                current_flags &= ~FLAG_OVERFLOW;
            }

            cpu.set_flags(current_flags);
            cpu.set_register_mode_aware(static_cast<Register>(reg1), result);
            DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
                "[PC=0x{:04X}] [SUB] R{}: {} - {} = {} (carry={}, overflow={})",
                cpu.get_pc(), reg1, val1, val2, result,
                (cpu.get_flags() & FLAG_CARRY) ? 1 : 0,
                (cpu.get_flags() & FLAG_OVERFLOW) ? 1 : 0
            ), DebugLevel::DETAIL);
        }
        cpu.set_pc(cpu.get_pc() + 3);
    } else {
        running = false;
    }
    cpu.print_state("SUB");
}

// Implementation for SWAP
void handle_swap(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    // SWAP always uses 32-bit addresses (4 bytes) regardless of CPU mode
    const size_t addr_size = 4;
    
    if (cpu.get_pc() + 1 + addr_size < program.size()) {
        uint8_t reg = program[cpu.get_pc() + 1];
        
        // Read 32-bit address
        uint32_t addr = 0;
        for (size_t i = 0; i < 4; i++) {
            addr |= (static_cast<uint32_t>(program[cpu.get_pc() + 2 + i]) << (i * 8));
        }
        
        DebugHandler::instance().report(DebugCategory::MEM_ACCESS, fmt::format(
            "[PC=0x{:04X}] [SWAP] PC={} Swapping R{} with memory[0x{:X}]",
            cpu.get_pc(), cpu.get_pc(), reg, addr
        ), DebugLevel::DETAIL);

        // Validate register bounds
        if (reg >= DemiEngine_Registers::TOTAL_REGISTERS) {
            std::string context = fmt::format("Register R{} out of range (0-{})", reg, DemiEngine_Registers::TOTAL_REGISTERS - 1);
            std::string message = "Invalid register in SWAP instruction";
            ErrorHandler::instance().report_runtime(ErrorCode::CPU_INVALID_REGISTER, message, cpu.get_pc(), context);
            running = false;
            cpu.set_pc(cpu.get_pc() + 2 + addr_size);
            cpu.print_state("SWAP");
            return;
        }

        // Validate memory bounds
        if (addr >= cpu.get_memory().size()) {
            std::string context = fmt::format("Attempted access at 0x{:X}, memory range: 0x0000-0x{:X}", addr, cpu.get_memory().size() - 1);
            std::string message = "Memory access out of bounds in SWAP instruction";
            ErrorHandler::instance().report_runtime(ErrorCode::CPU_MEMORY_OUT_OF_BOUNDS, message, cpu.get_pc(), context);
            running = false;
            cpu.set_pc(cpu.get_pc() + 2 + addr_size);
            cpu.print_state("SWAP");
            return;
        }
        
        // Debug-only additional validation
        cpu.validate_memory_read(addr, 1);
        cpu.validate_memory_write(addr, 1);

        uint64_t reg_val = cpu.get_register_mode_aware(static_cast<Register>(reg));
        uint8_t mem_val = cpu.get_memory()[addr];
        
        // Swap: reg gets mem byte, mem gets reg lower byte
        cpu.set_register_mode_aware(static_cast<Register>(reg), static_cast<uint64_t>(mem_val));
        cpu.get_memory()[addr] = static_cast<uint8_t>(reg_val & 0xFF);
        
        DebugHandler::instance().report(DebugCategory::MEM_ACCESS, fmt::format(
            "[PC=0x{:04X}] [SWAP] R{} = {}, memory[0x{:X}] = {}",
            cpu.get_pc(), reg, mem_val, addr, static_cast<uint8_t>(reg_val & 0xFF)
        ), DebugLevel::DETAIL);
        cpu.set_pc(cpu.get_pc() + 2 + addr_size);
    } else {
        running = false;
    }
    cpu.print_state("SWAP");
}

// Implementation from xor.cpp
void handle_xor(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg1 = program[pc + 1];
        uint8_t reg2 = program[pc + 2];

        if (reg1 < DemiEngine_Registers::TOTAL_REGISTERS && reg2 < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val1 = cpu.get_register_mode_aware(static_cast<Register>(reg1));
            uint64_t val2 = cpu.get_register_mode_aware(static_cast<Register>(reg2));
            uint64_t result = val1 ^ val2;
            uint64_t mask = cpu.get_operand_mask();
            uint64_t masked_result = result & mask;

            // Flags
            uint32_t current_flags = cpu.get_flags();
            if (masked_result == 0) {
                current_flags |= FLAG_ZERO;
            } else {
                current_flags &= ~FLAG_ZERO;
            }
            // XOR clears Carry and Overflow
            current_flags &= ~FLAG_CARRY;
            current_flags &= ~FLAG_OVERFLOW;
            
            cpu.set_flags(current_flags);
            cpu.set_register_mode_aware(static_cast<Register>(reg1), result);
        }

        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }

    cpu.print_state("XOR");
}

// Implementation for JC (Jump if Carry)
void handle_jc(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 4 < program.size()) {
        uint32_t addr = static_cast<uint32_t>(program[pc + 1]) |
                       (static_cast<uint32_t>(program[pc + 2]) << 8) |
                       (static_cast<uint32_t>(program[pc + 3]) << 16) |
                       (static_cast<uint32_t>(program[pc + 4]) << 24);
        DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
            "[PC=0x{:04X}] [JC] PC={} Checking carry flag",
            pc, pc
        ), DebugLevel::DETAIL);

        if (cpu.get_flags() & FLAG_CARRY) {
            DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
                "[PC=0x{:04X}] [JC] Carry flag set, jumping to address {}",
                pc, addr
            ), DebugLevel::DETAIL);
            cpu.set_pc(addr);
        } else {
            DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
                "[PC=0x{:04X}] [JC] Carry flag clear, continuing",
                pc, pc
            ), DebugLevel::DETAIL);
            cpu.set_pc(pc + 5);
        }
    } else {
        running = false;
    }

    cpu.print_state("JC");
}

// Implementation for JNC (Jump if No Carry)
void handle_jnc(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 4 < program.size()) {
        uint32_t addr = static_cast<uint32_t>(program[pc + 1]) |
                       (static_cast<uint32_t>(program[pc + 2]) << 8) |
                       (static_cast<uint32_t>(program[pc + 3]) << 16) |
                       (static_cast<uint32_t>(program[pc + 4]) << 24);
        DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
            "[PC=0x{:04X}] [JNC] PC={} Checking carry flag",
            pc, pc
        ), DebugLevel::DETAIL);

        if (!(cpu.get_flags() & FLAG_CARRY)) {
            DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
                "[PC=0x{:04X}] [JNC] Carry flag clear, jumping to address {}",
                pc, addr
            ), DebugLevel::DETAIL);
            cpu.set_pc(addr);
        } else {
            DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
                "[PC=0x{:04X}] [JNC] Carry flag set, continuing",
                pc, pc
            ), DebugLevel::DETAIL);
            cpu.set_pc(pc + 5);
        }
    } else {
        running = false;
    }

    cpu.print_state("JNC");
}

// Implementation from jo.cpp
void handle_jo(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 4 < program.size()) {
        uint32_t addr = static_cast<uint32_t>(program[pc + 1]) |
                       (static_cast<uint32_t>(program[pc + 2]) << 8) |
                       (static_cast<uint32_t>(program[pc + 3]) << 16) |
                       (static_cast<uint32_t>(program[pc + 4]) << 24);
        DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
            "[PC=0x{:04X}] [JO] PC={} Checking overflow flag",
            pc, pc
        ), DebugLevel::DETAIL);

        if (cpu.get_flags() & FLAG_OVERFLOW) {
            DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
                "[PC=0x{:04X}] [JO] Overflow flag set, jumping to address {}",
                pc, addr
            ), DebugLevel::DETAIL);
            cpu.set_pc(addr);
        } else {
            DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
                "[PC=0x{:04X}] [JO] Overflow flag clear, continuing",
                pc, pc
            ), DebugLevel::DETAIL);
            cpu.set_pc(pc + 5);
        }
    } else {
        running = false;
    }

    cpu.print_state("JO");
}

// Implementation from jno.cpp
void handle_jno(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 4 < program.size()) {
        uint32_t addr = static_cast<uint32_t>(program[pc + 1]) |
                       (static_cast<uint32_t>(program[pc + 2]) << 8) |
                       (static_cast<uint32_t>(program[pc + 3]) << 16) |
                       (static_cast<uint32_t>(program[pc + 4]) << 24);
        DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
            "[PC=0x{:04X}] [JNO] PC={} Checking overflow flag",
            pc, pc
        ), DebugLevel::DETAIL);

        if (!(cpu.get_flags() & FLAG_OVERFLOW)) {
            DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
                "[PC=0x{:04X}] [JNO] Overflow flag clear, jumping to address {}",
                pc, addr
            ), DebugLevel::DETAIL);
            cpu.set_pc(addr);
        } else {
            DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
                "[PC=0x{:04X}] [JNO] Overflow flag set, continuing",
                pc, pc
            ), DebugLevel::DETAIL);
            cpu.set_pc(pc + 5);
        }
    } else {
        running = false;
    }

    cpu.print_state("JNO");
}

// Implementation for JG (Jump if Greater)
void handle_jg(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 4 < program.size()) {
        uint32_t addr = static_cast<uint32_t>(program[pc + 1]) |
                       (static_cast<uint32_t>(program[pc + 2]) << 8) |
                       (static_cast<uint32_t>(program[pc + 3]) << 16) |
                       (static_cast<uint32_t>(program[pc + 4]) << 24);
        DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
            "[PC=0x{:04X}] [JG] PC={} Checking flags for greater",
            pc, pc
        ), DebugLevel::DETAIL);

        // JG: Jump if greater (not sign and not zero)
        if (!(cpu.get_flags() & FLAG_SIGN) && !(cpu.get_flags() & FLAG_ZERO)) {
            DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
                "[PC=0x{:04X}] [JG] Greater condition met, jumping to address {}",
                pc, addr
            ), DebugLevel::DETAIL);
            cpu.set_pc(addr);
        } else {
            DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
                "[PC=0x{:04X}] [JG] Greater condition not met, continuing",
                pc, pc
            ), DebugLevel::DETAIL);
            cpu.set_pc(pc + 5);
        }
    } else {
        running = false;
    }

    cpu.print_state("JG");
}

// Implementation for JL (Jump if Less)
void handle_jl(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 4 < program.size()) {
        uint32_t addr = static_cast<uint32_t>(program[pc + 1]) |
                       (static_cast<uint32_t>(program[pc + 2]) << 8) |
                       (static_cast<uint32_t>(program[pc + 3]) << 16) |
                       (static_cast<uint32_t>(program[pc + 4]) << 24);
        DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
            "[PC=0x{:04X}] [JL] PC={} Checking flags for less",
            pc, pc
        ), DebugLevel::DETAIL);

        // JL: Jump if less (sign set and not zero)
        if ((cpu.get_flags() & FLAG_SIGN) && !(cpu.get_flags() & FLAG_ZERO)) {
            DEBUG_DETAIL(Logging::DebugCategory::CPU_JUMP, "JL jumping to {} from PC={}", addr, pc);
            
            DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
                "[PC=0x{:04X}] [JL] Less condition met, jumping to address {}",
                pc, addr
            ), DebugLevel::DETAIL);
            cpu.set_pc(addr);
        } else {
            DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
                "[PC=0x{:04X}] [JL] Less condition not met, continuing",
                pc, pc
            ), DebugLevel::DETAIL);
            cpu.set_pc(pc + 5);
        }
    } else {
        running = false;
    }

    cpu.print_state("JL");
}

// Implementation for JGE (Jump if Greater or Equal)
void handle_jge(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 4 < program.size()) {
        uint32_t addr = static_cast<uint32_t>(program[pc + 1]) |
                       (static_cast<uint32_t>(program[pc + 2]) << 8) |
                       (static_cast<uint32_t>(program[pc + 3]) << 16) |
                       (static_cast<uint32_t>(program[pc + 4]) << 24);
        DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
            "[PC=0x{:04X}] [JGE] PC={} Checking flags for greater or equal",
            pc, pc
        ), DebugLevel::DETAIL);

        // JGE: Jump if greater or equal (not sign or zero)
        if (!(cpu.get_flags() & FLAG_SIGN) || (cpu.get_flags() & FLAG_ZERO)) {
            DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
                "[PC=0x{:04X}] [JGE] Greater or equal condition met, jumping to address {}",
                pc, addr
            ), DebugLevel::DETAIL);
            cpu.set_pc(addr);
        } else {
            DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
                "[PC=0x{:04X}] [JGE] Greater or equal condition not met, continuing",
                pc, pc
            ), DebugLevel::DETAIL);
            cpu.set_pc(pc + 5);
        }
    } else {
        running = false;
    }

    cpu.print_state("JGE");
}

// Implementation for JLE (Jump if Less or Equal)
void handle_jle(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 4 < program.size()) {
        uint32_t addr = static_cast<uint32_t>(program[pc + 1]) |
                       (static_cast<uint32_t>(program[pc + 2]) << 8) |
                       (static_cast<uint32_t>(program[pc + 3]) << 16) |
                       (static_cast<uint32_t>(program[pc + 4]) << 24);
        DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
            "[PC=0x{:04X}] [JLE] PC={} Checking flags for less or equal",
            pc, pc
        ), DebugLevel::DETAIL);

        // JLE: Jump if less or equal (sign set or zero)
        if ((cpu.get_flags() & FLAG_SIGN) || (cpu.get_flags() & FLAG_ZERO)) {
            DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
                "[PC=0x{:04X}] [JLE] Less or equal condition met, jumping to address {}",
                pc, addr
            ), DebugLevel::DETAIL);
            cpu.set_pc(addr);
        } else {
            DebugHandler::instance().report(DebugCategory::CPU_JUMP, fmt::format(
                "[PC=0x{:04X}] [JLE] Less or equal condition not met, continuing",
                pc, pc
            ), DebugLevel::DETAIL);
            cpu.set_pc(pc + 5);
        }
    } else {
        running = false;
    }

    cpu.print_state("JLE");
}

// Forward declarations for 64-bit operations implemented later in this file
void handle_inc64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_not64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_dec64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);

namespace {

inline Register normalize_simd_base(Register reg) {
    // XMM registers are stored as pairs (low/high). If the high part is passed,
    // normalize to the base (low) register.
    const size_t idx = static_cast<size_t>(reg);
    if ((idx & 1U) != 0U) {
        return static_cast<Register>(idx - 1);
    }
    return reg;
}

inline bool validate_simd_reg(CPU& cpu, uint8_t reg_byte, const char* opname) {
    if (reg_byte >= cpu.get_registers_64().size()) {
        std::string context = fmt::format("Register R{} out of range (max: R{})", reg_byte, cpu.get_registers_64().size() - 1);
        std::string message = fmt::format("Invalid register access in {} instruction", opname);
        ErrorHandler::instance().report_runtime(ErrorCode::CPU_INVALID_REGISTER, message, cpu.get_pc(), context);
        return false;
    }
    Register reg = static_cast<Register>(reg_byte);
    if (!RegisterNames::is_simd(reg)) {
        std::string context = fmt::format("Register R{} is not a SIMD register", reg_byte);
        std::string message = fmt::format("Invalid SIMD register in {} instruction", opname);
        ErrorHandler::instance().report_runtime(ErrorCode::CPU_INVALID_REGISTER, message, cpu.get_pc(), context);
        return false;
    }
    return true;
}

inline void unpack_ps(uint64_t low, uint64_t high, float out[4]) {
    uint32_t bits[4];
    bits[0] = static_cast<uint32_t>(low & 0xFFFFFFFFULL);
    bits[1] = static_cast<uint32_t>((low >> 32) & 0xFFFFFFFFULL);
    bits[2] = static_cast<uint32_t>(high & 0xFFFFFFFFULL);
    bits[3] = static_cast<uint32_t>((high >> 32) & 0xFFFFFFFFULL);
    out[0] = safe_bitcast<float>(bits[0]);
    out[1] = safe_bitcast<float>(bits[1]);
    out[2] = safe_bitcast<float>(bits[2]);
    out[3] = safe_bitcast<float>(bits[3]);
}

inline void pack_ps(const float in[4], uint64_t& low, uint64_t& high) {
    uint32_t bits[4];
    bits[0] = safe_bitcast<uint32_t>(in[0]);
    bits[1] = safe_bitcast<uint32_t>(in[1]);
    bits[2] = safe_bitcast<uint32_t>(in[2]);
    bits[3] = safe_bitcast<uint32_t>(in[3]);
    low = (static_cast<uint64_t>(bits[1]) << 32) | static_cast<uint64_t>(bits[0]);
    high = (static_cast<uint64_t>(bits[3]) << 32) | static_cast<uint64_t>(bits[2]);
}

inline void unpack_pd(uint64_t low, uint64_t high, double out[2]) {
    out[0] = safe_bitcast<double>(low);
    out[1] = safe_bitcast<double>(high);
}

inline void pack_pd(const double in[2], uint64_t& low, uint64_t& high) {
    low = safe_bitcast<uint64_t>(in[0]);
    high = safe_bitcast<uint64_t>(in[1]);
}

} // namespace

// SSE-style SIMD handlers (reg, reg; dst is updated)
static void handle_MOVAPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 >= program.size()) {
        running = false;
        return;
    }
    const uint8_t dst_b = program[cpu.get_pc() + 1];
    const uint8_t src_b = program[cpu.get_pc() + 2];
    if (!validate_simd_reg(cpu, dst_b, "MOVAPS") || !validate_simd_reg(cpu, src_b, "MOVAPS")) {
        running = false;
        return;
    }

    Register dst = normalize_simd_base(static_cast<Register>(dst_b));
    Register src = normalize_simd_base(static_cast<Register>(src_b));
    uint64_t low = 0, high = 0;
    cpu.get_xmm_register(src, low, high);
    cpu.set_xmm_register(dst, low, high);
    cpu.set_pc(cpu.get_pc() + 3);
    cpu.print_state("MOVAPS");
}

static void handle_MOVUPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    // VM does not model alignment, so MOVUPS behaves like MOVAPS.
    handle_MOVAPS(cpu, program, running);
}

static void handle_ADDPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 >= program.size()) {
        running = false;
        return;
    }
    const uint8_t dst_b = program[cpu.get_pc() + 1];
    const uint8_t src_b = program[cpu.get_pc() + 2];
    if (!validate_simd_reg(cpu, dst_b, "ADDPS") || !validate_simd_reg(cpu, src_b, "ADDPS")) {
        running = false;
        return;
    }

    Register dst = normalize_simd_base(static_cast<Register>(dst_b));
    Register src = normalize_simd_base(static_cast<Register>(src_b));
    uint64_t dlow = 0, dhigh = 0, slow = 0, shigh = 0;
    cpu.get_xmm_register(dst, dlow, dhigh);
    cpu.get_xmm_register(src, slow, shigh);

    float a[4], b[4];
    unpack_ps(dlow, dhigh, a);
    unpack_ps(slow, shigh, b);
    for (int i = 0; i < 4; ++i) a[i] = a[i] + b[i];
    pack_ps(a, dlow, dhigh);
    cpu.set_xmm_register(dst, dlow, dhigh);

    cpu.set_pc(cpu.get_pc() + 3);
    cpu.print_state("ADDPS");
}

static void handle_SUBPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 >= program.size()) {
        running = false;
        return;
    }
    const uint8_t dst_b = program[cpu.get_pc() + 1];
    const uint8_t src_b = program[cpu.get_pc() + 2];
    if (!validate_simd_reg(cpu, dst_b, "SUBPS") || !validate_simd_reg(cpu, src_b, "SUBPS")) {
        running = false;
        return;
    }
    Register dst = normalize_simd_base(static_cast<Register>(dst_b));
    Register src = normalize_simd_base(static_cast<Register>(src_b));
    uint64_t dlow = 0, dhigh = 0, slow = 0, shigh = 0;
    cpu.get_xmm_register(dst, dlow, dhigh);
    cpu.get_xmm_register(src, slow, shigh);

    float a[4], b[4];
    unpack_ps(dlow, dhigh, a);
    unpack_ps(slow, shigh, b);
    for (int i = 0; i < 4; ++i) a[i] = a[i] - b[i];
    pack_ps(a, dlow, dhigh);
    cpu.set_xmm_register(dst, dlow, dhigh);

    cpu.set_pc(cpu.get_pc() + 3);
    cpu.print_state("SUBPS");
}

static void handle_MULPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 >= program.size()) {
        running = false;
        return;
    }
    const uint8_t dst_b = program[cpu.get_pc() + 1];
    const uint8_t src_b = program[cpu.get_pc() + 2];
    if (!validate_simd_reg(cpu, dst_b, "MULPS") || !validate_simd_reg(cpu, src_b, "MULPS")) {
        running = false;
        return;
    }
    Register dst = normalize_simd_base(static_cast<Register>(dst_b));
    Register src = normalize_simd_base(static_cast<Register>(src_b));
    uint64_t dlow = 0, dhigh = 0, slow = 0, shigh = 0;
    cpu.get_xmm_register(dst, dlow, dhigh);
    cpu.get_xmm_register(src, slow, shigh);

    float a[4], b[4];
    unpack_ps(dlow, dhigh, a);
    unpack_ps(slow, shigh, b);
    for (int i = 0; i < 4; ++i) a[i] = a[i] * b[i];
    pack_ps(a, dlow, dhigh);
    cpu.set_xmm_register(dst, dlow, dhigh);
    cpu.set_pc(cpu.get_pc() + 3);
    cpu.print_state("MULPS");
}

static void handle_DIVPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 >= program.size()) {
        running = false;
        return;
    }
    const uint8_t dst_b = program[cpu.get_pc() + 1];
    const uint8_t src_b = program[cpu.get_pc() + 2];
    if (!validate_simd_reg(cpu, dst_b, "DIVPS") || !validate_simd_reg(cpu, src_b, "DIVPS")) {
        running = false;
        return;
    }
    Register dst = normalize_simd_base(static_cast<Register>(dst_b));
    Register src = normalize_simd_base(static_cast<Register>(src_b));
    uint64_t dlow = 0, dhigh = 0, slow = 0, shigh = 0;
    cpu.get_xmm_register(dst, dlow, dhigh);
    cpu.get_xmm_register(src, slow, shigh);
    float a[4], b[4];
    unpack_ps(dlow, dhigh, a);
    unpack_ps(slow, shigh, b);
    for (int i = 0; i < 4; ++i) a[i] = a[i] / b[i];
    pack_ps(a, dlow, dhigh);
    cpu.set_xmm_register(dst, dlow, dhigh);
    cpu.set_pc(cpu.get_pc() + 3);
    cpu.print_state("DIVPS");
}

static void handle_SQRTPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 >= program.size()) {
        running = false;
        return;
    }
    const uint8_t dst_b = program[cpu.get_pc() + 1];
    const uint8_t src_b = program[cpu.get_pc() + 2];
    if (!validate_simd_reg(cpu, dst_b, "SQRTPS") || !validate_simd_reg(cpu, src_b, "SQRTPS")) {
        running = false;
        return;
    }
    Register dst = normalize_simd_base(static_cast<Register>(dst_b));
    Register src = normalize_simd_base(static_cast<Register>(src_b));
    uint64_t slow = 0, shigh = 0;
    cpu.get_xmm_register(src, slow, shigh);
    float a[4];
    unpack_ps(slow, shigh, a);
    for (int i = 0; i < 4; ++i) a[i] = std::sqrt(a[i]);
    uint64_t dlow = 0, dhigh = 0;
    pack_ps(a, dlow, dhigh);
    cpu.set_xmm_register(dst, dlow, dhigh);
    cpu.set_pc(cpu.get_pc() + 3);
    cpu.print_state("SQRTPS");
}

static void handle_MAXPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 >= program.size()) {
        running = false;
        return;
    }
    const uint8_t dst_b = program[cpu.get_pc() + 1];
    const uint8_t src_b = program[cpu.get_pc() + 2];
    if (!validate_simd_reg(cpu, dst_b, "MAXPS") || !validate_simd_reg(cpu, src_b, "MAXPS")) {
        running = false;
        return;
    }
    Register dst = normalize_simd_base(static_cast<Register>(dst_b));
    Register src = normalize_simd_base(static_cast<Register>(src_b));
    uint64_t dlow = 0, dhigh = 0, slow = 0, shigh = 0;
    cpu.get_xmm_register(dst, dlow, dhigh);
    cpu.get_xmm_register(src, slow, shigh);
    float a[4], b[4];
    unpack_ps(dlow, dhigh, a);
    unpack_ps(slow, shigh, b);
    for (int i = 0; i < 4; ++i) a[i] = std::fmax(a[i], b[i]);
    pack_ps(a, dlow, dhigh);
    cpu.set_xmm_register(dst, dlow, dhigh);
    cpu.set_pc(cpu.get_pc() + 3);
    cpu.print_state("MAXPS");
}

static void handle_MINPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 >= program.size()) {
        running = false;
        return;
    }
    const uint8_t dst_b = program[cpu.get_pc() + 1];
    const uint8_t src_b = program[cpu.get_pc() + 2];
    if (!validate_simd_reg(cpu, dst_b, "MINPS") || !validate_simd_reg(cpu, src_b, "MINPS")) {
        running = false;
        return;
    }
    Register dst = normalize_simd_base(static_cast<Register>(dst_b));
    Register src = normalize_simd_base(static_cast<Register>(src_b));
    uint64_t dlow = 0, dhigh = 0, slow = 0, shigh = 0;
    cpu.get_xmm_register(dst, dlow, dhigh);
    cpu.get_xmm_register(src, slow, shigh);
    float a[4], b[4];
    unpack_ps(dlow, dhigh, a);
    unpack_ps(slow, shigh, b);
    for (int i = 0; i < 4; ++i) a[i] = std::fmin(a[i], b[i]);
    pack_ps(a, dlow, dhigh);
    cpu.set_xmm_register(dst, dlow, dhigh);
    cpu.set_pc(cpu.get_pc() + 3);
    cpu.print_state("MINPS");
}

static void handle_ANDPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 >= program.size()) {
        running = false;
        return;
    }
    const uint8_t dst_b = program[cpu.get_pc() + 1];
    const uint8_t src_b = program[cpu.get_pc() + 2];
    if (!validate_simd_reg(cpu, dst_b, "ANDPS") || !validate_simd_reg(cpu, src_b, "ANDPS")) {
        running = false;
        return;
    }
    Register dst = normalize_simd_base(static_cast<Register>(dst_b));
    Register src = normalize_simd_base(static_cast<Register>(src_b));
    uint64_t dlow = 0, dhigh = 0, slow = 0, shigh = 0;
    cpu.get_xmm_register(dst, dlow, dhigh);
    cpu.get_xmm_register(src, slow, shigh);
    cpu.set_xmm_register(dst, dlow & slow, dhigh & shigh);
    cpu.set_pc(cpu.get_pc() + 3);
    cpu.print_state("ANDPS");
}

static void handle_ORPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 >= program.size()) {
        running = false;
        return;
    }
    const uint8_t dst_b = program[cpu.get_pc() + 1];
    const uint8_t src_b = program[cpu.get_pc() + 2];
    if (!validate_simd_reg(cpu, dst_b, "ORPS") || !validate_simd_reg(cpu, src_b, "ORPS")) {
        running = false;
        return;
    }
    Register dst = normalize_simd_base(static_cast<Register>(dst_b));
    Register src = normalize_simd_base(static_cast<Register>(src_b));
    uint64_t dlow = 0, dhigh = 0, slow = 0, shigh = 0;
    cpu.get_xmm_register(dst, dlow, dhigh);
    cpu.get_xmm_register(src, slow, shigh);
    cpu.set_xmm_register(dst, dlow | slow, dhigh | shigh);
    cpu.set_pc(cpu.get_pc() + 3);
    cpu.print_state("ORPS");
}

static void handle_XORPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 >= program.size()) {
        running = false;
        return;
    }
    const uint8_t dst_b = program[cpu.get_pc() + 1];
    const uint8_t src_b = program[cpu.get_pc() + 2];
    if (!validate_simd_reg(cpu, dst_b, "XORPS") || !validate_simd_reg(cpu, src_b, "XORPS")) {
        running = false;
        return;
    }
    Register dst = normalize_simd_base(static_cast<Register>(dst_b));
    Register src = normalize_simd_base(static_cast<Register>(src_b));
    uint64_t dlow = 0, dhigh = 0, slow = 0, shigh = 0;
    cpu.get_xmm_register(dst, dlow, dhigh);
    cpu.get_xmm_register(src, slow, shigh);
    cpu.set_xmm_register(dst, dlow ^ slow, dhigh ^ shigh);
    cpu.set_pc(cpu.get_pc() + 3);
    cpu.print_state("XORPS");
}

static void handle_MOVAPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    handle_MOVAPS(cpu, program, running);
}

static void handle_MOVUPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    handle_MOVAPS(cpu, program, running);
}

static void handle_ADDPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 >= program.size()) {
        running = false;
        return;
    }
    const uint8_t dst_b = program[cpu.get_pc() + 1];
    const uint8_t src_b = program[cpu.get_pc() + 2];
    if (!validate_simd_reg(cpu, dst_b, "ADDPD") || !validate_simd_reg(cpu, src_b, "ADDPD")) {
        running = false;
        return;
    }
    Register dst = normalize_simd_base(static_cast<Register>(dst_b));
    Register src = normalize_simd_base(static_cast<Register>(src_b));
    uint64_t dlow = 0, dhigh = 0, slow = 0, shigh = 0;
    cpu.get_xmm_register(dst, dlow, dhigh);
    cpu.get_xmm_register(src, slow, shigh);
    double a[2], b[2];
    unpack_pd(dlow, dhigh, a);
    unpack_pd(slow, shigh, b);
    a[0] = a[0] + b[0];
    a[1] = a[1] + b[1];
    pack_pd(a, dlow, dhigh);
    cpu.set_xmm_register(dst, dlow, dhigh);
    cpu.set_pc(cpu.get_pc() + 3);
    cpu.print_state("ADDPD");
}

static void handle_SUBPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 >= program.size()) {
        running = false;
        return;
    }
    const uint8_t dst_b = program[cpu.get_pc() + 1];
    const uint8_t src_b = program[cpu.get_pc() + 2];
    if (!validate_simd_reg(cpu, dst_b, "SUBPD") || !validate_simd_reg(cpu, src_b, "SUBPD")) {
        running = false;
        return;
    }
    Register dst = normalize_simd_base(static_cast<Register>(dst_b));
    Register src = normalize_simd_base(static_cast<Register>(src_b));
    uint64_t dlow = 0, dhigh = 0, slow = 0, shigh = 0;
    cpu.get_xmm_register(dst, dlow, dhigh);
    cpu.get_xmm_register(src, slow, shigh);
    double a[2], b[2];
    unpack_pd(dlow, dhigh, a);
    unpack_pd(slow, shigh, b);
    a[0] = a[0] - b[0];
    a[1] = a[1] - b[1];
    pack_pd(a, dlow, dhigh);
    cpu.set_xmm_register(dst, dlow, dhigh);
    cpu.set_pc(cpu.get_pc() + 3);
    cpu.print_state("SUBPD");
}

static void handle_MULPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 >= program.size()) {
        running = false;
        return;
    }
    const uint8_t dst_b = program[cpu.get_pc() + 1];
    const uint8_t src_b = program[cpu.get_pc() + 2];
    if (!validate_simd_reg(cpu, dst_b, "MULPD") || !validate_simd_reg(cpu, src_b, "MULPD")) {
        running = false;
        return;
    }
    Register dst = normalize_simd_base(static_cast<Register>(dst_b));
    Register src = normalize_simd_base(static_cast<Register>(src_b));
    uint64_t dlow = 0, dhigh = 0, slow = 0, shigh = 0;
    cpu.get_xmm_register(dst, dlow, dhigh);
    cpu.get_xmm_register(src, slow, shigh);
    double a[2], b[2];
    unpack_pd(dlow, dhigh, a);
    unpack_pd(slow, shigh, b);
    a[0] = a[0] * b[0];
    a[1] = a[1] * b[1];
    pack_pd(a, dlow, dhigh);
    cpu.set_xmm_register(dst, dlow, dhigh);
    cpu.set_pc(cpu.get_pc() + 3);
    cpu.print_state("MULPD");
}

static void handle_DIVPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 >= program.size()) {
        running = false;
        return;
    }
    const uint8_t dst_b = program[cpu.get_pc() + 1];
    const uint8_t src_b = program[cpu.get_pc() + 2];
    if (!validate_simd_reg(cpu, dst_b, "DIVPD") || !validate_simd_reg(cpu, src_b, "DIVPD")) {
        running = false;
        return;
    }
    Register dst = normalize_simd_base(static_cast<Register>(dst_b));
    Register src = normalize_simd_base(static_cast<Register>(src_b));
    uint64_t dlow = 0, dhigh = 0, slow = 0, shigh = 0;
    cpu.get_xmm_register(dst, dlow, dhigh);
    cpu.get_xmm_register(src, slow, shigh);
    double a[2], b[2];
    unpack_pd(dlow, dhigh, a);
    unpack_pd(slow, shigh, b);
    a[0] = a[0] / b[0];
    a[1] = a[1] / b[1];
    pack_pd(a, dlow, dhigh);
    cpu.set_xmm_register(dst, dlow, dhigh);
    cpu.set_pc(cpu.get_pc() + 3);
    cpu.print_state("DIVPD");
}

static void handle_SQRTPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 >= program.size()) {
        running = false;
        return;
    }
    const uint8_t dst_b = program[cpu.get_pc() + 1];
    const uint8_t src_b = program[cpu.get_pc() + 2];
    if (!validate_simd_reg(cpu, dst_b, "SQRTPD") || !validate_simd_reg(cpu, src_b, "SQRTPD")) {
        running = false;
        return;
    }
    Register dst = normalize_simd_base(static_cast<Register>(dst_b));
    Register src = normalize_simd_base(static_cast<Register>(src_b));
    uint64_t slow = 0, shigh = 0;
    cpu.get_xmm_register(src, slow, shigh);
    double a[2];
    unpack_pd(slow, shigh, a);
    a[0] = std::sqrt(a[0]);
    a[1] = std::sqrt(a[1]);
    uint64_t dlow = 0, dhigh = 0;
    pack_pd(a, dlow, dhigh);
    cpu.set_xmm_register(dst, dlow, dhigh);
    cpu.set_pc(cpu.get_pc() + 3);
    cpu.print_state("SQRTPD");
}

static void handle_MAXPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 >= program.size()) {
        running = false;
        return;
    }
    const uint8_t dst_b = program[cpu.get_pc() + 1];
    const uint8_t src_b = program[cpu.get_pc() + 2];
    if (!validate_simd_reg(cpu, dst_b, "MAXPD") || !validate_simd_reg(cpu, src_b, "MAXPD")) {
        running = false;
        return;
    }
    Register dst = normalize_simd_base(static_cast<Register>(dst_b));
    Register src = normalize_simd_base(static_cast<Register>(src_b));
    uint64_t dlow = 0, dhigh = 0, slow = 0, shigh = 0;
    cpu.get_xmm_register(dst, dlow, dhigh);
    cpu.get_xmm_register(src, slow, shigh);
    double a[2], b[2];
    unpack_pd(dlow, dhigh, a);
    unpack_pd(slow, shigh, b);
    a[0] = std::fmax(a[0], b[0]);
    a[1] = std::fmax(a[1], b[1]);
    pack_pd(a, dlow, dhigh);
    cpu.set_xmm_register(dst, dlow, dhigh);
    cpu.set_pc(cpu.get_pc() + 3);
    cpu.print_state("MAXPD");
}

static void handle_MINPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 2 >= program.size()) {
        running = false;
        return;
    }
    const uint8_t dst_b = program[cpu.get_pc() + 1];
    const uint8_t src_b = program[cpu.get_pc() + 2];
    if (!validate_simd_reg(cpu, dst_b, "MINPD") || !validate_simd_reg(cpu, src_b, "MINPD")) {
        running = false;
        return;
    }
    Register dst = normalize_simd_base(static_cast<Register>(dst_b));
    Register src = normalize_simd_base(static_cast<Register>(src_b));
    uint64_t dlow = 0, dhigh = 0, slow = 0, shigh = 0;
    cpu.get_xmm_register(dst, dlow, dhigh);
    cpu.get_xmm_register(src, slow, shigh);
    double a[2], b[2];
    unpack_pd(dlow, dhigh, a);
    unpack_pd(slow, shigh, b);
    a[0] = std::fmin(a[0], b[0]);
    a[1] = std::fmin(a[1], b[1]);
    pack_pd(a, dlow, dhigh);
    cpu.set_xmm_register(dst, dlow, dhigh);
    cpu.set_pc(cpu.get_pc() + 3);
    cpu.print_state("MINPD");
}

static void handle_ANDPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    handle_ANDPS(cpu, program, running);
}
static void handle_ORPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    handle_ORPS(cpu, program, running);
}
static void handle_XORPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    handle_XORPS(cpu, program, running);
}

static void handle_CMPPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 3 >= program.size()) {
        running = false;
        return;
    }
    const uint8_t dst_b = program[cpu.get_pc() + 1];
    const uint8_t src_b = program[cpu.get_pc() + 2];
    const uint8_t imm8 = program[cpu.get_pc() + 3]; // Comparison predicate
    if (!validate_simd_reg(cpu, dst_b, "CMPPS") || !validate_simd_reg(cpu, src_b, "CMPPS")) {
        running = false;
        return;
    }

    Register dst = normalize_simd_base(static_cast<Register>(dst_b));
    Register src = normalize_simd_base(static_cast<Register>(src_b));
    uint64_t dlow = 0, dhigh = 0, slow = 0, shigh = 0;
    cpu.get_xmm_register(dst, dlow, dhigh);
    cpu.get_xmm_register(src, slow, shigh);

    float a[4], b[4];
    unpack_ps(dlow, dhigh, a);
    unpack_ps(slow, shigh, b);

    // Compare and set all bits to 1 if true, 0 if false
    uint32_t result[4];
    for (int i = 0; i < 4; ++i) {
        bool cmp_result = false;
        switch (imm8 & 0x7) { // Lower 3 bits determine comparison type
            case 0: cmp_result = (a[i] == b[i]); break; // EQ
            case 1: cmp_result = (a[i] < b[i]); break;  // LT
            case 2: cmp_result = (a[i] <= b[i]); break; // LE
            case 3: cmp_result = std::isunordered(a[i], b[i]); break; // UNORD
            case 4: cmp_result = (a[i] != b[i]); break; // NEQ
            case 5: cmp_result = !(a[i] < b[i]); break; // NLT (GE)
            case 6: cmp_result = !(a[i] <= b[i]); break; // NLE (GT)
            case 7: cmp_result = !std::isunordered(a[i], b[i]); break; // ORD
        }
        result[i] = cmp_result ? 0xFFFFFFFF : 0x00000000;
    }

    dlow = (static_cast<uint64_t>(result[1]) << 32) | static_cast<uint64_t>(result[0]);
    dhigh = (static_cast<uint64_t>(result[3]) << 32) | static_cast<uint64_t>(result[2]);
    cpu.set_xmm_register(dst, dlow, dhigh);

    cpu.set_pc(cpu.get_pc() + 4);
    cpu.print_state("CMPPS");
}

static void handle_CMPPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    if (cpu.get_pc() + 3 >= program.size()) {
        running = false;
        return;
    }
    const uint8_t dst_b = program[cpu.get_pc() + 1];
    const uint8_t src_b = program[cpu.get_pc() + 2];
    const uint8_t imm8 = program[cpu.get_pc() + 3]; // Comparison predicate
    if (!validate_simd_reg(cpu, dst_b, "CMPPD") || !validate_simd_reg(cpu, src_b, "CMPPD")) {
        running = false;
        return;
    }

    Register dst = normalize_simd_base(static_cast<Register>(dst_b));
    Register src = normalize_simd_base(static_cast<Register>(src_b));
    uint64_t dlow = 0, dhigh = 0, slow = 0, shigh = 0;
    cpu.get_xmm_register(dst, dlow, dhigh);
    cpu.get_xmm_register(src, slow, shigh);

    double a[2], b[2];
    unpack_pd(dlow, dhigh, a);
    unpack_pd(slow, shigh, b);

    // Compare and set all bits to 1 if true, 0 if false
    uint64_t result[2];
    for (int i = 0; i < 2; ++i) {
        bool cmp_result = false;
        switch (imm8 & 0x7) { // Lower 3 bits determine comparison type
            case 0: cmp_result = (a[i] == b[i]); break; // EQ
            case 1: cmp_result = (a[i] < b[i]); break;  // LT
            case 2: cmp_result = (a[i] <= b[i]); break; // LE
            case 3: cmp_result = std::isunordered(a[i], b[i]); break; // UNORD
            case 4: cmp_result = (a[i] != b[i]); break; // NEQ
            case 5: cmp_result = !(a[i] < b[i]); break; // NLT (GE)
            case 6: cmp_result = !(a[i] <= b[i]); break; // NLE (GT)
            case 7: cmp_result = !std::isunordered(a[i], b[i]); break; // ORD
        }
        result[i] = cmp_result ? 0xFFFFFFFFFFFFFFFFULL : 0x0000000000000000ULL;
    }

    dlow = result[0];
    dhigh = result[1];
    cpu.set_xmm_register(dst, dlow, dhigh);

    cpu.set_pc(cpu.get_pc() + 4);
    cpu.print_state("CMPPD");
}

// Dispatcher function (copied from opcode_dispatcher.cpp)
void dispatch_opcode(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    DebugHandler::instance().report(DebugCategory::CPU_DISPATCHER, fmt::format(
        "[DISPATCH_OPCODE] ENTRY: PC=0x{:04X}",
        cpu.get_pc()
    ), DebugLevel::TRACE);
    
    if (cpu.get_pc() >= program.size()) {
        DebugHandler::instance().report(DebugCategory::CPU_DISPATCHER, 
            "[DISPATCH_OPCODE] PC out of bounds, stopping",
            DebugLevel::IMPORTANT);
        running = false;
        return;
    }

    Opcode opcode = static_cast<Opcode>(program[cpu.get_pc()]);
    DebugHandler::instance().report(DebugCategory::CPU_DISPATCHER, fmt::format(
        "[DISPATCH_OPCODE] Processing opcode 0x{:02X}",
        static_cast<uint8_t>(opcode)
    ), DebugLevel::TRACE);

    switch (opcode) {
        case Opcode::NOP:
            handle_nop(cpu, program, running);
            break;
        case Opcode::LOAD_IMM:
            handle_load_imm(cpu, program, running);
            break;
        case Opcode::ADD:
            handle_add(cpu, program, running);
            break;
        case Opcode::SUB:
            handle_sub(cpu, program, running);
            break;
        case Opcode::MUL:
            handle_mul(cpu, program, running);
            break;
        case Opcode::DIV:
            handle_div(cpu, program, running);
            break;
        case Opcode::MOD:
            handle_mod(cpu, program, running);
            break;
        case Opcode::INC:
            handle_inc(cpu, program, running);
            break;
        case Opcode::DEC:
            handle_dec(cpu, program, running);
            break;
        case Opcode::MOV:
            handle_mov(cpu, program, running);
            break;
        case Opcode::JMP:
            handle_jmp(cpu, program, running);
            break;
        case Opcode::JZ:
            handle_jz(cpu, program, running);
            break;
        case Opcode::JNZ:
            handle_jnz(cpu, program, running);
            break;
        case Opcode::JS:
            handle_js(cpu, program, running);
            break;
        case Opcode::JNS:
            handle_jns(cpu, program, running);
            break;
        case Opcode::JC:
            handle_jc(cpu, program, running);
            break;
        case Opcode::JNC:
            handle_jnc(cpu, program, running);
            break;
        case Opcode::JO:
            handle_jo(cpu, program, running);
            break;
        case Opcode::JNO:
            handle_jno(cpu, program, running);
            break;
        case Opcode::JG:
            handle_jg(cpu, program, running);
            break;
        case Opcode::JL:
            handle_jl(cpu, program, running);
            break;
        case Opcode::JGE:
            handle_jge(cpu, program, running);
            break;
        case Opcode::JLE:
            handle_jle(cpu, program, running);
            break;
        case Opcode::LOAD:
            handle_load(cpu, program, running);
            break;
        case Opcode::LOADR:
            handle_loadr(cpu, program, running);
            break;
        case Opcode::LEA:
            handle_lea(cpu, program, running);
            break;
        case Opcode::STORE:
            handle_store(cpu, program, running);
            break;
        case Opcode::SWAP:
            handle_swap(cpu, program, running);
            break;
        case Opcode::PUSH:
            handle_push(cpu, program, running);
            break;
        case Opcode::POP:
            handle_pop(cpu, program, running);
            break;
        case Opcode::CMP:
            handle_cmp(cpu, program, running);
            break;
        case Opcode::PUSH_FLAG:
            handle_push_flag(cpu, program, running);
            break;
        case Opcode::POP_FLAG:
            handle_pop_flag(cpu, program, running);
            break;
        case Opcode::HALT:
            handle_halt(cpu, program, running);
            break;
        case Opcode::CLI:
            handle_cli(cpu, program, running);
            break;
        case Opcode::STI:
            handle_sti(cpu, program, running);
            break;
        case Opcode::IRET:
            handle_iret(cpu, program, running);
            break;
        case Opcode::AND:
            handle_and(cpu, program, running);
            break;
        case Opcode::OR:
            handle_or(cpu, program, running);
            break;
        case Opcode::XOR:
            handle_xor(cpu, program, running);
            break;
        case Opcode::NOT:
            handle_not(cpu, program, running);
            break;
        case Opcode::SHL:
            handle_shl(cpu, program, running);
            break;
        case Opcode::SHR:
            handle_shr(cpu, program, running);
            break;
        case Opcode::CALL:
            handle_call(cpu, program, running);
            break;
        case Opcode::RET:
            handle_ret(cpu, program, running);
            break;
        case Opcode::PUSH_ARG:
            handle_push_arg(cpu, program, running);
            break;
        case Opcode::POP_ARG:
            handle_pop_arg(cpu, program, running);
            break;
        case Opcode::IN:
            handle_in(cpu, program, running);
            break;
        case Opcode::OUT:
            handle_out(cpu, program, running);
            break;
        case Opcode::INB:
            handle_inb(cpu, program, running);
            break;
        case Opcode::OUTB:
            handle_outb(cpu, program, running);
            break;
        case Opcode::INW:
            handle_inw(cpu, program, running);
            break;
        case Opcode::OUTW:
            handle_outw(cpu, program, running);
            break;
        case Opcode::INL:
            handle_inl(cpu, program, running);
            break;
        case Opcode::OUTL:
            handle_outl(cpu, program, running);
            break;
        case Opcode::INSTR:
            handle_instr(cpu, program, running);
            break;
        case Opcode::OUTSTR:
            handle_outstr(cpu, program, running);
            break;
        case Opcode::DB:
            handle_db(cpu, program, running);
            break;

        // Extended 64-bit register operations
        case Opcode::ADD64:
            handle_add64(cpu, program, running);
            break;
        case Opcode::SUB64:
            handle_sub64(cpu, program, running);
            break;
        case Opcode::MOV64:
            handle_mov64(cpu, program, running);
            break;
        case Opcode::INC64:
            handle_inc64(cpu, program, running);
            break;
        case Opcode::DEC64:
            handle_dec64(cpu, program, running);
            break;
        case Opcode::LOAD_IMM64:
            handle_load_imm64(cpu, program, running);
            break;
        case Opcode::MUL64:
            handle_mul64(cpu, program, running);
            break;
        case Opcode::DIV64:
            handle_div64(cpu, program, running);
            break;
        case Opcode::MOD64:
            handle_mod64(cpu, program, running);
            break;
        case Opcode::AND64:
            handle_and64(cpu, program, running);
            break;
        case Opcode::OR64:
            handle_or64(cpu, program, running);
            break;
        case Opcode::XOR64:
            handle_xor64(cpu, program, running);
            break;
        case Opcode::NOT64:
            handle_not64(cpu, program, running);
            break;
        case Opcode::CMP64:
            handle_cmp64(cpu, program, running);
            break;
        case Opcode::MOVEX:
            handle_movex(cpu, program, running);
            break;
        case Opcode::ADDEX:
            handle_addex(cpu, program, running);
            break;
        case Opcode::SUBEX:
            handle_subex(cpu, program, running);
            break;
        case Opcode::LOADEX:
            handle_loadex(cpu, program, running);
            break;
        case Opcode::STOREX:
            handle_storex(cpu, program, running);
            break;

        // CPU Mode Control Operations
        case Opcode::MODE32:
            handle_mode32(cpu, program, running);
            break;
        case Opcode::MODE64:
            handle_mode64(cpu, program, running);
            break;
        case Opcode::MODECMP:
            handle_modecmp(cpu, program, running);
            break;

        // FPU Operations
        case Opcode::FLD:
            handle_FLD(cpu, program, running);
            break;
        case Opcode::FST:
            handle_FST(cpu, program, running);
            break;
        case Opcode::FSTP:
            handle_FSTP(cpu, program, running);
            break;
        case Opcode::FILD:
            handle_FILD(cpu, program, running);
            break;
        case Opcode::FIST:
            handle_FIST(cpu, program, running);
            break;
        case Opcode::FISTP:
            handle_FISTP(cpu, program, running);
            break;
        case Opcode::FADD:
            handle_FADD(cpu, program, running);
            break;
        case Opcode::FSUB:
            handle_FSUB(cpu, program, running);
            break;
        case Opcode::FMUL:
            handle_FMUL(cpu, program, running);
            break;
        case Opcode::FDIV:
            handle_FDIV(cpu, program, running);
            break;
        case Opcode::FINIT:
            handle_FINIT(cpu, program, running);
            break;
        case Opcode::FABS:
            handle_FABS(cpu, program, running);
            break;
        case Opcode::FCHS:
            handle_FCHS(cpu, program, running);
            break;
        case Opcode::FSQRT:
            handle_FSQRT(cpu, program, running);
            break;
        case Opcode::FSIN:
            handle_FSIN(cpu, program, running);
            break;
        case Opcode::FCOS:
            handle_FCOS(cpu, program, running);
            break;
        case Opcode::FTAN:
            handle_FTAN(cpu, program, running);
            break;
        case Opcode::FCOMPP:
            handle_FCOMPP(cpu, program, running);
            break;
        case Opcode::FUCOMPP:
            handle_FUCOMPP(cpu, program, running);
            break;
        case Opcode::FCLEX:
            handle_FCLEX(cpu, program, running);
            break;
        case Opcode::FSTCW:
            handle_FSTCW(cpu, program, running);
            break;
        case Opcode::FLDCW:
            handle_FLDCW(cpu, program, running);
            break;
        case Opcode::FSTSW:
            handle_FSTSW(cpu, program, running);
            break;

        // SSE-style SIMD (XMM) operations
        case Opcode::MOVAPS:
            handle_MOVAPS(cpu, program, running);
            break;
        case Opcode::MOVUPS:
            handle_MOVUPS(cpu, program, running);
            break;
        case Opcode::ADDPS:
            handle_ADDPS(cpu, program, running);
            break;
        case Opcode::SUBPS:
            handle_SUBPS(cpu, program, running);
            break;
        case Opcode::MULPS:
            handle_MULPS(cpu, program, running);
            break;
        case Opcode::DIVPS:
            handle_DIVPS(cpu, program, running);
            break;
        case Opcode::SQRTPS:
            handle_SQRTPS(cpu, program, running);
            break;
        case Opcode::MAXPS:
            handle_MAXPS(cpu, program, running);
            break;
        case Opcode::MINPS:
            handle_MINPS(cpu, program, running);
            break;
        case Opcode::ANDPS:
            handle_ANDPS(cpu, program, running);
            break;
        case Opcode::ORPS:
            handle_ORPS(cpu, program, running);
            break;
        case Opcode::XORPS:
            handle_XORPS(cpu, program, running);
            break;
        case Opcode::CMPPS:
            handle_CMPPS(cpu, program, running);
            break;
        case Opcode::MOVAPD:
            handle_MOVAPD(cpu, program, running);
            break;
        case Opcode::MOVUPD:
            handle_MOVUPD(cpu, program, running);
            break;
        case Opcode::ADDPD:
            handle_ADDPD(cpu, program, running);
            break;
        case Opcode::SUBPD:
            handle_SUBPD(cpu, program, running);
            break;
        case Opcode::MULPD:
            handle_MULPD(cpu, program, running);
            break;
        case Opcode::DIVPD:
            handle_DIVPD(cpu, program, running);
            break;
        case Opcode::SQRTPD:
            handle_SQRTPD(cpu, program, running);
            break;
        case Opcode::MAXPD:
            handle_MAXPD(cpu, program, running);
            break;
        case Opcode::MINPD:
            handle_MINPD(cpu, program, running);
            break;
        case Opcode::ANDPD:
            handle_ANDPD(cpu, program, running);
            break;
        case Opcode::ORPD:
            handle_ORPD(cpu, program, running);
            break;
        case Opcode::XORPD:
            handle_XORPD(cpu, program, running);
            break;
        case Opcode::CMPPD:
            handle_CMPPD(cpu, program, running);
            break;

        // SIMD Vector Operations
        case Opcode::VADD:
            handle_VADD(cpu, program, running);
            break;
        case Opcode::VMUL:
            handle_VMUL(cpu, program, running);
            break;
        case Opcode::VDOT:
            handle_VDOT(cpu, program, running);
            break;
        case Opcode::VMAX:
            handle_VMAX(cpu, program, running);
            break;
        case Opcode::VBROADCAST:
            handle_VBROADCAST(cpu, program, running);
            break;
        case Opcode::VCMPGT:
            handle_VCMPGT(cpu, program, running);
            break;
        case Opcode::PACKB:
            handle_PACKB(cpu, program, running);
            break;
        case Opcode::UNPACKB:
            handle_UNPACKB(cpu, program, running);
            break;
        case Opcode::STORER:
            handle_storer(cpu, program, running);
            break;
        case Opcode::INT:
            handle_int(cpu, program, running);
            break;

        default:
            {
                std::string opcode_hex = fmt::format("{:02X}", static_cast<int>(opcode));
                std::string context = fmt::format("Invalid opcode: 0x{}", opcode_hex);
                std::string message = "Invalid or unrecognized opcode encountered";
                ErrorHandler::instance().report_runtime(ErrorCode::CPU_INVALID_OPCODE, message, cpu.get_pc(), context);
                running = false;
                throw CPUException("Invalid opcode: 0x" + opcode_hex);
            }
            break;
    }
    DebugHandler::instance().report(DebugCategory::CPU_DISPATCHER, fmt::format(
        "[DISPATCH_OPCODE] EXIT: PC=0x{:04X}",
        cpu.get_pc()
    ), DebugLevel::TRACE);
}

// 64-bit and Extended Register Operations Implementation
// These are placeholder implementations for compatibility

// Implementation for ADD64 opcode - 64-bit addition
void handle_add64(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg1 = program[pc + 1];
        uint8_t reg2 = program[pc + 2];

        DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
            "[PC=0x{:04X}] [ADD64] Adding 64-bit registers R{} and R{}",
            pc, reg1, reg2
        ), DebugLevel::DETAIL);

        // Use 64-bit register access for extended registers
        uint64_t value1 = cpu.get_register_64(static_cast<Register>(reg1));
        uint64_t value2 = cpu.get_register_64(static_cast<Register>(reg2));
        uint64_t result = value1 + value2;

        // Update flags for 64-bit arithmetic
        uint32_t flags = cpu.get_flags();

        // Check for carry
        if (result < value1) { // Overflow in unsigned addition
            flags |= FLAG_CARRY;
        } else {
            flags &= ~FLAG_CARRY;
        }

        // Check for signed overflow
        bool sign1 = (value1 >> 63) & 1;
        bool sign2 = (value2 >> 63) & 1;
        bool signr = (result >> 63) & 1;
        
        if ((sign1 == sign2) && (sign1 != signr)) {
            flags |= FLAG_OVERFLOW;
        } else {
            flags &= ~FLAG_OVERFLOW;
        }

        // Check for zero
        // result==0 is a valid arithmetic outcome; cppcheck cannot determine
        // its value statically. Intentional check, not dead code.
        if (result == 0) {
            flags |= FLAG_ZERO;
        } else {
            flags &= ~FLAG_ZERO;
        }

        // Check for negative (using sign bit)
        if (signr) {
            flags |= FLAG_SIGN;
        } else {
            flags &= ~FLAG_SIGN;
        }

        // Update flags and register
        cpu.set_flags(flags);
        cpu.set_register_64(static_cast<Register>(reg1), result);

        DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
            "[PC=0x{:04X}] [ADD64] Result: R{} = 0x{:016X} + 0x{:016X} = 0x{:016X}",
            pc, reg1, value1, value2, result
        ), DebugLevel::DETAIL);

        cpu.set_pc(pc + 3); // Advance past opcode and two register operands
    } else {
        running = false;
    }

    cpu.print_state("ADD64");
}

// Implementation for MOD64 opcode - 64-bit modulo (remainder)
// Format: MOD64 dest, src1, src2  (dest = src1 % src2)
void handle_mod64(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    // Check bounds for 3-operand instruction (opcode + 3 registers)
    if (pc + 3 >= program.size()) {
        Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION,
            fmt::format("[PC=0x{:04X}] MOD64: Not enough bytes for instruction", pc), Logging::DebugLevel::CRITICAL);
        running = false;
        return;
    }

    uint8_t dest_reg = program[pc + 1];
    uint8_t src1_reg = program[pc + 2];
    uint8_t src2_reg = program[pc + 3];

    Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION,
        fmt::format("[PC=0x{:04X}] [MOD64] R{} = R{} % R{}", pc, dest_reg, src1_reg, src2_reg),
        Logging::DebugLevel::DETAIL);

    if (dest_reg >= cpu.get_registers_64().size() ||
        src1_reg >= cpu.get_registers_64().size() ||
        src2_reg >= cpu.get_registers_64().size()) {
        Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION,
            fmt::format("[PC=0x{:04X}] MOD64: Invalid register numbers - dest:{} src1:{} src2:{}",
                        pc, dest_reg, src1_reg, src2_reg),
            Logging::DebugLevel::CRITICAL);
        running = false;
        return;
    }

    uint64_t dividend = cpu.get_registers_64()[src1_reg];
    uint64_t divisor = cpu.get_registers_64()[src2_reg];

    if (divisor == 0) {
        std::string error_msg = fmt::format("[PC=0x{:04X}] MOD64: Modulo by zero", pc);
        Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION, error_msg, Logging::DebugLevel::CRITICAL);
        throw CPUException(error_msg);
    }

    uint64_t result = dividend % divisor;
    cpu.get_registers_64()[dest_reg] = result;

    if (dest_reg < 8) {
        cpu.get_registers()[dest_reg] = static_cast<uint32_t>(result);
    }

    // Update flags (minimal: Z/S; clear C/O)
    uint32_t flags = cpu.get_flags();
    flags &= ~(FLAG_ZERO | FLAG_SIGN | FLAG_OVERFLOW | FLAG_CARRY);
    if (result == 0) flags |= FLAG_ZERO;
    if (static_cast<int64_t>(result) < 0) flags |= FLAG_SIGN;
    cpu.set_flags(flags);

    cpu.set_pc(pc + 4);
}

// Implementation for SUB64 opcode - 64-bit subtraction
void handle_sub64(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg1 = program[pc + 1];
        uint8_t reg2 = program[pc + 2];

        DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
            "[PC=0x{:04X}] [SUB64] Subtracting 64-bit registers R{} - R{}",
            pc, reg1, reg2
        ), DebugLevel::DETAIL);

        // Use 64-bit register access for extended registers
        uint64_t value1 = cpu.get_register_64(static_cast<Register>(reg1));
        uint64_t value2 = cpu.get_register_64(static_cast<Register>(reg2));
        uint64_t result = value1 - value2;

        // Update flags for 64-bit arithmetic
        uint32_t flags = cpu.get_flags();

        // Check for borrow (unsigned underflow)
        if (value1 < value2) {
            flags |= FLAG_CARRY;  // Carry flag indicates borrow
        } else {
            flags &= ~FLAG_CARRY;
        }

        // Check for signed overflow
        bool sign1 = (value1 >> 63) & 1;
        bool sign2 = (value2 >> 63) & 1;
        bool signr = (result >> 63) & 1;
        
        // Overflow occurs when subtracting numbers of opposite signs produces wrong sign
        if ((sign1 != sign2) && (sign1 != signr)) {
            flags |= FLAG_OVERFLOW;
        } else {
            flags &= ~FLAG_OVERFLOW;
        }

        // Check for zero
        // result==0 is a valid arithmetic outcome; cppcheck cannot determine
        // its value statically. Intentional check, not dead code.
        if (result == 0) {
            flags |= FLAG_ZERO;
        } else {
            flags &= ~FLAG_ZERO;
        }

        // Check for negative (using sign bit)
        if (signr) {
            flags |= FLAG_SIGN;
        } else {
            flags &= ~FLAG_SIGN;
        }

        // Update flags and register
        cpu.set_flags(flags);
        cpu.set_register_64(static_cast<Register>(reg1), result);

        DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
            "[PC=0x{:04X}] [SUB64] Result: R{} = 0x{:016X} - 0x{:016X} = 0x{:016X}",
            pc, reg1, value1, value2, result
        ), DebugLevel::DETAIL);

        cpu.set_pc(pc + 3); // Advance past opcode and two register operands
    } else {
        running = false;
    }

    cpu.print_state("SUB64");
}

// Implementation for MOV64 opcode - 64-bit move
void handle_mov64(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t dest_reg = program[pc + 1];
        uint8_t src_reg = program[pc + 2];

        DEBUG_TRACE(Logging::DebugCategory::CPU_EXECUTION, "[PC=0x{:04X}] [MOV64] Moving 64-bit value from R{} to R{}", pc, src_reg, dest_reg);

        // Use 64-bit register access for extended registers
        uint64_t value = cpu.get_register_64(static_cast<Register>(src_reg));
        cpu.set_register_64(static_cast<Register>(dest_reg), value);

        DEBUG_TRACE(Logging::DebugCategory::CPU_EXECUTION, "[PC=0x{:04X}] [MOV64] Result: R{} = 0x{:016X}", pc, dest_reg, value);

        cpu.set_pc(pc + 3); // Advance past opcode and two register operands
    } else {
        running = false;
    }

    cpu.print_state("MOV64");
}

// Implementation for INC64 opcode - 64-bit increment
void handle_inc64(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 1 < program.size()) {
        uint8_t reg = program[pc + 1];

        DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
            "[PC=0x{:04X}] [INC64] Incrementing 64-bit register R{}",
            pc, reg
        ), DebugLevel::DETAIL);

        // Use 64-bit register access
        uint64_t value = cpu.get_register_64(static_cast<Register>(reg));
        uint64_t result = value + 1;

        // Update flags for 64-bit arithmetic
        uint32_t flags = cpu.get_flags();

        // Check for overflow (signed)
        // Incrementing 0x7FFFFFFFFFFFFFFF to 0x8000000000000000 causes overflow
        if (value == 0x7FFFFFFFFFFFFFFF) {
            flags |= FLAG_OVERFLOW;
        } else {
            flags &= ~FLAG_OVERFLOW;
        }

        // Check for zero
        // result==0 is a valid arithmetic outcome; cppcheck cannot determine
        // its value statically. Intentional check, not dead code.
        if (result == 0) {
            flags |= FLAG_ZERO;
        } else {
            flags &= ~FLAG_ZERO;
        }

        // Check for negative (using sign bit)
        if ((result >> 63) & 1) {
            flags |= FLAG_SIGN;
        } else {
            flags &= ~FLAG_SIGN;
        }

        // Note: INC typically does not affect the carry flag on x86, 
        // but we'll follow the pattern of other arithmetic ops if needed.
        // For now, let's preserve the carry flag (standard x86 behavior).

        // Update flags and register
        cpu.set_flags(flags);
        cpu.set_register_64(static_cast<Register>(reg), result);

        DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
            "[PC=0x{:04X}] [INC64] Result: R{} = 0x{:016X} + 1 = 0x{:016X}",
            pc, reg, value, result
        ), DebugLevel::DETAIL);

        cpu.set_pc(pc + 2); // Advance past opcode and register operand
    } else {
        running = false;
    }

    cpu.print_state("INC64");
}

// Implementation for DEC64 opcode - 64-bit decrement
void handle_dec64(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 1 < program.size()) {
        uint8_t reg = program[pc + 1];

        DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
            "[PC=0x{:04X}] [DEC64] Decrementing 64-bit register R{}",
            pc, reg
        ), DebugLevel::DETAIL);

        // Use 64-bit register access
        uint64_t value = cpu.get_register_64(static_cast<Register>(reg));
        uint64_t result = value - 1;

        // Update flags for 64-bit arithmetic
        uint32_t flags = cpu.get_flags();

        // Check for overflow (signed)
        // Decrementing 0x8000000000000000 to 0x7FFFFFFFFFFFFFFF causes overflow
        if (value == 0x8000000000000000) {
            flags |= FLAG_OVERFLOW;
        } else {
            flags &= ~FLAG_OVERFLOW;
        }

        // Check for zero
        // result==0 is a valid arithmetic outcome; cppcheck cannot determine
        // its value statically. Intentional check, not dead code.
        if (result == 0) {
            flags |= FLAG_ZERO;
        } else {
            flags &= ~FLAG_ZERO;
        }

        // Check for negative (using sign bit)
        if ((result >> 63) & 1) {
            flags |= FLAG_SIGN;
        } else {
            flags &= ~FLAG_SIGN;
        }

        // Note: DEC typically does not affect the carry flag on x86.

        // Update flags and register
        cpu.set_flags(flags);
        cpu.set_register_64(static_cast<Register>(reg), result);

        DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
            "[PC=0x{:04X}] [DEC64] Result: R{} = 0x{:016X} - 1 = 0x{:016X}",
            pc, reg, value, result
        ), DebugLevel::DETAIL);

        cpu.set_pc(pc + 2); // Advance past opcode and register operand
    } else {
        running = false;
    }

    cpu.print_state("DEC64");
}

// Implementation for LOAD_IMM64 opcode - 64-bit immediate load
void handle_load_imm64(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    DEBUG_TRACE(Logging::DebugCategory::CPU_EXECUTION, "[PC=0x{:04X}] [LOAD_IMM64] 64-bit immediate load operation", cpu.get_pc());
    uint32_t pc = cpu.get_pc();
    if (pc + 9 < program.size()) {
        uint8_t reg = program[pc + 1];
        uint64_t imm = 0;
        // Little-endian: lowest byte first
        for (int i = 0; i < 8; ++i) {
            imm |= static_cast<uint64_t>(program[pc + 2 + i]) << (8 * i);
        }
        DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
            "[PC=0x{:04X}] [LOAD_IMM64] reg={} imm=0x{:016X}",
            pc, reg, imm
        ), DebugLevel::DETAIL);
        if (reg < TOTAL_REGISTERS) {
            cpu.set_register_64(static_cast<Register>(reg), imm);
            DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
                "[PC=0x{:04X}] [LOAD_IMM64] Set R{} = 0x{:016X}",
                pc, reg, imm
            ), DebugLevel::DETAIL);
        }
        cpu.set_pc(pc + 10);
    } else {
        running = false;
    }
    cpu.print_state("LOAD_IMM64");
}

// Mode Control Operations Implementation

// Implementation for MODE32 opcode - Switch to 32-bit mode
void handle_mode32(CPU& cpu, [[maybe_unused]] const std::vector<uint8_t>& program, [[maybe_unused]] bool& running) {
    DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
        "[PC=0x{:04X}] [MODE32] Switching CPU to 32-bit mode",
        cpu.get_pc()
    ), DebugLevel::INFO);
    cpu.set_cpu_mode(CPUMode::MODE_32BIT);
    cpu.set_pc(cpu.get_pc() + 1);
}

// Implementation for MODE64 opcode - Switch to 64-bit mode
void handle_mode64(CPU& cpu, [[maybe_unused]] const std::vector<uint8_t>& program, [[maybe_unused]] bool& running) {
    DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
        "[PC=0x{:04X}] [MODE64] Switching CPU to 64-bit mode",
        cpu.get_pc()
    ), DebugLevel::INFO);
    cpu.set_cpu_mode(CPUMode::MODE_64BIT);
    cpu.set_pc(cpu.get_pc() + 1);
}

// Implementation for MODECMP opcode - Mode-aware comparison
void handle_modecmp(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg1 = program[pc + 1];
        uint8_t reg2 = program[pc + 2];

        // Get current CPU mode
        CPUMode mode = cpu.get_cpu_mode();

        DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
            "[PC=0x{:04X}] [MODECMP] Comparing R{} and R{} in {} mode",
            pc, reg1, reg2, (mode == CPUMode::MODE_64BIT ? "64-bit" : "32-bit")
        ), DebugLevel::DETAIL);

        uint32_t flags = cpu.get_flags();

        if (mode == CPUMode::MODE_64BIT) {
            // 64-bit comparison
            uint64_t value1 = cpu.get_register_64(static_cast<Register>(reg1));
            uint64_t value2 = cpu.get_register_64(static_cast<Register>(reg2));
            
            // Perform subtraction for comparison (don't store result)
            uint64_t result = value1 - value2;

            // Zero flag
            if (result == 0) {
                flags |= FLAG_ZERO;
            } else {
                flags &= ~FLAG_ZERO;
            }

            // Sign flag (bit 63 for 64-bit)
            if ((result >> 63) & 1) {
                flags |= FLAG_SIGN;
            } else {
                flags &= ~FLAG_SIGN;
            }

            // Carry flag (unsigned comparison: value1 < value2)
            if (value1 < value2) {
                flags |= FLAG_CARRY;
            } else {
                flags &= ~FLAG_CARRY;
            }

            DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
                "[PC=0x{:04X}] [MODECMP] 64-bit compare: 0x{:016X} vs 0x{:016X}",
                pc, value1, value2
            ), DebugLevel::DETAIL);
        } else {
            // 32-bit comparison (MODE_32BIT or default)
            // Use legacy registers for R0-R7 in 32-bit mode
            uint32_t value1 = (reg1 < DemiEngine_Registers::TOTAL_REGISTERS) ? 
                cpu.get_registers()[reg1] : static_cast<uint32_t>(cpu.get_register(static_cast<Register>(reg1)));
            uint32_t value2 = (reg2 < DemiEngine_Registers::TOTAL_REGISTERS) ? 
                cpu.get_registers()[reg2] : static_cast<uint32_t>(cpu.get_register(static_cast<Register>(reg2)));
            
            // Perform subtraction for comparison (don't store result)
            uint32_t result = value1 - value2;

            // Zero flag
            if (result == 0) {
                flags |= FLAG_ZERO;
            } else {
                flags &= ~FLAG_ZERO;
            }

            // Sign flag (bit 31 for 32-bit)
            if ((result >> 31) & 1) {
                flags |= FLAG_SIGN;
            } else {
                flags &= ~FLAG_SIGN;
            }

            // Carry flag (unsigned comparison: value1 < value2)
            if (value1 < value2) {
                flags |= FLAG_CARRY;
            } else {
                flags &= ~FLAG_CARRY;
            }

            DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
                "[PC=0x{:04X}] [MODECMP] 32-bit compare: 0x{:08X} vs 0x{:08X}",
                pc, value1, value2
            ), DebugLevel::DETAIL);
        }

        cpu.set_flags(flags);
        cpu.set_pc(pc + 3); // Advance past opcode and two register operands
    } else {
        running = false;
    }

    cpu.print_state("MODECMP");
}

void handle_not64(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 1 < program.size()) {
        uint8_t reg = program[pc + 1];

        DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
            "[PC=0x{:04X}] [NOT64] Bitwise NOT of 64-bit register R{}",
            pc, reg
        ), DebugLevel::DETAIL);

        uint64_t value = cpu.get_register_64(static_cast<Register>(reg));
        uint64_t result = ~value;

        uint32_t flags = cpu.get_flags();

        // result==0 is a valid arithmetic outcome; cppcheck cannot determine
        // its value statically. Intentional check, not dead code.
        if (result == 0) {
            flags |= FLAG_ZERO;
        } else {
            flags &= ~FLAG_ZERO;
        }

        if ((result >> 63) & 1) {
            flags |= FLAG_SIGN;
        } else {
            flags &= ~FLAG_SIGN;
        }

        cpu.set_flags(flags);
        cpu.set_register_64(static_cast<Register>(reg), result);

        DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
            "[PC=0x{:04X}] [NOT64] Result: R{} = 0x{:016X} = ~0x{:016X}",
            pc, reg, result, value
        ), DebugLevel::DETAIL);

        cpu.set_pc(pc + 2);
    } else {
        running = false;
    }

    cpu.print_state("NOT64");
}

// === High-Priority Missing x86-Equivalent Opcodes ===

// ADC — Add with Carry: R[dst] = R[dst] + R[src] + CF
void handle_adc(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg1 = program[pc + 1];
        uint8_t reg2 = program[pc + 2];

        DEBUG_INSTRUCTION("ADC", pc, fmt::format("R{} += R{} + CF", reg1, reg2), "");

        if (reg1 < DemiEngine_Registers::TOTAL_REGISTERS && reg2 < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val1 = cpu.get_register_mode_aware(static_cast<Register>(reg1));
            uint64_t val2 = cpu.get_register_mode_aware(static_cast<Register>(reg2));
            uint32_t current_flags = cpu.get_flags();
            uint64_t carry_in = (current_flags & FLAG_CARRY) ? 1 : 0;

            uint64_t result = val1 + val2 + carry_in;
            uint64_t mask = cpu.get_operand_mask();
            uint64_t masked_result = result & mask;

            // Zero Flag
            if (masked_result == 0) {
                current_flags |= FLAG_ZERO;
            } else {
                current_flags &= ~FLAG_ZERO;
            }

            // Sign Flag
            if (cpu.is_32bit_mode()) {
                if ((masked_result & 0x80000000) != 0) current_flags |= FLAG_SIGN;
                else current_flags &= ~FLAG_SIGN;
            } else {
                if ((masked_result & 0x8000000000000000ULL) != 0) current_flags |= FLAG_SIGN;
                else current_flags &= ~FLAG_SIGN;
            }

            // Carry Flag (unsigned overflow of full-precision result)
            bool carry = false;
            if (cpu.is_32bit_mode()) {
                carry = (result > 0xFFFFFFFF);
            } else {
                carry = (result < val1 || (carry_in && result == val1));
            }
            if (carry) {
                current_flags |= FLAG_CARRY;
            } else {
                current_flags &= ~FLAG_CARRY;
            }

            // Overflow Flag (signed overflow)
            bool sign1, sign2, signR;
            if (cpu.is_32bit_mode()) {
                sign1 = (val1 & 0x80000000) != 0;
                sign2 = (val2 & 0x80000000) != 0;
                signR = (masked_result & 0x80000000) != 0;
            } else {
                sign1 = (val1 & 0x8000000000000000ULL) != 0;
                sign2 = (val2 & 0x8000000000000000ULL) != 0;
                signR = (masked_result & 0x8000000000000000ULL) != 0;
            }
            if ((sign1 == sign2) && (sign1 != signR)) {
                current_flags |= FLAG_OVERFLOW;
            } else {
                current_flags &= ~FLAG_OVERFLOW;
            }

            cpu.set_flags(current_flags);
            cpu.set_register_mode_aware(static_cast<Register>(reg1), result);

            DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
                "[PC=0x{:04X}] [ADC] R{}: {} + {} + {} = {} (carry={}, overflow={})",
                pc, reg1, val1, val2, carry_in, result,
                (cpu.get_flags() & FLAG_CARRY) ? 1 : 0,
                (cpu.get_flags() & FLAG_OVERFLOW) ? 1 : 0
            ), DebugLevel::DETAIL);
        }
        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }
    cpu.print_state("ADC");
}

// SBB — Subtract with Borrow: R[dst] = R[dst] - R[src] - CF
void handle_sbb(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg1 = program[pc + 1];
        uint8_t reg2 = program[pc + 2];

        DEBUG_INSTRUCTION("SBB", pc, fmt::format("R{} -= R{} + CF", reg1, reg2), "");

        if (reg1 < DemiEngine_Registers::TOTAL_REGISTERS && reg2 < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val1 = cpu.get_register_mode_aware(static_cast<Register>(reg1));
            uint64_t val2 = cpu.get_register_mode_aware(static_cast<Register>(reg2));
            uint32_t current_flags = cpu.get_flags();
            uint64_t borrow_in = (current_flags & FLAG_CARRY) ? 1 : 0;

            uint64_t subtrahend = val2 + borrow_in;
            uint64_t result = val1 - subtrahend;
            uint64_t mask = cpu.get_operand_mask();
            uint64_t masked_result = result & mask;

            // Zero Flag
            if (masked_result == 0) {
                current_flags |= FLAG_ZERO;
            } else {
                current_flags &= ~FLAG_ZERO;
            }

            // Sign Flag
            if (cpu.is_32bit_mode()) {
                if ((masked_result & 0x80000000) != 0) current_flags |= FLAG_SIGN;
                else current_flags &= ~FLAG_SIGN;
            } else {
                if ((masked_result & 0x8000000000000000ULL) != 0) current_flags |= FLAG_SIGN;
                else current_flags &= ~FLAG_SIGN;
            }

            // Borrow Flag (Carry flag is set on borrow)
            bool borrow = false;
            if (cpu.is_32bit_mode()) {
                borrow = (static_cast<uint32_t>(val1) < static_cast<uint32_t>(subtrahend));
            } else {
                borrow = (val1 < subtrahend);
            }
            if (borrow) {
                current_flags |= FLAG_CARRY;
            } else {
                current_flags &= ~FLAG_CARRY;
            }

            // Overflow Flag (signed overflow)
            bool sign1, sign2, signR;
            if (cpu.is_32bit_mode()) {
                sign1 = (val1 & 0x80000000) != 0;
                sign2 = (val2 & 0x80000000) != 0;
                signR = (masked_result & 0x80000000) != 0;
            } else {
                sign1 = (val1 & 0x8000000000000000ULL) != 0;
                sign2 = (val2 & 0x8000000000000000ULL) != 0;
                signR = (masked_result & 0x8000000000000000ULL) != 0;
            }
            // Overflow on SUB: sign1 != sign2 AND sign1 != signR
            if ((sign1 != sign2) && (sign1 != signR)) {
                current_flags |= FLAG_OVERFLOW;
            } else {
                current_flags &= ~FLAG_OVERFLOW;
            }

            cpu.set_flags(current_flags);
            cpu.set_register_mode_aware(static_cast<Register>(reg1), result);

            DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
                "[PC=0x{:04X}] [SBB] R{}: {} - ({} + {}) = {} (borrow={}, overflow={})",
                pc, reg1, val1, val2, borrow_in, result,
                (cpu.get_flags() & FLAG_CARRY) ? 1 : 0,
                (cpu.get_flags() & FLAG_OVERFLOW) ? 1 : 0
            ), DebugLevel::DETAIL);
        }
        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }
    cpu.print_state("SBB");
}

// IMUL — Signed Multiply: R[dst] = R[dst] * R[src] (signed)
// Sets CF/OF if upper half is not the sign-extension of lower half
void handle_imul(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg1 = program[pc + 1];
        uint8_t reg2 = program[pc + 2];

        DEBUG_INSTRUCTION("IMUL", pc, fmt::format("R{} *= R{} (signed)", reg1, reg2), "");

        if (reg1 < DemiEngine_Registers::TOTAL_REGISTERS && reg2 < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val1 = cpu.get_register_mode_aware(static_cast<Register>(reg1));
            uint64_t val2 = cpu.get_register_mode_aware(static_cast<Register>(reg2));

            int64_t sval1, sval2;
            uint64_t mask = cpu.get_operand_mask();

            if (cpu.is_32bit_mode()) {
                sval1 = static_cast<int64_t>(static_cast<int32_t>(val1 & 0xFFFFFFFF));
                sval2 = static_cast<int64_t>(static_cast<int32_t>(val2 & 0xFFFFFFFF));
            } else {
                sval1 = static_cast<int64_t>(val1);
                sval2 = static_cast<int64_t>(val2);
            }

#if defined(__GNUC__) || defined(__clang__)
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wpedantic"
            __int128 signed_result = static_cast<__int128>(sval1) * static_cast<__int128>(sval2);
            uint64_t result = static_cast<uint64_t>(signed_result);
            #pragma GCC diagnostic pop
#else
            int64_t result = sval1 * sval2;
#endif

            uint64_t masked_result = result & mask;

            uint32_t current_flags = cpu.get_flags();

            // Zero Flag
            if (masked_result == 0) {
                current_flags |= FLAG_ZERO;
            } else {
                current_flags &= ~FLAG_ZERO;
            }

            // Sign Flag
            if (cpu.is_32bit_mode()) {
                if ((masked_result & 0x80000000) != 0) current_flags |= FLAG_SIGN;
                else current_flags &= ~FLAG_SIGN;
            } else {
                if ((masked_result & 0x8000000000000000ULL) != 0) current_flags |= FLAG_SIGN;
                else current_flags &= ~FLAG_SIGN;
            }

            // CF/OF: set if upper half is not the sign-extension of the lower half
            bool overflow = false;
            if (cpu.is_32bit_mode()) {
                // Check if the 64-bit signed result fits in 32 bits (sign-extended)
                int64_t truncated = static_cast<int64_t>(static_cast<int32_t>(result & 0xFFFFFFFF));
                overflow = (truncated != static_cast<int64_t>(signed_result));
            } else {
                // Check if the 128-bit result fits in 64 bits (sign-extended)
                int64_t truncated_result = static_cast<int64_t>(result);
                __int128 upper = signed_result >> 64;
                overflow = (upper != 0 && upper != -1) ||
                           ((static_cast<int64_t>(result) < 0) != (static_cast<int64_t>(static_cast<uint64_t>(signed_result)) < 0));
                // Simpler: the upper 64 bits must be all 0 or all 1 (sign extension of lower half)
                uint64_t upper64 = static_cast<uint64_t>(static_cast<__int128>(signed_result) >> 64);
                bool lower_negative = (result & 0x8000000000000000ULL) != 0;
                overflow = !((lower_negative && upper64 == 0xFFFFFFFFFFFFFFFFULL) ||
                             (!lower_negative && upper64 == 0));
            }

            if (overflow) {
                current_flags |= FLAG_CARRY;
                current_flags |= FLAG_OVERFLOW;
            } else {
                current_flags &= ~FLAG_CARRY;
                current_flags &= ~FLAG_OVERFLOW;
            }

            cpu.set_flags(current_flags);
            cpu.set_register_mode_aware(static_cast<Register>(reg1), result);

            DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
                "[PC=0x{:04X}] [IMUL] R{}: {} * {} = {} (signed) (carry={}, overflow={})",
                pc, reg1, static_cast<int64_t>(sval1), static_cast<int64_t>(sval2),
                static_cast<int64_t>(result),
                (cpu.get_flags() & FLAG_CARRY) ? 1 : 0,
                (cpu.get_flags() & FLAG_OVERFLOW) ? 1 : 0
            ), DebugLevel::DETAIL);
        }
        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }
    cpu.print_state("IMUL");
}

// IDIV — Signed Divide: R[dst] = R[dst] / R[src] (signed)
// Special case: INT_MIN / -1 is undefined (sets overflow, leaves dst unchanged)
void handle_idiv(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg1 = program[pc + 1];
        uint8_t reg2 = program[pc + 2];

        DEBUG_INSTRUCTION("IDIV", pc, fmt::format("R{} /= R{} (signed)", reg1, reg2), "");

        if (reg1 < DemiEngine_Registers::TOTAL_REGISTERS && reg2 < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val1 = cpu.get_register_mode_aware(static_cast<Register>(reg1));
            uint64_t val2 = cpu.get_register_mode_aware(static_cast<Register>(reg2));

            int64_t sval1, sval2;
            if (cpu.is_32bit_mode()) {
                sval1 = static_cast<int64_t>(static_cast<int32_t>(val1 & 0xFFFFFFFF));
                sval2 = static_cast<int64_t>(static_cast<int32_t>(val2 & 0xFFFFFFFF));
            } else {
                sval1 = static_cast<int64_t>(val1);
                sval2 = static_cast<int64_t>(val2);
            }

            uint32_t current_flags = cpu.get_flags();

            if (sval2 == 0) {
                // Division by zero — undefined, leave result and flags unchanged
                DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
                    "[PC=0x{:04X}] [IDIV] Division by zero — result unchanged", pc
                ), DebugLevel::IMPORTANT);
            } else if (cpu.is_32bit_mode() &&
                       sval1 == INT32_MIN && sval2 == -1) {
                // INT32_MIN / -1 overflows 32-bit signed range
                current_flags |= FLAG_OVERFLOW;
                DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
                    "[PC=0x{:04X}] [IDIV] INT32_MIN / -1 overflow — result unchanged", pc
                ), DebugLevel::IMPORTANT);
            } else if (!cpu.is_32bit_mode() &&
                       sval1 == INT64_MIN && sval2 == -1) {
                // INT64_MIN / -1 overflows 64-bit signed range
                current_flags |= FLAG_OVERFLOW;
                DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
                    "[PC=0x{:04X}] [IDIV] INT64_MIN / -1 overflow — result unchanged", pc
                ), DebugLevel::IMPORTANT);
            } else {
                int64_t result = sval1 / sval2;
                uint64_t mask = cpu.get_operand_mask();
                uint64_t masked_result = static_cast<uint64_t>(result) & mask;

                // Zero Flag
                if (masked_result == 0) {
                    current_flags |= FLAG_ZERO;
                } else {
                    current_flags &= ~FLAG_ZERO;
                }

                // Sign Flag
                if (cpu.is_32bit_mode()) {
                    if ((masked_result & 0x80000000) != 0) current_flags |= FLAG_SIGN;
                    else current_flags &= ~FLAG_SIGN;
                } else {
                    if ((masked_result & 0x8000000000000000ULL) != 0) current_flags |= FLAG_SIGN;
                    else current_flags &= ~FLAG_SIGN;
                }

                // IDIV clears CF and OF on valid division
                current_flags &= ~FLAG_CARRY;
                current_flags &= ~FLAG_OVERFLOW;

                cpu.set_register_mode_aware(static_cast<Register>(reg1), static_cast<uint64_t>(result));

                DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
                    "[PC=0x{:04X}] [IDIV] R{}: {} / {} = {} (signed)",
                    pc, reg1, sval1, sval2, result
                ), DebugLevel::DETAIL);
            }

            cpu.set_flags(current_flags);
        }
        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }
    cpu.print_state("IDIV");
}

// SAL — Shift Arithmetic Left: identical to SHL (x86 SAL and SHL are the same opcode)
void handle_sal(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg = program[pc + 1];
        uint8_t imm = program[pc + 2];

        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val = cpu.get_register_mode_aware(static_cast<Register>(reg));
            uint64_t result = val << imm;
            uint64_t mask = cpu.get_operand_mask();
            uint64_t masked_result = result & mask;

            uint32_t current_flags = cpu.get_flags();

            // Zero Flag
            if (masked_result == 0) {
                current_flags |= FLAG_ZERO;
            } else {
                current_flags &= ~FLAG_ZERO;
            }

            // Sign Flag
            if (cpu.is_32bit_mode()) {
                if ((masked_result & 0x80000000) != 0) current_flags |= FLAG_SIGN;
                else current_flags &= ~FLAG_SIGN;
            } else {
                if ((masked_result & 0x8000000000000000ULL) != 0) current_flags |= FLAG_SIGN;
                else current_flags &= ~FLAG_SIGN;
            }

            // Carry Flag (last bit shifted out)
            if (imm > 0) {
                bool carry = false;
                if (cpu.is_32bit_mode()) {
                    if (imm <= 32) {
                        carry = (val & (1ULL << (32 - imm))) != 0;
                    }
                } else {
                    if (imm <= 64) {
                        carry = (val & (1ULL << (64 - imm))) != 0;
                    }
                }
                if (carry) {
                    current_flags |= FLAG_CARRY;
                } else {
                    current_flags &= ~FLAG_CARRY;
                }
            }

            // Overflow Flag (only for 1-bit shifts)
            if (imm == 1) {
                bool msb_before, msb_after;
                if (cpu.is_32bit_mode()) {
                    msb_before = (val & 0x80000000) != 0;
                    msb_after = (masked_result & 0x80000000) != 0;
                } else {
                    msb_before = (val & 0x8000000000000000ULL) != 0;
                    msb_after = (masked_result & 0x8000000000000000ULL) != 0;
                }
                if (msb_before != msb_after) {
                    current_flags |= FLAG_OVERFLOW;
                } else {
                    current_flags &= ~FLAG_OVERFLOW;
                }
            }

            cpu.set_flags(current_flags);
            cpu.set_register_mode_aware(static_cast<Register>(reg), result);

            DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
                "[PC=0x{:04X}] [SAL] R{} << {} = {} (carry={})",
                pc, reg, imm, result,
                (cpu.get_flags() & FLAG_CARRY) ? 1 : 0
            ), DebugLevel::DETAIL);
        }
        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }
    cpu.print_state("SAL");
}

// SAR — Shift Arithmetic Right: sign-extending right shift
void handle_sar(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();

    if (pc + 2 < program.size()) {
        uint8_t reg = program[pc + 1];
        uint8_t imm = program[pc + 2];

        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val = cpu.get_register_mode_aware(static_cast<Register>(reg));

            // Arithmetic right shift: preserve the sign bit
            uint64_t result;
            if (cpu.is_32bit_mode()) {
                int32_t sval = static_cast<int32_t>(val & 0xFFFFFFFF);
                result = static_cast<uint64_t>(static_cast<uint32_t>(sval >> imm));
            } else {
                int64_t sval = static_cast<int64_t>(val);
                result = static_cast<uint64_t>(sval >> imm);
            }

            uint64_t mask = cpu.get_operand_mask();
            uint64_t masked_result = result & mask;

            uint32_t current_flags = cpu.get_flags();

            // Zero Flag
            if (masked_result == 0) {
                current_flags |= FLAG_ZERO;
            } else {
                current_flags &= ~FLAG_ZERO;
            }

            // Sign Flag
            if (cpu.is_32bit_mode()) {
                if ((masked_result & 0x80000000) != 0) current_flags |= FLAG_SIGN;
                else current_flags &= ~FLAG_SIGN;
            } else {
                if ((masked_result & 0x8000000000000000ULL) != 0) current_flags |= FLAG_SIGN;
                else current_flags &= ~FLAG_SIGN;
            }

            // Carry Flag (last bit shifted out)
            if (imm > 0) {
                bool carry = (val & (1ULL << (imm - 1))) != 0;
                if (carry) {
                    current_flags |= FLAG_CARRY;
                } else {
                    current_flags &= ~FLAG_CARRY;
                }
            }

            // Overflow Flag (only for 1-bit shifts)
            if (imm == 1) {
                bool msb;
                if (cpu.is_32bit_mode()) {
                    msb = (val & 0x80000000) != 0;
                } else {
                    msb = (val & 0x8000000000000000ULL) != 0;
                }
                if (msb) {
                    current_flags |= FLAG_OVERFLOW;
                } else {
                    current_flags &= ~FLAG_OVERFLOW;
                }
            }

            cpu.set_flags(current_flags);
            cpu.set_register_mode_aware(static_cast<Register>(reg), result);

            DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
                "[PC=0x{:04X}] [SAR] R{} >> {} (arithmetic) = {} (carry={})",
                pc, reg, imm, static_cast<int64_t>(result),
                (cpu.get_flags() & FLAG_CARRY) ? 1 : 0
            ), DebugLevel::DETAIL);
        }
        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }
    cpu.print_state("SAR");
}

// === Medium-Priority x86-Equivalent Opcodes ===

// CLC — Clear Carry Flag
void handle_clc(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    uint32_t flags = cpu.get_flags();
    flags &= ~FLAG_CARRY;
    cpu.set_flags(flags);
    cpu.set_pc(pc + 1);
    cpu.print_state("CLC");
}

// STC — Set Carry Flag
void handle_stc(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    uint32_t flags = cpu.get_flags();
    flags |= FLAG_CARRY;
    cpu.set_flags(flags);
    cpu.set_pc(pc + 1);
    cpu.print_state("STC");
}

// CMC — Complement Carry Flag
void handle_cmc(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    uint32_t flags = cpu.get_flags();
    flags ^= FLAG_CARRY;
    cpu.set_flags(flags);
    cpu.set_pc(pc + 1);
    cpu.print_state("CMC");
}

// CLD — Clear Direction Flag
void handle_cld(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    uint32_t flags = cpu.get_flags();
    flags &= ~FLAG_DIRECTION;
    cpu.set_flags(flags);
    cpu.set_pc(pc + 1);
    cpu.print_state("CLD");
}

// STD — Set Direction Flag
void handle_std(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    uint32_t flags = cpu.get_flags();
    flags |= FLAG_DIRECTION;
    cpu.set_flags(flags);
    cpu.set_pc(pc + 1);
    cpu.print_state("STD");
}

// LAHF — Load Flags into AH (bits 8-15 of RAX)
// SF:ZF:0:AF:0:PF:1:CF → bit layout: 7:6:5:4:3:2:1:0
void handle_lahf(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    uint32_t flags = cpu.get_flags();
    uint8_t ah = 0;
    if (flags & FLAG_SIGN)      ah |= 0x80;  // SF → bit 7
    if (flags & FLAG_ZERO)      ah |= 0x40;  // ZF → bit 6
    // AF = 0, PF = 0 (not implemented in VM)
    ah |= 0x02;  // bit 1 always 1 in LAHF
    if (flags & FLAG_CARRY)     ah |= 0x01;  // CF → bit 0

    // Place AH into RAX bits 15:8 (bit-shift into position 8)
    uint64_t rax = cpu.get_register_mode_aware(static_cast<Register>(0));
    rax = (rax & 0xFFFFFFFFFFFF00FFULL) | (static_cast<uint64_t>(ah) << 8);
    cpu.set_register_mode_aware(static_cast<Register>(0), rax);
    cpu.set_pc(pc + 1);
    cpu.print_state("LAHF");
}

// SAHF — Store AH into Flags
void handle_sahf(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    uint64_t rax = cpu.get_register_mode_aware(static_cast<Register>(0));
    uint8_t ah = static_cast<uint8_t>((rax >> 8) & 0xFF);

    uint32_t flags = cpu.get_flags();
    // SF ← bit 7
    if (ah & 0x80) flags |= FLAG_SIGN; else flags &= ~FLAG_SIGN;
    // ZF ← bit 6
    if (ah & 0x40) flags |= FLAG_ZERO; else flags &= ~FLAG_ZERO;
    // CF ← bit 0
    if (ah & 0x01) flags |= FLAG_CARRY; else flags &= ~FLAG_CARRY;
    // Bits 5,3,1 ignored (AF, PF, reserved)
    cpu.set_flags(flags);
    cpu.set_pc(pc + 1);
    cpu.print_state("SAHF");
}

// CBW — Convert Byte to Word: sign-extend AL to AX
void handle_cbw(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    uint64_t rax = cpu.get_register_mode_aware(static_cast<Register>(0));
    int8_t al = static_cast<int8_t>(rax & 0xFF);
    int16_t ax = static_cast<int16_t>(al);
    // Place sign-extended value into lower 16 bits, preserve upper bits
    rax = (rax & 0xFFFFFFFFFFFF0000ULL) | (static_cast<uint16_t>(ax));
    cpu.set_register_mode_aware(static_cast<Register>(0), rax);
    cpu.set_pc(pc + 1);
    cpu.print_state("CBW");
}

// CWDE — Convert Word to Doubleword: sign-extend AX to EAX
void handle_cwde(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    uint64_t rax = cpu.get_register_mode_aware(static_cast<Register>(0));
    int16_t ax = static_cast<int16_t>(rax & 0xFFFF);
    int32_t eax = static_cast<int32_t>(ax);
    // Place into lower 32 bits
    rax = (rax & 0xFFFFFFFF00000000ULL) | (static_cast<uint32_t>(eax));
    cpu.set_register_mode_aware(static_cast<Register>(0), rax);
    cpu.set_pc(pc + 1);
    cpu.print_state("CWDE");
}

// CWD — Convert Word to Doubleword: sign-extend AX to DX:AX
void handle_cwd(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    uint64_t rax = cpu.get_register_mode_aware(static_cast<Register>(0));
    int16_t ax = static_cast<int16_t>(rax & 0xFFFF);
    int32_t dword = static_cast<int32_t>(ax);
    // DX gets high 16 bits (sign extension), AX gets low 16 bits
    uint16_t dx_val = static_cast<uint16_t>((dword >> 16) & 0xFFFF);
    uint16_t ax_val = static_cast<uint16_t>(dword & 0xFFFF);
    // DX is register 2 (RDX)
    uint64_t rdx = cpu.get_register_mode_aware(static_cast<Register>(2));
    rdx = (rdx & 0xFFFFFFFFFFFF0000ULL) | dx_val;
    cpu.set_register_mode_aware(static_cast<Register>(2), rdx);
    rax = (rax & 0xFFFFFFFFFFFF0000ULL) | ax_val;
    cpu.set_register_mode_aware(static_cast<Register>(0), rax);
    cpu.set_pc(pc + 1);
    cpu.print_state("CWD");
}

// CDQ — Convert Doubleword to Quadword: sign-extend EAX to EDX:EAX
void handle_cdq(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    uint64_t rax = cpu.get_register_mode_aware(static_cast<Register>(0));
    int32_t eax = static_cast<int32_t>(rax & 0xFFFFFFFF);
    int64_t qword = static_cast<int64_t>(eax);
    // EDX (R2) gets high 32 bits, EAX gets low 32 bits
    uint32_t edx_val = static_cast<uint32_t>((qword >> 32) & 0xFFFFFFFF);
    uint32_t eax_val = static_cast<uint32_t>(qword & 0xFFFFFFFF);
    uint64_t rdx = cpu.get_register_mode_aware(static_cast<Register>(2));
    rdx = (rdx & 0xFFFFFFFF00000000ULL) | edx_val;
    cpu.set_register_mode_aware(static_cast<Register>(2), rdx);
    rax = (rax & 0xFFFFFFFF00000000ULL) | eax_val;
    cpu.set_register_mode_aware(static_cast<Register>(0), rax);
    cpu.set_pc(pc + 1);
    cpu.print_state("CDQ");
}

// ROL — Rotate Left: bits shifted out on left come back in on right
void handle_rol(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    if (pc + 2 < program.size()) {
        uint8_t reg = program[pc + 1];
        uint8_t imm = program[pc + 2];
        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val = cpu.get_register_mode_aware(static_cast<Register>(reg));
            uint64_t mask = cpu.get_operand_mask();
            uint32_t current_flags = cpu.get_flags();

            uint64_t effective_shift = imm;
            if (cpu.is_32bit_mode()) {
                effective_shift = imm & 0x1F;  // Mask to 5 bits for 32-bit
                uint32_t val32 = static_cast<uint32_t>(val);
                uint32_t result32 = (val32 << effective_shift) | (val32 >> (32 - effective_shift));
                uint64_t result = (val & 0xFFFFFFFF00000000ULL) | result32;
                uint32_t masked = result32 & 0xFFFFFFFF;

                if (imm > 0 && ((masked & 1) != 0)) current_flags |= FLAG_CARRY;
                else if (imm > 0) current_flags &= ~FLAG_CARRY;

                if (masked == 0) current_flags |= FLAG_ZERO;
                else current_flags &= ~FLAG_ZERO;

                cpu.set_register_mode_aware(static_cast<Register>(reg), result);
            } else {
                effective_shift = imm & 0x3F;  // Mask to 6 bits for 64-bit
                uint64_t result = (val << effective_shift) | (val >> (64 - effective_shift));
                uint64_t masked = result & mask;

                if (imm > 0 && ((masked & 1) != 0)) current_flags |= FLAG_CARRY;
                else if (imm > 0) current_flags &= ~FLAG_CARRY;

                if (masked == 0) current_flags |= FLAG_ZERO;
                else current_flags &= ~FLAG_ZERO;

                cpu.set_register_mode_aware(static_cast<Register>(reg), result);
            }

            // ROL: overflow flag undefined for multi-bit, defined for 1-bit
            if (imm == 1) {
                bool msb_before, msb_after;
                if (cpu.is_32bit_mode()) {
                    msb_before = (val & 0x80000000) != 0;
                    msb_after = (cpu.get_register_mode_aware(static_cast<Register>(reg)) & 0x80000000) != 0;
                } else {
                    msb_before = (val & 0x8000000000000000ULL) != 0;
                    msb_after = (cpu.get_register_mode_aware(static_cast<Register>(reg)) & 0x8000000000000000ULL) != 0;
                }
                if (msb_before != (current_flags & FLAG_CARRY ? true : false)) {
                    current_flags |= FLAG_OVERFLOW;
                } else {
                    current_flags &= ~FLAG_OVERFLOW;
                }
            }

            cpu.set_flags(current_flags);
        }
        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }
    cpu.print_state("ROL");
}

// ROR — Rotate Right: bits shifted out on right come back in on left
void handle_ror(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    if (pc + 2 < program.size()) {
        uint8_t reg = program[pc + 1];
        uint8_t imm = program[pc + 2];
        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val = cpu.get_register_mode_aware(static_cast<Register>(reg));
            uint64_t mask = cpu.get_operand_mask();
            uint32_t current_flags = cpu.get_flags();

            uint64_t effective_shift = imm;
            if (cpu.is_32bit_mode()) {
                effective_shift = imm & 0x1F;
                uint32_t val32 = static_cast<uint32_t>(val);
                uint32_t result32 = (val32 >> effective_shift) | (val32 << (32 - effective_shift));
                uint64_t result = (val & 0xFFFFFFFF00000000ULL) | result32;

                if (imm > 0 && ((result32 >> 31) & 1)) current_flags |= FLAG_CARRY;
                else if (imm > 0) current_flags &= ~FLAG_CARRY;

                if (result32 == 0) current_flags |= FLAG_ZERO;
                else current_flags &= ~FLAG_ZERO;

                cpu.set_register_mode_aware(static_cast<Register>(reg), result);
            } else {
                effective_shift = imm & 0x3F;
                uint64_t result = (val >> effective_shift) | (val << (64 - effective_shift));
                uint64_t masked = result & mask;

                if (imm > 0 && ((result >> 63) & 1)) current_flags |= FLAG_CARRY;
                else if (imm > 0) current_flags &= ~FLAG_CARRY;

                if (masked == 0) current_flags |= FLAG_ZERO;
                else current_flags &= ~FLAG_ZERO;

                cpu.set_register_mode_aware(static_cast<Register>(reg), result);
            }

            if (imm == 1) {
                bool msb_before, msb_after;
                if (cpu.is_32bit_mode()) {
                    msb_before = (val & 0x80000000) != 0;
                    msb_after = (cpu.get_register_mode_aware(static_cast<Register>(reg)) & 0x80000000) != 0;
                } else {
                    msb_before = (val & 0x8000000000000000ULL) != 0;
                    msb_after = (cpu.get_register_mode_aware(static_cast<Register>(reg)) & 0x8000000000000000ULL) != 0;
                }
                if (msb_before != msb_after) {
                    current_flags |= FLAG_OVERFLOW;
                } else {
                    current_flags &= ~FLAG_OVERFLOW;
                }
            }

            cpu.set_flags(current_flags);
        }
        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }
    cpu.print_state("ROR");
}

// LOOP — Decrement ECX (R1), jump to addr if ECX != 0
void handle_loop(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    if (pc + 4 < program.size()) {
        uint32_t addr = static_cast<uint32_t>(program[pc + 1]) |
                       (static_cast<uint32_t>(program[pc + 2]) << 8) |
                       (static_cast<uint32_t>(program[pc + 3]) << 16) |
                       (static_cast<uint32_t>(program[pc + 4]) << 24);
        // ECX = register 1
        uint64_t ecx = cpu.get_register_mode_aware(static_cast<Register>(1));
        ecx = (ecx & cpu.get_operand_mask()) - 1;
        cpu.set_register_mode_aware(static_cast<Register>(1), ecx);

        if ((ecx & cpu.get_operand_mask()) != 0) {
            if (addr >= program.size()) { running = false; return; }
            cpu.set_pc(addr);
        } else {
            cpu.set_pc(pc + 5);
        }
    } else {
        running = false;
    }
    cpu.print_state("LOOP");
}

// LOOPE — Decrement ECX, jump if ECX != 0 AND ZF=1
void handle_loope(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    if (pc + 4 < program.size()) {
        uint32_t addr = static_cast<uint32_t>(program[pc + 1]) |
                       (static_cast<uint32_t>(program[pc + 2]) << 8) |
                       (static_cast<uint32_t>(program[pc + 3]) << 16) |
                       (static_cast<uint32_t>(program[pc + 4]) << 24);
        uint64_t ecx = cpu.get_register_mode_aware(static_cast<Register>(1));
        ecx = (ecx & cpu.get_operand_mask()) - 1;
        cpu.set_register_mode_aware(static_cast<Register>(1), ecx);

        if ((ecx & cpu.get_operand_mask()) != 0 && (cpu.get_flags() & FLAG_ZERO)) {
            if (addr >= program.size()) { running = false; return; }
            cpu.set_pc(addr);
        } else {
            cpu.set_pc(pc + 5);
        }
    } else {
        running = false;
    }
    cpu.print_state("LOOPE");
}

// LOOPNE — Decrement ECX, jump if ECX != 0 AND ZF=0
void handle_loopne(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    if (pc + 4 < program.size()) {
        uint32_t addr = static_cast<uint32_t>(program[pc + 1]) |
                       (static_cast<uint32_t>(program[pc + 2]) << 8) |
                       (static_cast<uint32_t>(program[pc + 3]) << 16) |
                       (static_cast<uint32_t>(program[pc + 4]) << 24);
        uint64_t ecx = cpu.get_register_mode_aware(static_cast<Register>(1));
        ecx = (ecx & cpu.get_operand_mask()) - 1;
        cpu.set_register_mode_aware(static_cast<Register>(1), ecx);

        if ((ecx & cpu.get_operand_mask()) != 0 && !(cpu.get_flags() & FLAG_ZERO)) {
            if (addr >= program.size()) { running = false; return; }
            cpu.set_pc(addr);
        } else {
            cpu.set_pc(pc + 5);
        }
    } else {
        running = false;
    }
    cpu.print_state("LOOPNE");
}

// RCL — Rotate Left through Carry: CF acts as an extra bit
void handle_rcl(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    if (pc + 2 < program.size()) {
        uint8_t reg = program[pc + 1];
        uint8_t count = program[pc + 2];
        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val = cpu.get_register_mode_aware(static_cast<Register>(reg));
            uint32_t flags = cpu.get_flags();
            bool old_cf = (flags & FLAG_CARRY) != 0;
            uint64_t mask = cpu.get_operand_mask();

            uint64_t result;
            bool new_cf = false;

            if (cpu.is_32bit_mode()) {
                uint8_t eff_count = (count & 0x1F) % 33;  // 33-bit rotation (32 + CF)
                if (eff_count == 0) {
                    result = val;
                    new_cf = old_cf;
                } else {
                    uint64_t temp = (static_cast<uint64_t>(val & 0xFFFFFFFF) << 1) | (old_cf ? 1ULL : 0);
                    for (uint8_t i = 1; i < eff_count; i++) {
                        uint64_t msb = (temp >> 32) & 1;
                        temp = ((temp & 0xFFFFFFFFULL) << 1) | msb;
                    }
                    result = (val & 0xFFFFFFFF00000000ULL) | (temp & 0xFFFFFFFF);
                    new_cf = ((temp >> 32) & 1) != 0;
                }
            } else {
                uint8_t eff_count = (count & 0x3F) % 65;  // 65-bit rotation (64 + CF)
                if (eff_count == 0) {
                    result = val;
                    new_cf = old_cf;
                } else {
                    // Build 65-bit value: (CF << 64) | val
                    unsigned __int128 temp = (static_cast<unsigned __int128>(val)) |
                                             (static_cast<unsigned __int128>(old_cf ? 1 : 0) << 64);
                    // Rotate left by eff_count in 65-bit space
                    unsigned __int128 rotated = ((temp << eff_count) | (temp >> (65 - eff_count))) & (((unsigned __int128)1 << 65) - 1);
                    result = static_cast<uint64_t>(rotated & 0xFFFFFFFFFFFFFFFFULL);
                    new_cf = ((rotated >> 64) & 1) != 0;
                }
            }

            // Set flags
            if ((result & mask) == 0) flags |= FLAG_ZERO; else flags &= ~FLAG_ZERO;
            if (new_cf) flags |= FLAG_CARRY; else flags &= ~FLAG_CARRY;
            // OF: only defined for single-bit RCL (CF != new MSB)
            if (count == 1) {
                bool new_msb;
                if (cpu.is_32bit_mode()) new_msb = (result & 0x80000000) != 0;
                else new_msb = (result & 0x8000000000000000ULL) != 0;
                if (new_msb != new_cf) flags |= FLAG_OVERFLOW;
                else flags &= ~FLAG_OVERFLOW;
            }

            cpu.set_flags(flags);
            cpu.set_register_mode_aware(static_cast<Register>(reg), result);
        }
        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }
    cpu.print_state("RCL");
}

// RCR — Rotate Right through Carry: CF acts as an extra bit
void handle_rcr(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    if (pc + 2 < program.size()) {
        uint8_t reg = program[pc + 1];
        uint8_t count = program[pc + 2];
        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val = cpu.get_register_mode_aware(static_cast<Register>(reg));
            uint32_t flags = cpu.get_flags();
            bool old_cf = (flags & FLAG_CARRY) != 0;
            uint64_t mask = cpu.get_operand_mask();

            uint64_t result;
            bool new_cf = false;

            if (cpu.is_32bit_mode()) {
                uint8_t eff_count = (count & 0x1F) % 33;
                if (eff_count == 0) {
                    result = val;
                    new_cf = old_cf;
                } else {
                    // Build 33-bit value: val | (CF << 32)
                    uint64_t temp = (static_cast<uint64_t>(val & 0xFFFFFFFF)) |
                                    (static_cast<uint64_t>(old_cf ? 1 : 0) << 32);
                    // Rotate right by eff_count in 33-bit space
                    uint64_t rotated = ((temp >> eff_count) | (temp << (33 - eff_count))) & 0x1FFFFFFFFULL;
                    result = (val & 0xFFFFFFFF00000000ULL) | (rotated & 0xFFFFFFFF);
                    new_cf = ((rotated >> 32) & 1) != 0;
                }
            } else {
                uint8_t eff_count = (count & 0x3F) % 65;
                if (eff_count == 0) {
                    result = val;
                    new_cf = old_cf;
                } else {
                    unsigned __int128 temp = (static_cast<unsigned __int128>(val)) |
                                             (static_cast<unsigned __int128>(old_cf ? 1 : 0) << 64);
                    unsigned __int128 rotated = ((temp >> eff_count) | (temp << (65 - eff_count))) & (((unsigned __int128)1 << 65) - 1);
                    result = static_cast<uint64_t>(rotated & 0xFFFFFFFFFFFFFFFFULL);
                    new_cf = ((rotated >> 64) & 1) != 0;
                }
            }

            if ((result & mask) == 0) flags |= FLAG_ZERO; else flags &= ~FLAG_ZERO;
            if (new_cf) flags |= FLAG_CARRY; else flags &= ~FLAG_CARRY;
            if (count == 1) {
                bool new_msb;
                if (cpu.is_32bit_mode()) new_msb = (result & 0x80000000) != 0;
                else new_msb = (result & 0x8000000000000000ULL) != 0;
                bool old_msb;
                if (cpu.is_32bit_mode()) old_msb = (val & 0x80000000) != 0;
                else old_msb = (val & 0x8000000000000000ULL) != 0;
                if (new_msb != old_msb) flags |= FLAG_OVERFLOW;
                else flags &= ~FLAG_OVERFLOW;
            }

            cpu.set_flags(flags);
            cpu.set_register_mode_aware(static_cast<Register>(reg), result);
        }
        cpu.set_pc(pc + 3);
    } else {
        running = false;
    }
    cpu.print_state("RCR");
}

// === SETcc — Conditional Set Byte (0xEB-0xF6) ===

// Helper for SETcc: check condition, write 1 or 0 to register
static void setcc_impl(CPU& cpu, const std::vector<uint8_t>& program, bool& running,
                        bool condition, const char* name) {
    uint32_t pc = cpu.get_pc();
    if (pc + 1 < program.size()) {
        uint8_t reg = program[pc + 1];
        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val = cpu.get_register_mode_aware(static_cast<Register>(reg));
            uint64_t mask = cpu.get_operand_mask();
            // Clear lowest byte, then set to 0 or 1
            val = (val & ~0xFFULL) | (condition ? 1ULL : 0ULL);
            cpu.set_register_mode_aware(static_cast<Register>(reg), val & mask);
            DebugHandler::instance().report(DebugCategory::CPU_EXECUTION, fmt::format(
                "[PC=0x{:04X}] [{}] R{} = {} (condition={})",
                pc, name, reg, condition ? 1 : 0, condition ? 1 : 0
            ), DebugLevel::DETAIL);
        }
        cpu.set_pc(pc + 2);
    } else {
        running = false;
    }
    cpu.print_state(name);
}

void handle_setz(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    setcc_impl(cpu, program, running, (cpu.get_flags() & FLAG_ZERO) != 0, "SETZ");
}
void handle_setnz(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    setcc_impl(cpu, program, running, (cpu.get_flags() & FLAG_ZERO) == 0, "SETNZ");
}
void handle_setc(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    setcc_impl(cpu, program, running, (cpu.get_flags() & FLAG_CARRY) != 0, "SETC");
}
void handle_setnc(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    setcc_impl(cpu, program, running, (cpu.get_flags() & FLAG_CARRY) == 0, "SETNC");
}
void handle_seto(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    setcc_impl(cpu, program, running, (cpu.get_flags() & FLAG_OVERFLOW) != 0, "SETO");
}
void handle_setno(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    setcc_impl(cpu, program, running, (cpu.get_flags() & FLAG_OVERFLOW) == 0, "SETNO");
}
void handle_sets(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    setcc_impl(cpu, program, running, (cpu.get_flags() & FLAG_SIGN) != 0, "SETS");
}
void handle_setns(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    setcc_impl(cpu, program, running, (cpu.get_flags() & FLAG_SIGN) == 0, "SETNS");
}
void handle_setg(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t f = cpu.get_flags();
    bool zf = (f & FLAG_ZERO) != 0;
    bool sf = (f & FLAG_SIGN) != 0;
    bool of = (f & FLAG_OVERFLOW) != 0;
    setcc_impl(cpu, program, running, !zf && (sf == of), "SETG");
}
void handle_setge(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t f = cpu.get_flags();
    bool sf = (f & FLAG_SIGN) != 0;
    bool of = (f & FLAG_OVERFLOW) != 0;
    setcc_impl(cpu, program, running, sf == of, "SETGE");
}
void handle_setl(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t f = cpu.get_flags();
    bool sf = (f & FLAG_SIGN) != 0;
    bool of = (f & FLAG_OVERFLOW) != 0;
    setcc_impl(cpu, program, running, sf != of, "SETL");
}
void handle_setle(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t f = cpu.get_flags();
    bool zf = (f & FLAG_ZERO) != 0;
    bool sf = (f & FLAG_SIGN) != 0;
    bool of = (f & FLAG_OVERFLOW) != 0;
    setcc_impl(cpu, program, running, zf || (sf != of), "SETLE");
}

// XCHG — Exchange register contents
void handle_xchg(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    if (pc + 2 < program.size()) {
        uint8_t reg1 = program[pc + 1];
        uint8_t reg2 = program[pc + 2];
        if (reg1 < DemiEngine_Registers::TOTAL_REGISTERS && reg2 < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t v1 = cpu.get_register_mode_aware(static_cast<Register>(reg1));
            uint64_t v2 = cpu.get_register_mode_aware(static_cast<Register>(reg2));
            cpu.set_register_mode_aware(static_cast<Register>(reg1), v2);
            cpu.set_register_mode_aware(static_cast<Register>(reg2), v1);
        }
        cpu.set_pc(pc + 3);
    } else { running = false; }
    cpu.print_state("XCHG");
}

// BSWAP — Byte Swap
void handle_bswap(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    if (pc + 1 < program.size()) {
        uint8_t reg = program[pc + 1];
        if (reg < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t val = cpu.get_register_mode_aware(static_cast<Register>(reg));
            uint64_t result;
            if (cpu.is_32bit_mode()) {
                uint32_t v = static_cast<uint32_t>(val);
                v = ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) | ((v & 0xFF0000) >> 8) | ((v & 0xFF000000) >> 24);
                result = (val & 0xFFFFFFFF00000000ULL) | v;
            } else {
                result = ((val & 0xFFULL) << 56) | ((val & 0xFF00ULL) << 40) | ((val & 0xFF0000ULL) << 24) |
                         ((val & 0xFF000000ULL) << 8) | ((val & 0xFF00000000ULL) >> 8) |
                         ((val & 0xFF0000000000ULL) >> 24) | ((val & 0xFF000000000000ULL) >> 40) |
                         ((val & 0xFF00000000000000ULL) >> 56);
            }
            cpu.set_register_mode_aware(static_cast<Register>(reg), result);
        }
        cpu.set_pc(pc + 2);
    } else { running = false; }
    cpu.print_state("BSWAP");
}

// MOVSX — Move with Sign Extension
void handle_movsx(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    if (pc + 2 < program.size()) {
        uint8_t dst = program[pc + 1], src = program[pc + 2];
        if (dst < DemiEngine_Registers::TOTAL_REGISTERS && src < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t sval = cpu.get_register_mode_aware(static_cast<Register>(src));
            uint64_t result;
            if (cpu.is_32bit_mode()) {
                result = static_cast<uint64_t>(static_cast<uint32_t>(static_cast<int8_t>(sval & 0xFF)));
            } else {
                result = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(sval & 0xFFFFFFFF)));
            }
            cpu.set_register_mode_aware(static_cast<Register>(dst), result);
            uint32_t flags = cpu.get_flags();
            if ((result & cpu.get_operand_mask()) == 0) flags |= FLAG_ZERO; else flags &= ~FLAG_ZERO;
            if (cpu.is_32bit_mode() ? (result & 0x80000000) : (result & 0x8000000000000000ULL)) flags |= FLAG_SIGN; else flags &= ~FLAG_SIGN;
            cpu.set_flags(flags);
        }
        cpu.set_pc(pc + 3);
    } else { running = false; }
    cpu.print_state("MOVSX");
}

// MOVZX — Move with Zero Extension
void handle_movzx(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    if (pc + 2 < program.size()) {
        uint8_t dst = program[pc + 1], src = program[pc + 2];
        if (dst < DemiEngine_Registers::TOTAL_REGISTERS && src < DemiEngine_Registers::TOTAL_REGISTERS) {
            uint64_t sval = cpu.get_register_mode_aware(static_cast<Register>(src));
            uint64_t result = cpu.is_32bit_mode() ? (sval & 0xFF) : (sval & 0xFFFFFFFF);
            cpu.set_register_mode_aware(static_cast<Register>(dst), result);
            uint32_t flags = cpu.get_flags();
            if (result == 0) flags |= FLAG_ZERO; else flags &= ~FLAG_ZERO;
            flags &= ~FLAG_SIGN;
            cpu.set_flags(flags);
        }
        cpu.set_pc(pc + 3);
    } else { running = false; }
    cpu.print_state("MOVZX");
}

// === CMOVcc — Conditional Move (14 handlers, 16 mnemonics) ===

static void cmovcc_impl(CPU& cpu, const std::vector<uint8_t>& program, bool& running,
                         bool condition, const char* name) {
    uint32_t pc = cpu.get_pc();
    if (pc + 2 < program.size()) {
        uint8_t dst = program[pc + 1], src = program[pc + 2];
        if (dst < DemiEngine_Registers::TOTAL_REGISTERS && src < DemiEngine_Registers::TOTAL_REGISTERS) {
            if (condition) {
                uint64_t val = cpu.get_register_mode_aware(static_cast<Register>(src));
                cpu.set_register_mode_aware(static_cast<Register>(dst), val);
            }
        }
        cpu.set_pc(pc + 3);
    } else { running = false; }
    cpu.print_state(name);
}

void handle_cmovo(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    cmovcc_impl(cpu, program, running, (cpu.get_flags() & FLAG_OVERFLOW) != 0, "CMOVO");
}
void handle_cmovno(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    cmovcc_impl(cpu, program, running, (cpu.get_flags() & FLAG_OVERFLOW) == 0, "CMOVNO");
}
void handle_cmovc(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    cmovcc_impl(cpu, program, running, (cpu.get_flags() & FLAG_CARRY) != 0, "CMOVC");
}
void handle_cmovnc(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    cmovcc_impl(cpu, program, running, (cpu.get_flags() & FLAG_CARRY) == 0, "CMOVNC");
}
void handle_cmovz(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    cmovcc_impl(cpu, program, running, (cpu.get_flags() & FLAG_ZERO) != 0, "CMOVZ");
}
void handle_cmovnz(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    cmovcc_impl(cpu, program, running, (cpu.get_flags() & FLAG_ZERO) == 0, "CMOVNZ");
}
void handle_cmovs(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    cmovcc_impl(cpu, program, running, (cpu.get_flags() & FLAG_SIGN) != 0, "CMOVS");
}
void handle_cmovns(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    cmovcc_impl(cpu, program, running, (cpu.get_flags() & FLAG_SIGN) == 0, "CMOVNS");
}
void handle_cmovg(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t f = cpu.get_flags();
    cmovcc_impl(cpu, program, running,
        !(f & FLAG_ZERO) && ((f & FLAG_SIGN) != 0) == ((f & FLAG_OVERFLOW) != 0), "CMOVG");
}
void handle_cmovge(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t f = cpu.get_flags();
    cmovcc_impl(cpu, program, running, ((f & FLAG_SIGN) != 0) == ((f & FLAG_OVERFLOW) != 0), "CMOVGE");
}
void handle_cmovl(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t f = cpu.get_flags();
    cmovcc_impl(cpu, program, running, ((f & FLAG_SIGN) != 0) != ((f & FLAG_OVERFLOW) != 0), "CMOVL");
}
void handle_cmovle(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t f = cpu.get_flags();
    cmovcc_impl(cpu, program, running,
        (f & FLAG_ZERO) || (((f & FLAG_SIGN) != 0) != ((f & FLAG_OVERFLOW) != 0)), "CMOVLE");
}
void handle_cmova(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t f = cpu.get_flags();
    cmovcc_impl(cpu, program, running, !(f & FLAG_CARRY) && !(f & FLAG_ZERO), "CMOVA");
}
void handle_cmovbe(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t f = cpu.get_flags();
    cmovcc_impl(cpu, program, running, (f & FLAG_CARRY) || (f & FLAG_ZERO), "CMOVBE");
}

// === String Operations ===
static int string_step(CPU& cpu) {
    return (cpu.get_flags() & FLAG_DIRECTION) ? -1 : 1;
}
void handle_movsb(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc(); auto& mem = cpu.get_memory();
    uint64_t esi = cpu.get_register_mode_aware(static_cast<Register>(6));
    uint64_t edi = cpu.get_register_mode_aware(static_cast<Register>(7));
    int step = string_step(cpu);
    uint32_t si=esi&0xFFFFFFFF, di=edi&0xFFFFFFFF;
    if(si<mem.size()&&di<mem.size()) mem[di]=mem[si];
    cpu.set_register_mode_aware(static_cast<Register>(6),esi+step);
    cpu.set_register_mode_aware(static_cast<Register>(7),edi+step);
    cpu.set_pc(pc+1); cpu.print_state("MOVSB");
}
void handle_movsw(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc(); auto& mem = cpu.get_memory();
    uint64_t esi=cpu.get_register_mode_aware(static_cast<Register>(6));
    uint64_t edi=cpu.get_register_mode_aware(static_cast<Register>(7));
    int step=string_step(cpu)*2;
    uint32_t si=esi&0xFFFFFFFF,di=edi&0xFFFFFFFF;
    if(si+1<mem.size()&&di+1<mem.size()){mem[di]=mem[si];mem[di+1]=mem[si+1];}
    cpu.set_register_mode_aware(static_cast<Register>(6),esi+step);
    cpu.set_register_mode_aware(static_cast<Register>(7),edi+step);
    cpu.set_pc(pc+1); cpu.print_state("MOVSW");
}
void handle_movsd(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc=cpu.get_pc(); auto& mem=cpu.get_memory();
    uint64_t esi=cpu.get_register_mode_aware(static_cast<Register>(6));
    uint64_t edi=cpu.get_register_mode_aware(static_cast<Register>(7));
    int step=string_step(cpu)*4;
    uint32_t si=esi&0xFFFFFFFF,di=edi&0xFFFFFFFF;
    if(si+3<mem.size()&&di+3<mem.size()){for(int i=0;i<4;i++)mem[di+i]=mem[si+i];}
    cpu.set_register_mode_aware(static_cast<Register>(6),esi+step);
    cpu.set_register_mode_aware(static_cast<Register>(7),edi+step);
    cpu.set_pc(pc+1); cpu.print_state("MOVSD");
}
void handle_stosb(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc=cpu.get_pc(); auto& mem=cpu.get_memory();
    uint64_t edi=cpu.get_register_mode_aware(static_cast<Register>(7));
    uint8_t al=cpu.get_register_mode_aware(static_cast<Register>(0))&0xFF;
    int step=string_step(cpu); uint32_t di=edi&0xFFFFFFFF;
    if(di<mem.size())mem[di]=al;
    cpu.set_register_mode_aware(static_cast<Register>(7),edi+step);
    cpu.set_pc(pc+1); cpu.print_state("STOSB");
}
void handle_stosw(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc=cpu.get_pc(); auto& mem=cpu.get_memory();
    uint64_t edi=cpu.get_register_mode_aware(static_cast<Register>(7));
    uint16_t ax=cpu.get_register_mode_aware(static_cast<Register>(0))&0xFFFF;
    int step=string_step(cpu)*2; uint32_t di=edi&0xFFFFFFFF;
    if(di+1<mem.size()){mem[di]=(uint8_t)ax;mem[di+1]=(uint8_t)(ax>>8);}
    cpu.set_register_mode_aware(static_cast<Register>(7),edi+step);
    cpu.set_pc(pc+1); cpu.print_state("STOSW");
}
void handle_stosd(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc=cpu.get_pc(); auto& mem=cpu.get_memory();
    uint64_t edi=cpu.get_register_mode_aware(static_cast<Register>(7));
    uint32_t eax=cpu.get_register_mode_aware(static_cast<Register>(0))&0xFFFFFFFF;
    int step=string_step(cpu)*4; uint32_t di=edi&0xFFFFFFFF;
    if(di+3<mem.size()){for(int i=0;i<4;i++)mem[di+i]=(eax>>(i*8))&0xFF;}
    cpu.set_register_mode_aware(static_cast<Register>(7),edi+step);
    cpu.set_pc(pc+1); cpu.print_state("STOSD");
}
void handle_lodsb(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc=cpu.get_pc(); auto& mem=cpu.get_memory();
    uint64_t esi=cpu.get_register_mode_aware(static_cast<Register>(6));
    int step=string_step(cpu); uint32_t si=esi&0xFFFFFFFF;
    uint64_t rax=cpu.get_register_mode_aware(static_cast<Register>(0));
    if(si<mem.size())rax=(rax&~0xFFULL)|mem[si];
    cpu.set_register_mode_aware(static_cast<Register>(0),rax);
    cpu.set_register_mode_aware(static_cast<Register>(6),esi+step);
    cpu.set_pc(pc+1); cpu.print_state("LODSB");
}
void handle_lodsw(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc=cpu.get_pc(); auto& mem=cpu.get_memory();
    uint64_t esi=cpu.get_register_mode_aware(static_cast<Register>(6));
    int step=string_step(cpu)*2; uint32_t si=esi&0xFFFFFFFF;
    uint64_t rax=cpu.get_register_mode_aware(static_cast<Register>(0));
    if(si+1<mem.size()){uint16_t v=mem[si]|((uint16_t)mem[si+1]<<8);rax=(rax&~0xFFFFULL)|v;}
    cpu.set_register_mode_aware(static_cast<Register>(0),rax);
    cpu.set_register_mode_aware(static_cast<Register>(6),esi+step);
    cpu.set_pc(pc+1); cpu.print_state("LODSW");
}
void handle_lodsd(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc=cpu.get_pc(); auto& mem=cpu.get_memory();
    uint64_t esi=cpu.get_register_mode_aware(static_cast<Register>(6));
    int step=string_step(cpu)*4; uint32_t si=esi&0xFFFFFFFF;
    if(si+3<mem.size()){uint32_t v=mem[si]|((uint32_t)mem[si+1]<<8)|((uint32_t)mem[si+2]<<16)|((uint32_t)mem[si+3]<<24);uint64_t rax=cpu.get_register_mode_aware(static_cast<Register>(0));rax=(rax&0xFFFFFFFF00000000ULL)|v;cpu.set_register_mode_aware(static_cast<Register>(0),rax);}
    cpu.set_register_mode_aware(static_cast<Register>(6),esi+step);
    cpu.set_pc(pc+1); cpu.print_state("LODSD");
}

// === Bit Test + Atomics ===
void handle_bt(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc=cpu.get_pc(); uint32_t f=cpu.get_flags();
    if(pc+2<program.size()){uint8_t r=program[pc+1];uint8_t bit=program[pc+2];
    if(r<DemiEngine_Registers::TOTAL_REGISTERS){uint64_t v=cpu.get_register_mode_aware(static_cast<Register>(r));
    uint64_t mask=cpu.get_operand_mask();uint8_t pos=bit&(cpu.is_32bit_mode()?0x1F:0x3F);
    if(v&(1ULL<<pos))f|=FLAG_CARRY;else f&=~FLAG_CARRY;cpu.set_flags(f);}
    cpu.set_pc(pc+3);}else running=false;cpu.print_state("BT");}
void handle_bts(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc=cpu.get_pc();uint32_t f=cpu.get_flags();
    if(pc+2<program.size()){uint8_t r=program[pc+1];uint8_t bit=program[pc+2];
    if(r<DemiEngine_Registers::TOTAL_REGISTERS){uint64_t v=cpu.get_register_mode_aware(static_cast<Register>(r));
    uint64_t mask=cpu.get_operand_mask();uint8_t pos=bit&(cpu.is_32bit_mode()?0x1F:0x3F);
    if(v&(1ULL<<pos))f|=FLAG_CARRY;else f&=~FLAG_CARRY;
    v|=(1ULL<<pos);cpu.set_register_mode_aware(static_cast<Register>(r),v&mask);cpu.set_flags(f);}
    cpu.set_pc(pc+3);}else running=false;cpu.print_state("BTS");}
void handle_btr(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc=cpu.get_pc();uint32_t f=cpu.get_flags();
    if(pc+2<program.size()){uint8_t r=program[pc+1];uint8_t bit=program[pc+2];
    if(r<DemiEngine_Registers::TOTAL_REGISTERS){uint64_t v=cpu.get_register_mode_aware(static_cast<Register>(r));
    uint64_t mask=cpu.get_operand_mask();uint8_t pos=bit&(cpu.is_32bit_mode()?0x1F:0x3F);
    if(v&(1ULL<<pos))f|=FLAG_CARRY;else f&=~FLAG_CARRY;
    v&=~(1ULL<<pos);cpu.set_register_mode_aware(static_cast<Register>(r),v&mask);cpu.set_flags(f);}
    cpu.set_pc(pc+3);}else running=false;cpu.print_state("BTR");}
void handle_btc(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc=cpu.get_pc();uint32_t f=cpu.get_flags();
    if(pc+2<program.size()){uint8_t r=program[pc+1];uint8_t bit=program[pc+2];
    if(r<DemiEngine_Registers::TOTAL_REGISTERS){uint64_t v=cpu.get_register_mode_aware(static_cast<Register>(r));
    uint64_t mask=cpu.get_operand_mask();uint8_t pos=bit&(cpu.is_32bit_mode()?0x1F:0x3F);
    if(v&(1ULL<<pos))f|=FLAG_CARRY;else f&=~FLAG_CARRY;
    v^=(1ULL<<pos);cpu.set_register_mode_aware(static_cast<Register>(r),v&mask);cpu.set_flags(f);}
    cpu.set_pc(pc+3);}else running=false;cpu.print_state("BTC");}
void handle_cmpxchg(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc=cpu.get_pc();
    if(pc+2<program.size()){uint8_t dst=program[pc+1];uint8_t src=program[pc+2];
    if(dst<DemiEngine_Registers::TOTAL_REGISTERS&&src<DemiEngine_Registers::TOTAL_REGISTERS){
    uint64_t eax=cpu.get_register_mode_aware(static_cast<Register>(0));
    uint64_t dval=cpu.get_register_mode_aware(static_cast<Register>(dst));
    uint64_t sval=cpu.get_register_mode_aware(static_cast<Register>(src));
    uint64_t mask=cpu.get_operand_mask();uint32_t f=cpu.get_flags();
    uint64_t diff=(eax&mask)-(dval&mask);uint64_t mdiff=diff&mask;
    if(mdiff==0)f|=FLAG_ZERO;else f&=~FLAG_ZERO;
    bool borrow;if(cpu.is_32bit_mode())borrow=((uint32_t)(eax&mask)<(uint32_t)(dval&mask));
    else borrow=((eax&mask)<(dval&mask));
    if(borrow)f|=FLAG_CARRY;else f&=~FLAG_CARRY;
    bool s1,s2,sr;if(cpu.is_32bit_mode()){s1=(eax&0x80000000)!=0;s2=(dval&0x80000000)!=0;sr=(mdiff&0x80000000)!=0;}
    else{s1=(eax&0x8000000000000000ULL)!=0;s2=(dval&0x8000000000000000ULL)!=0;sr=(mdiff&0x8000000000000000ULL)!=0;}
    if((s1!=s2)&&(s1!=sr))f|=FLAG_OVERFLOW;else f&=~FLAG_OVERFLOW;
    if(mdiff==0){cpu.set_register_mode_aware(static_cast<Register>(dst),sval);}
    else{cpu.set_register_mode_aware(static_cast<Register>(0),dval);}
    cpu.set_flags(f);}
    cpu.set_pc(pc+3);}else running=false;cpu.print_state("CMPXCHG");}
void handle_xadd(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc=cpu.get_pc();
    if(pc+2<program.size()){uint8_t dst=program[pc+1];uint8_t src=program[pc+2];
    if(dst<DemiEngine_Registers::TOTAL_REGISTERS&&src<DemiEngine_Registers::TOTAL_REGISTERS){
    uint64_t dval=cpu.get_register_mode_aware(static_cast<Register>(dst));
    uint64_t sval=cpu.get_register_mode_aware(static_cast<Register>(src));
    uint64_t sum=dval+sval;uint64_t mask=cpu.get_operand_mask();uint64_t msum=sum&mask;
    uint32_t f=cpu.get_flags();
    if(msum==0)f|=FLAG_ZERO;else f&=~FLAG_ZERO;
    bool carry;if(cpu.is_32bit_mode())carry=(sum>0xFFFFFFFF);else carry=(sum<dval);
    if(carry)f|=FLAG_CARRY;else f&=~FLAG_CARRY;
    bool s1,s2,sr;if(cpu.is_32bit_mode()){s1=(dval&0x80000000)!=0;s2=(sval&0x80000000)!=0;sr=(msum&0x80000000)!=0;}
    else{s1=(dval&0x8000000000000000ULL)!=0;s2=(sval&0x8000000000000000ULL)!=0;sr=(msum&0x8000000000000000ULL)!=0;}
    if((s1==s2)&&(s1!=sr))f|=FLAG_OVERFLOW;else f&=~FLAG_OVERFLOW;
    if(cpu.is_32bit_mode()){if((msum&0x80000000)!=0)f|=FLAG_SIGN;else f&=~FLAG_SIGN;}
    else{if((msum&0x8000000000000000ULL)!=0)f|=FLAG_SIGN;else f&=~FLAG_SIGN;}
    cpu.set_flags(f);
    cpu.set_register_mode_aware(static_cast<Register>(dst),sum);
    cpu.set_register_mode_aware(static_cast<Register>(src),dval);}
    cpu.set_pc(pc+3);}else running=false;cpu.print_state("XADD");}

// === Platform-Specific Opcodes ===
#include <ctime>
void handle_cpuid(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc=cpu.get_pc();
    uint64_t eax=cpu.get_register_mode_aware(static_cast<Register>(0));
    uint32_t leaf=eax&0xFFFFFFFF;
    if(leaf==0){
        cpu.set_register_mode_aware(static_cast<Register>(0),1);  // max leaf=1
        cpu.set_register_mode_aware(static_cast<Register>(3),0x696D6544); // EBX: "Demi"
        cpu.set_register_mode_aware(static_cast<Register>(2),0x69676E45); // EDX: "Engi"
        cpu.set_register_mode_aware(static_cast<Register>(1),0x4D56656E); // ECX: "neVM"
    }else if(leaf==1){
        cpu.set_register_mode_aware(static_cast<Register>(0),0x00010000); // version 1.0
        cpu.set_register_mode_aware(static_cast<Register>(3),0x00000800); // feature flags
        cpu.set_register_mode_aware(static_cast<Register>(2),0x00000000);
        cpu.set_register_mode_aware(static_cast<Register>(1),0x00000000);
    }
    cpu.set_pc(pc+1); cpu.print_state("CPUID");
}
void handle_rdtsc(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc=cpu.get_pc();
    static uint64_t counter=0; counter+=100;
    uint32_t edx_val=static_cast<uint32_t>(counter>>32);
    uint32_t eax_val=static_cast<uint32_t>(counter);
    cpu.set_register_mode_aware(static_cast<Register>(2),edx_val);
    cpu.set_register_mode_aware(static_cast<Register>(0),eax_val);
    cpu.set_pc(pc+1); cpu.print_state("RDTSC");
}
void handle_syscall(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc=cpu.get_pc();
    cpu.trigger_interrupt(0x80);
    cpu.set_pc(pc+1); cpu.print_state("SYSCALL");
}
void handle_sysenter(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc=cpu.get_pc();
    cpu.trigger_interrupt(0x80);
    cpu.set_pc(pc+1); cpu.print_state("SYSENTER");
}

// === Final Opcodes: ENTER + REP ===
void handle_enter(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc=cpu.get_pc();
    if(pc+3<program.size()){
        uint16_t frame_size=program[pc+1]|(static_cast<uint16_t>(program[pc+2])<<8);
        uint8_t nesting=program[pc+3];
        // Push RBP
        uint64_t rbp=cpu.get_register_mode_aware(static_cast<Register>(5));
        uint32_t sp=cpu.get_sp()-4;
        cpu.set_sp(sp); cpu.get_registers()[4]=sp;
        if(sp+3<cpu.get_memory().size()){
            cpu.get_memory()[sp]=rbp&0xFF;
            cpu.get_memory()[sp+1]=(rbp>>8)&0xFF;
            cpu.get_memory()[sp+2]=(rbp>>16)&0xFF;
            cpu.get_memory()[sp+3]=(rbp>>24)&0xFF;
        }
        // RBP = RSP
        uint64_t rsp=cpu.get_register_mode_aware(static_cast<Register>(4));
        cpu.set_register_mode_aware(static_cast<Register>(5),rsp);
        // RSP -= frame_size
        cpu.set_register_mode_aware(static_cast<Register>(4),rsp-frame_size);
        cpu.set_pc(pc+4);
    }else running=false;
    cpu.print_state("ENTER");
}
void handle_rep(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc=cpu.get_pc();
    if(pc+1<program.size()){
        uint8_t next_op=program[pc+1];
        bool is_string=(next_op>=0x9A&&next_op<=0x9F)||(next_op>=0xB7&&next_op<=0xB9);
        if(!is_string){cpu.set_pc(pc+1);cpu.print_state("REP");return;}
        uint64_t ecx=cpu.get_register_mode_aware(static_cast<Register>(1));
        uint64_t mask=cpu.get_operand_mask();uint64_t count=ecx&mask;
        if(count==0){cpu.set_pc(pc+2);cpu.print_state("REP");return;}
        // Set PC to string op and execute count times
        cpu.set_pc(pc+1);
        for(uint64_t i=0;i<count&&running;i++){
            switch(next_op){
                case 0x9A: handle_movsb(cpu,program,running);break;
                case 0x9B: handle_movsw(cpu,program,running);break;
                case 0x9C: handle_movsd(cpu,program,running);break;
                case 0x9D: handle_stosb(cpu,program,running);break;
                case 0x9E: handle_stosw(cpu,program,running);break;
                case 0x9F: handle_stosd(cpu,program,running);break;
                case 0xB7: handle_lodsb(cpu,program,running);break;
                case 0xB8: handle_lodsw(cpu,program,running);break;
                case 0xB9: handle_lodsd(cpu,program,running);break;
            }
            if(!running)break;
            // Re-point PC at string op for next iteration (handlers advance PC by 1)
            cpu.set_pc(pc+1);
        }
        // Skip past both REP and string op
        if(running)cpu.set_pc(pc+2);
    }else running=false;
    cpu.print_state("REP");
}
