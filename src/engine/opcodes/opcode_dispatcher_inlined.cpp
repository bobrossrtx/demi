#include "opcode_dispatcher_inlined.hpp"
#include "../cpu.hpp"
#include "../cpu_flags.hpp"
#include "../branch_prediction.hpp"
#include "../opcodes/instruction_fusion.hpp"
#include "../opcodes/opcode_profiler.hpp"
#include "../../config.hpp"

// Macro to dispatch next instruction with fusion check.
// The computed goto dispatcher runs in a loop; we need to check for fusion
// at every dispatch point so the fusion engine gets a chance to match
// multi-instruction patterns (INC+CMP, MOV+ADD, PUSH+POP, etc.).
#define DISPATCH_WITH_FUSION() \
    do { \
        if (!running || cpu.get_pc() >= program.size()) { \
            return; \
        } \
        if (InstructionFusion::try_instruction_fusion(cpu, program, running)) { \
            if (!running || cpu.get_pc() >= program.size()) { \
                return; \
            } \
            OPCODE_PROFILE(program[cpu.get_pc()]); \
            goto *dispatch_table[program[cpu.get_pc()]]; \
        } \
        OPCODE_PROFILE(program[cpu.get_pc()]); \
        goto *dispatch_table[program[cpu.get_pc()]]; \
    } while(0)
#include "../../debug/debug_handler.hpp"
#include <fmt/format.h>
#include <iomanip>
#include <iostream>
#include <atomic>

// Suppress pedantic warnings about computed gotos - they are intentional for performance
// Suppress pedantic warnings for performance-critical computed gotos
// This is a widely-used GCC extension for threaded code interpretation
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

// External handler declarations
extern void handle_sub(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_call(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_ret(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_jmp(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_load(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_store(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_push(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_pop(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cmp(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_jz(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_jnz(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_js(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_jns(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_jc(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_jnc(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_jo(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_jno(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_jg(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_jl(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_jge(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_jle(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_mod(CPU& cpu, const std::vector<uint8_t>& program, bool& running);

// High-priority missing x86-equivalent opcodes
extern void handle_adc(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_sbb(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_imul(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_idiv(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_sal(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_sar(CPU& cpu, const std::vector<uint8_t>& program, bool& running);

// Medium-priority opcodes
extern void handle_clc(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_stc(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cmc(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cld(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_std(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_lahf(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_sahf(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cbw(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cwde(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cwd(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cdq(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_rol(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_ror(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_loop(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_loope(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_loopne(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_rcl(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_rcr(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_setz(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_setnz(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_setc(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_setnc(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_seto(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_setno(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_sets(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_setns(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_setg(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_setge(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_setl(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_setle(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_xchg(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_bswap(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_movsx(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_movzx(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cmovo(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cmovno(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cmovc(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cmovnc(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cmovz(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cmovnz(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cmovs(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cmovns(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cmovg(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cmovge(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cmovl(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cmovle(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cmova(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cmovbe(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_movsb(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_movsw(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_movsd(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_stosb(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_stosw(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_stosd(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_lodsb(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_lodsw(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_lodsd(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_bt(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_bts(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_btr(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_btc(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cmpxchg(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_xadd(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cpuid(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_rdtsc(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_syscall(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_sysenter(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_enter(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_rep(CPU& cpu, const std::vector<uint8_t>& program, bool& running);

// 64-bit operation handlers
extern void handle_add64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_sub64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_mov64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_mul64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_div64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_and64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_or64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_xor64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_mod64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cmp64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_inc64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_not64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_shl64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_shr64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_dec64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_load_imm64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_loadex(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_storex(CPU& cpu, const std::vector<uint8_t>& program, bool& running);

extern void dispatch_opcode(CPU& cpu, const std::vector<uint8_t>& program, bool& running);

// Check for computed goto support at compile time
#if USE_THREADED_CODE

// High-performance dispatcher with aggressive inlining
// This implements the most common operations directly in the dispatcher
// to eliminate function call overhead
void dispatch_opcode_inlined(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    DEBUG_TRACE(Logging::DebugCategory::CPU_DISPATCHER, "[DISPATCH_INLINED] ENTRY: PC=0x{:04X}", cpu.get_pc());
    
    // Declare dispatch table at function scope
    static void* dispatch_table[256] = {nullptr};
    
    DEBUG_TRACE(Logging::DebugCategory::CPU_DISPATCHER, "[DISPATCH_INLINED] About to goto init_dispatch_table", "");
    // Entry point - jump to dispatch table initialization
    goto init_dispatch_table;

    // Inlined NOP operation - most common, simplest operation
    op_nop: {
        uint32_t pc = cpu.get_pc();
        
        #ifndef NDEBUG
        if (Config::trace) {
            DEBUG_TRACE(Logging::DebugCategory::CPU_EXECUTION, "[PC=0x{:04X}] [NOP] PC={}", pc, pc);
            cpu.print_state("NOP");
        }
        #endif
        
        cpu.set_pc(pc + 1);
        DISPATCH_WITH_FUSION();
    }

    // Inlined LOAD_IMM operation - very common for loading 32-bit constants into registers
    op_load_imm: {
        uint32_t pc = cpu.get_pc();

        if (__builtin_expect(pc + 5 >= program.size(), 0)) {
            #ifdef VM_DEBUG_BOUNDS
            Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION,
                fmt::format("[LOAD_IMM] Out of bounds at PC={}", pc), Logging::DebugLevel::CRITICAL);
            #endif
            running = false;
            return;
        }

        uint8_t reg = program[pc + 1];

        #ifndef NDEBUG
        if (__builtin_expect(reg >= TOTAL_REGISTERS, 0)) {
            #ifdef VM_DEBUG_BOUNDS
            DEBUG_CRITICAL(Logging::DebugCategory::MEM_BOUNDS, "[LOAD_IMM] Invalid register R{}", reg);
            #endif
            running = false;
            return;
        }
        #endif

        // Read 32-bit immediate in little-endian format
        uint32_t value = 0;
        value |= static_cast<uint32_t>(program[pc + 2]);
        value |= static_cast<uint32_t>(program[pc + 3]) << 8;
        value |= static_cast<uint32_t>(program[pc + 4]) << 16;
        value |= static_cast<uint32_t>(program[pc + 5]) << 24;

        #ifndef NDEBUG
        if (Config::trace) {
            DEBUG_TRACE(Logging::DebugCategory::CPU_EXECUTION, "[PC=0x{:04X}] [LOAD_IMM] R{} = {}", pc, reg, value);
        }
        #endif

        cpu.set_register_mode_aware(static_cast<Register>(reg), value);
        cpu.set_pc(pc + 6); // opcode(1) + reg(1) + imm32(4)

        #ifndef NDEBUG
        if (Config::trace) {
            cpu.print_state("LOAD_IMM");
        }
        #endif

        DISPATCH_WITH_FUSION();
    }

    // Inlined ADD operation - very common arithmetic operation
    op_add: {
        uint32_t pc = cpu.get_pc();
        
        #ifndef NDEBUG
        if (__builtin_expect(pc + 2 >= program.size(), 0)) {
            #ifdef VM_DEBUG_BOUNDS
            Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION,
                fmt::format("[ADD] Out of bounds at PC={}", pc), Logging::DebugLevel::CRITICAL);
            #endif
            running = false;
            return;
        }
        #endif
        
        uint8_t reg1 = program[pc + 1];
        uint8_t reg2 = program[pc + 2];
        
        #ifndef NDEBUG
        if (__builtin_expect(reg1 >= cpu.get_registers_64().size() || reg2 >= cpu.get_registers_64().size(), 0)) {
            #ifdef VM_DEBUG_BOUNDS
            DEBUG_CRITICAL(Logging::DebugCategory::MEM_BOUNDS, "[ADD] Invalid register R{} or R{}", reg1, reg2);
            #endif
            running = false;
            return;
        }
        
        if (Config::trace) {
            DEBUG_TRACE(Logging::DebugCategory::CPU_EXECUTION, "[PC=0x{:04X}] [ADD] R{} += R{}", pc, reg1, reg2);
        }
        #endif
        
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
        
        cpu.set_pc(pc + 3);
        
        #ifndef NDEBUG
        if (Config::trace) {
            cpu.print_state("ADD");
        }
        #endif
        
        DISPATCH_WITH_FUSION();
    }

    // Inlined MOV operation - very common register move
    op_mov: {
        uint32_t pc = cpu.get_pc();
        
        #ifndef NDEBUG
        if (__builtin_expect(pc + 2 >= program.size(), 0)) {
            #ifdef VM_DEBUG_BOUNDS
            Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION,
                fmt::format("[MOV] Out of bounds at PC={}", pc), Logging::DebugLevel::CRITICAL);
            #endif
            running = false;
            return;
        }
        #endif
        
        uint8_t reg_dst = program[pc + 1];
        uint8_t reg_src = program[pc + 2];
        
        #ifndef NDEBUG
        if (__builtin_expect(reg_dst >= TOTAL_REGISTERS || reg_src >= TOTAL_REGISTERS, 0)) {
            #ifdef VM_DEBUG_BOUNDS
            DEBUG_CRITICAL(Logging::DebugCategory::MEM_BOUNDS, "[MOV] Invalid register R{} or R{}", reg_dst, reg_src);
            #endif
            running = false;
            return;
        }
        
        if (Config::trace) {
            DEBUG_TRACE(Logging::DebugCategory::CPU_EXECUTION, "[PC=0x{:04X}] [MOV] R{} = R{}", pc, reg_dst, reg_src);
        }
        #endif
        
        // Use proper register accessors for mode-aware operation
        uint64_t value = cpu.get_register_mode_aware(static_cast<Register>(reg_src));
        cpu.set_register_mode_aware(static_cast<Register>(reg_dst), value);
        cpu.set_pc(pc + 3);
        
        #ifndef NDEBUG
        if (Config::trace) {
            cpu.print_state("MOV");
        }
        #endif
        
        DISPATCH_WITH_FUSION();
    }

    // Inlined HALT operation - must stop execution
    op_halt: {
        #ifndef NDEBUG
        if (Config::trace) {
            DEBUG_TRACE(Logging::DebugCategory::CPU_EXECUTION, "[PC=0x{:04X}] [HALT]", cpu.get_pc());
            cpu.print_state("HALT");
        }
        #endif
        
        running = false;
        return;
    }

    // Inlined LOAD_IMM64 operation - common for 64-bit constants
    op_load_imm64: {
        uint32_t pc = cpu.get_pc();
        
        #ifndef NDEBUG
        if (__builtin_expect(pc + 9 >= program.size(), 0)) {
            #ifdef VM_DEBUG_BOUNDS
            Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION,
                fmt::format("[LOAD_IMM64] Out of bounds at PC={}", pc), Logging::DebugLevel::CRITICAL);
            #endif
            running = false;
            return;
        }
        #endif
        
        uint8_t reg = program[pc + 1];
        
        #ifndef NDEBUG
        if (__builtin_expect(reg >= TOTAL_REGISTERS, 0)) {
            #ifdef VM_DEBUG_BOUNDS
            DEBUG_CRITICAL(Logging::DebugCategory::MEM_BOUNDS, "[LOAD_IMM64] Invalid register R{}", reg);
            #endif
            running = false;
            return;
        }
        #endif
        
        // Read 64-bit value (8 bytes) in little-endian format
        uint64_t value = 0;
        value |= static_cast<uint64_t>(program[pc + 2]);
        value |= static_cast<uint64_t>(program[pc + 3]) << 8;
        value |= static_cast<uint64_t>(program[pc + 4]) << 16;
        value |= static_cast<uint64_t>(program[pc + 5]) << 24;
        value |= static_cast<uint64_t>(program[pc + 6]) << 32;
        value |= static_cast<uint64_t>(program[pc + 7]) << 40;
        value |= static_cast<uint64_t>(program[pc + 8]) << 48;
        value |= static_cast<uint64_t>(program[pc + 9]) << 56;
        
        #ifndef NDEBUG
        if (Config::trace) {
            DEBUG_TRACE(Logging::DebugCategory::CPU_EXECUTION, "[PC=0x{:04X}] [LOAD_IMM64] R{} = {}", pc, reg, static_cast<int64_t>(value));
        }
        #endif
        
        // Store as int64_t (signed 64-bit) to the 64-bit register array
        cpu.set_register_64(static_cast<Register>(reg), static_cast<int64_t>(value));
        cpu.set_pc(pc + 10); // opcode + register + 8 bytes
        
        DISPATCH_WITH_FUSION();
    }

    // Inlined DEBUG operation
    op_debug: {
        uint32_t pc = cpu.get_pc();
        
        #ifndef NDEBUG
        if (__builtin_expect(pc + 2 >= program.size(), 0)) {
            running = false;
            return;
        }
        #endif
        
        uint8_t sub_opcode = program[pc + 1];
        uint32_t next_pc = pc + 2;
        
        if (sub_opcode == 0) { // PRINT
            if (Config::debug) {
                uint8_t type = program[pc + 2];
                if (type == 1) { // String
                    uint8_t len = program[pc + 3];
                    std::string s;
                    for (int i = 0; i < len; ++i) s += (char)program[pc + 4 + i];
                    std::cout << s << std::endl;
                    next_pc = pc + 4 + len;
                } else if (type == 0) { // Register
                    uint8_t reg = program[pc + 3];
                    std::cout << cpu.get_registers()[reg] << std::endl;
                    next_pc = pc + 4;
                }
            } else {
                uint8_t type = program[pc + 2];
                if (type == 1) { // String
                    uint8_t len = program[pc + 3];
                    next_pc = pc + 4 + len;
                } else {
                    next_pc = pc + 4;
                }
            }
        } else if (sub_opcode == 1) { // BREAK
            if (Config::debug) {
                std::cout << "BREAKPOINT at 0x" << std::hex << pc << std::dec << std::endl;
            }
        } else if (sub_opcode == 2) { // DUMP
            if (Config::debug) {
                cpu.print_state("DUMP");
            }
        } else if (sub_opcode == 3) { // MEMDUMP
            // Start (4 bytes), Length (4 bytes)
            uint32_t start = 0;
            start |= (uint32_t)program[pc + 2];
            start |= (uint32_t)program[pc + 3] << 8;
            start |= (uint32_t)program[pc + 4] << 16;
            start |= (uint32_t)program[pc + 5] << 24;
            
            uint32_t len = 0;
            len |= (uint32_t)program[pc + 6];
            len |= (uint32_t)program[pc + 7] << 8;
            len |= (uint32_t)program[pc + 8] << 16;
            len |= (uint32_t)program[pc + 9] << 24;
            
            if (Config::debug) {
                const auto& mem = cpu.get_memory();
                std::cout << "Memory dump [0x" << std::hex << std::uppercase << start << " - 0x" << (start + len - 1) << "]:" << std::dec << std::endl;
                
                // 16-byte wide hex dump with ASCII
                for (uint32_t i = 0; i < len; i += 16) {
                    // Address
                    std::cout << "  0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << (start + i) << ": ";
                    
                    // Hex bytes
                    for (uint32_t j = 0; j < 16; ++j) {
                        uint32_t addr = start + i + j;
                        if (i + j < len && addr < mem.size()) {
                            std::cout << std::setw(2) << std::setfill('0') << std::hex << (int)mem[addr];
                        } else {
                            std::cout << "  ";
                        }
                        std::cout << " ";
                        if (j == 7) std::cout << " "; // Extra space in middle
                    }
                    
                    // ASCII representation
                    std::cout << " |";
                    for (uint32_t j = 0; j < 16; ++j) {
                        uint32_t addr = start + i + j;
                        if (i + j < len && addr < mem.size()) {
                            unsigned char c = mem[addr];
                            std::cout << (char)((c >= 32 && c <= 126) ? c : '.');
                        } else {
                            std::cout << " ";
                        }
                    }
                    std::cout << "|" << std::dec << std::endl;
                }
            }
            
            next_pc = pc + 10;
        } else if (sub_opcode == 4) { // TRACE
            uint8_t val = program[pc + 2];
            if (val == 2) {
                Config::trace = !Config::trace;
            } else {
                Config::trace = (val != 0);
            }
            next_pc = pc + 3;
        } else if (sub_opcode == 5) { // ASSERT
            uint8_t type = program[pc + 2];
            uint64_t actual = 0;
            if (type == 0) { // Register
                uint8_t reg = program[pc + 3];
                actual = cpu.get_registers()[reg];
                next_pc = pc + 4;
            } else { // Memory
                uint32_t addr = 0;
                addr |= (uint32_t)program[pc + 3];
                addr |= (uint32_t)program[pc + 4] << 8;
                addr |= (uint32_t)program[pc + 5] << 16;
                addr |= (uint32_t)program[pc + 6] << 24;
                const auto& mem = cpu.get_memory();
                if (addr + 7 < mem.size()) {
                    actual = *(uint64_t*)(&mem[addr]);
                }
                next_pc = pc + 7;
            }
            uint64_t expected = 0;
            for (int i = 0; i < 8; ++i) {
                expected |= ((uint64_t)program[next_pc + i]) << (i * 8);
            }
            next_pc += 8;
            
            if (Config::debug) {
                if (actual != expected) {
                    std::cout << "ASSERTION FAILED at 0x" << std::hex << pc << ": expected 0x" << expected 
                              << ", got 0x" << actual << std::dec << std::endl;
                } else {
                    std::cout << "ASSERTION PASSED at 0x" << std::hex << pc << std::dec << std::endl;
                }
            }
        } else if (sub_opcode == 6) { // DUMPSTACK
            uint32_t depth = 0;
            depth |= (uint32_t)program[pc + 2];
            depth |= (uint32_t)program[pc + 3] << 8;
            depth |= (uint32_t)program[pc + 4] << 16;
            depth |= (uint32_t)program[pc + 5] << 24;
            
            if (Config::debug) {
                const auto& regs = cpu.get_registers();
                uint32_t sp = regs[6]; // RSP
                const auto& mem = cpu.get_memory();
                
                std::cout << "Stack dump (depth=" << depth << ", SP=0x" << std::hex << sp << "):" << std::dec << std::endl;
                for (uint32_t i = 0; i < depth && (sp + i * 8) < mem.size(); ++i) {
                    uint64_t val = 0;
                    uint32_t addr = sp + i * 8;
                    if (addr + 7 < mem.size()) {
                        val = *(uint64_t*)(&mem[addr]);
                    }
                    std::cout << "  [SP+" << (i * 8) << "] 0x" << std::hex << addr << ": 0x" << val << std::dec << std::endl;
                }
            }
            next_pc = pc + 6;
        } else if (sub_opcode == 7) { // WATCH
            uint32_t addr = 0;
            addr |= (uint32_t)program[pc + 2];
            addr |= (uint32_t)program[pc + 3] << 8;
            addr |= (uint32_t)program[pc + 4] << 16;
            addr |= (uint32_t)program[pc + 5] << 24;
            
            uint32_t len = 0;
            len |= (uint32_t)program[pc + 6];
            len |= (uint32_t)program[pc + 7] << 8;
            len |= (uint32_t)program[pc + 8] << 16;
            len |= (uint32_t)program[pc + 9] << 24;
            
            if (Config::debug) {
                std::cout << "WATCHPOINT set at 0x" << std::hex << addr << " (length " << std::dec << len << ")" << std::endl;
                // Note: Full implementation would require tracking watchpoints
            }
            next_pc = pc + 10;
        } else if (sub_opcode == 8) { // UNWATCH
            uint32_t addr = 0;
            addr |= (uint32_t)program[pc + 2];
            addr |= (uint32_t)program[pc + 3] << 8;
            addr |= (uint32_t)program[pc + 4] << 16;
            addr |= (uint32_t)program[pc + 5] << 24;
            
            if (Config::debug) {
                std::cout << "WATCHPOINT removed at 0x" << std::hex << addr << std::dec << std::endl;
            }
            next_pc = pc + 6;
        } else if (sub_opcode == 9) { // CHECKPOINT
            uint8_t len = program[pc + 2];
            std::string label;
            for (int i = 0; i < len; ++i) label += (char)program[pc + 3 + i];
            
            if (Config::debug) {
                std::cout << "CHECKPOINT: " << label << " at 0x" << std::hex << pc << std::dec << std::endl;
            }
            next_pc = pc + 3 + len;
        } else if (sub_opcode == 10) { // LOG
            uint8_t level = program[pc + 2];
            uint8_t len = program[pc + 3];
            std::string message;
            for (int i = 0; i < len; ++i) message += (char)program[pc + 4 + i];
            
            if (Config::debug) {
                const char* level_names[] = {"DEBUG", "INFO", "WARN", "ERROR"};
                const char* level_name = (level < 4) ? level_names[level] : "UNKNOWN";
                std::cout << "[" << level_name << "] " << message << std::endl;
            }
            next_pc = pc + 4 + len;
        } else if (sub_opcode == 11) { // DUMPREG
            uint8_t reg = program[pc + 2];
            
            if (Config::debug) {
                const auto& regs = cpu.get_registers();
                const char* reg_names[] = {"RAX", "RBX", "RCX", "RDX", "RSI", "RDI", "RSP", "RBP", 
                                          "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15"};
                const char* name = (reg < 16) ? reg_names[reg] : "UNKNOWN";
                std::cout << name << " = 0x" << std::hex << regs[reg] << " (" << std::dec << regs[reg] << ")" << std::endl;
            }
            next_pc = pc + 3;
        } else if (sub_opcode == 12) { // MEMSET
            uint32_t addr = 0;
            addr |= (uint32_t)program[pc + 2];
            addr |= (uint32_t)program[pc + 3] << 8;
            addr |= (uint32_t)program[pc + 4] << 16;
            addr |= (uint32_t)program[pc + 5] << 24;
            
            uint32_t len = 0;
            len |= (uint32_t)program[pc + 6];
            len |= (uint32_t)program[pc + 7] << 8;
            len |= (uint32_t)program[pc + 8] << 16;
            len |= (uint32_t)program[pc + 9] << 24;
            
            uint8_t value = program[pc + 10];
            
            if (Config::debug) {
                auto& mem = cpu.get_memory();
                for (uint32_t i = 0; i < len && (addr + i) < mem.size(); ++i) {
                    mem[addr + i] = value;
                }
                std::cout << "MEMSET: filled " << len << " bytes at 0x" << std::hex << addr 
                          << " with 0x" << (int)value << std::dec << std::endl;
            }
            next_pc = pc + 11;
        } else if (sub_opcode == 13) { // STEP
            uint32_t count = 0;
            count |= (uint32_t)program[pc + 2];
            count |= (uint32_t)program[pc + 3] << 8;
            count |= (uint32_t)program[pc + 4] << 16;
            count |= (uint32_t)program[pc + 5] << 24;
            
            if (Config::debug) {
                std::cout << "STEP: executing " << count << " instruction(s) starting at 0x" 
                          << std::hex << pc << std::dec << std::endl;
                // Note: Full implementation would require step-mode control
            }
            next_pc = pc + 6;
        }
        
        cpu.set_pc(next_pc);
        
        if (__builtin_expect(cpu.get_pc() >= program.size(), 0)) {
            running = false;
            return;
        }
        goto *dispatch_table[program[cpu.get_pc()]];
    }

    // Generic handler for non-inlined operations
    op_handler: {
        uint8_t opcode = program[cpu.get_pc()];
        
        // Switch to appropriate handler for non-inlined opcodes
        switch (opcode) {
            case static_cast<uint8_t>(Opcode::SUB): handle_sub(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::JMP): handle_jmp(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::LOAD): handle_load(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::STORE): handle_store(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::PUSH): handle_push(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::POP): handle_pop(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::CMP): handle_cmp(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::JZ): handle_jz(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::JNZ): handle_jnz(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::JS): handle_js(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::JNS): handle_jns(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::JC): handle_jc(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::JNC): handle_jnc(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::JO): handle_jo(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::JNO): handle_jno(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::JG): handle_jg(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::JL): handle_jl(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::JGE): handle_jge(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::JLE): handle_jle(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::MOD): handle_mod(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::CALL): handle_call(cpu, program, running); break;
            case static_cast<uint8_t>(Opcode::RET):  handle_ret(cpu, program, running);  break;
            
            // High-priority missing x86-equivalent opcodes (0x44-0x49)
            case 0x44: handle_adc(cpu, program, running); break;   // ADC
            case 0x45: handle_sbb(cpu, program, running); break;   // SBB
            case 0x46: handle_imul(cpu, program, running); break;  // IMUL
            case 0x47: handle_idiv(cpu, program, running); break;  // IDIV
            case 0x48: handle_sal(cpu, program, running); break;   // SAL
            case 0x49: handle_sar(cpu, program, running); break;   // SAR
            case 0x4A: handle_clc(cpu, program, running); break;    // CLC
            case 0x4B: handle_stc(cpu, program, running); break;    // STC
            case 0x4C: handle_cmc(cpu, program, running); break;    // CMC
            case 0x4D: handle_cld(cpu, program, running); break;    // CLD
            case 0x4E: handle_std(cpu, program, running); break;    // STD
            case 0x4F: handle_lahf(cpu, program, running); break;   // LAHF
            case 0x74: handle_sahf(cpu, program, running); break;   // SAHF
            case 0x75: handle_cbw(cpu, program, running); break;    // CBW
            case 0x76: handle_cwde(cpu, program, running); break;   // CWDE
            case 0x77: handle_cwd(cpu, program, running); break;    // CWD
            case 0x78: handle_cdq(cpu, program, running); break;    // CDQ
            case 0x79: handle_rol(cpu, program, running); break;    // ROL
            case 0x7A: handle_ror(cpu, program, running); break;    // ROR
            case 0x7B: handle_loop(cpu, program, running); break;   // LOOP
            case 0x7C: handle_loope(cpu, program, running); break;  // LOOPE
            case 0x7D: handle_loopne(cpu, program, running); break; // LOOPNE
            case 0x7E: handle_rcl(cpu, program, running); break;    // RCL
            case 0x7F: handle_rcr(cpu, program, running); break;    // RCR
            case 0x3A: handle_cmovo(cpu, program, running); break;  // CMOVO
            case 0x3B: handle_cmovno(cpu, program, running); break; // CMOVNO
            case 0x3C: handle_cmovc(cpu, program, running); break;  // CMOVC
            case 0x3D: handle_cmovnc(cpu, program, running); break; // CMOVNC
            case 0x3E: handle_cmovz(cpu, program, running); break;  // CMOVZ
            case 0x3F: handle_cmovnz(cpu, program, running); break; // CMOVNZ
            case 0x6A: handle_cmovs(cpu, program, running); break;  // CMOVS
            case 0x6B: handle_cmovns(cpu, program, running); break; // CMOVNS
            case 0x6C: handle_cmovg(cpu, program, running); break;  // CMOVG
            case 0x6D: handle_cmovge(cpu, program, running); break; // CMOVGE
            case 0x6E: handle_cmovl(cpu, program, running); break;  // CMOVL
            case 0x6F: handle_cmovle(cpu, program, running); break; // CMOVLE
            case 0xEB: handle_setz(cpu, program, running); break;   // SETZ
            case 0xEC: handle_setnz(cpu, program, running); break;  // SETNZ
            case 0xED: handle_setc(cpu, program, running); break;   // SETC
            case 0xEE: handle_setnc(cpu, program, running); break;  // SETNC
            case 0xEF: handle_seto(cpu, program, running); break;   // SETO
            case 0xF0: handle_setno(cpu, program, running); break;  // SETNO
            case 0xF1: handle_sets(cpu, program, running); break;   // SETS
            case 0xF2: handle_setns(cpu, program, running); break;  // SETNS
            case 0xF3: handle_setg(cpu, program, running); break;   // SETG
            case 0xF4: handle_setge(cpu, program, running); break;  // SETGE
            case 0xF5: handle_setl(cpu, program, running); break;   // SETL
            case 0xF6: handle_setle(cpu, program, running); break;  // SETLE
            case 0xF7: handle_xchg(cpu, program, running); break;  // XCHG
            case 0xF8: handle_bswap(cpu, program, running); break; // BSWAP
            case 0xF9: handle_movsx(cpu, program, running); break; // MOVSX
            case 0xFC: handle_movzx(cpu, program, running); break; // MOVZX
            case 0xFD: handle_cmova(cpu, program, running); break;  // CMOVA
            case 0xFE: handle_cmovbe(cpu, program, running); break; // CMOVBE
            case 0x9A: handle_movsb(cpu, program, running); break; // MOVSB
            case 0x9B: handle_movsw(cpu, program, running); break; // MOVSW
            case 0x9C: handle_movsd(cpu, program, running); break; // MOVSD
            case 0x9D: handle_stosb(cpu, program, running); break; // STOSB
            case 0x9E: handle_stosw(cpu, program, running); break; // STOSW
            case 0x9F: handle_stosd(cpu, program, running); break; // STOSD
            case 0xB7: handle_lodsb(cpu, program, running); break; // LODSB
            case 0xB8: handle_lodsw(cpu, program, running); break; // LODSW
            case 0xB9: handle_lodsd(cpu, program, running); break; // LODSD
            case 0xBA: handle_bt(cpu, program, running); break;      // BT
            case 0xBB: handle_bts(cpu, program, running); break;     // BTS
            case 0xBC: handle_btr(cpu, program, running); break;     // BTR
            case 0xBD: handle_btc(cpu, program, running); break;     // BTC
            case 0xBE: handle_cmpxchg(cpu, program, running); break; // CMPXCHG
            case 0xBF: handle_xadd(cpu, program, running); break;    // XADD
            
            // 64-bit operations (0x50-0x5F range)
            case 0x50: handle_add64(cpu, program, running); break;  // ADD64
            case 0x51: handle_sub64(cpu, program, running); break;  // SUB64
            case 0x52: handle_mov64(cpu, program, running); break;  // MOV64
            case 0x54: handle_mul64(cpu, program, running); break;  // MUL64
            case 0x55: handle_div64(cpu, program, running); break;  // DIV64
            case 0x56: handle_and64(cpu, program, running); break;  // AND64
            case 0x57: handle_or64(cpu, program, running); break;   // OR64
            case 0x58: handle_xor64(cpu, program, running); break;  // XOR64
            case 0x59: handle_not64(cpu, program, running); break;  // NOT64
            case 0x5A: handle_shl64(cpu, program, running); break;  // SHL64
            case 0x5B: handle_shr64(cpu, program, running); break;  // SHR64
            case 0x5C: handle_cmp64(cpu, program, running); break;  // CMP64
            case 0x5D: handle_inc64(cpu, program, running); break;  // INC64
            case 0x5E: handle_dec64(cpu, program, running); break;  // DEC64
            case 0x5F: handle_mod64(cpu, program, running); break;  // MOD64
            
            // Extended operations (0x60-0x6F range)  
            case 0x66: handle_loadex(cpu, program, running); break; // LOADEX
            case 0x67: handle_storex(cpu, program, running); break; // STOREX
            
            default:
                DEBUG_TRACE(Logging::DebugCategory::CPU_DISPATCHER, "[OP_HANDLER] Falling back to consolidated dispatcher for opcode 0x{:02X}", opcode);
                {
                    // Delegate to the consolidated dispatcher for any opcode not handled
                    // above. If it returns without advancing PC and with running still
                    // true, the opcode was not recognised by anyone in the chain — treat
                    // it as invalid to prevent an infinite loop.
                    uint32_t pre_pc = cpu.get_pc();
                    dispatch_opcode(cpu, program, running);
                    DEBUG_TRACE(Logging::DebugCategory::CPU_DISPATCHER, "[OP_HANDLER] Returned from consolidated dispatcher", "");
                    if (__builtin_expect(running && cpu.get_pc() == pre_pc, 0)) {
                        #ifndef NDEBUG
                        Logging::DebugHandler::instance().report(
                            Logging::DebugCategory::CPU_DISPATCHER,
                            fmt::format("[OP_HANDLER] Opcode 0x{:02X} at PC={} made no "
                                        "progress after consolidated dispatch; treating as invalid",
                                        opcode, pre_pc),
                            Logging::DebugLevel::CRITICAL);
                        #endif
                        running = false;
                    }
                }
                return;
        }
        
        DEBUG_TRACE(Logging::DebugCategory::CPU_DISPATCHER, "[OP_HANDLER] Handler completed, continuing to next instruction", "");
        DISPATCH_WITH_FUSION();
    }

    // Invalid opcode handler
    op_invalid: {
        uint8_t opcode = program[cpu.get_pc()];
        DEBUG_CRITICAL(Logging::DebugCategory::CPU_EXECUTION, "Invalid opcode: 0x{:02X} at PC={}", opcode, cpu.get_pc());
        
        // Trigger invalid opcode exception (vector 0x06) via interrupt controller
        // If no handler is installed, handle_pending_interrupts will halt the VM
        cpu.get_interrupt_controller().trigger_exception(
            DemiEngine_Interrupts::CPUException::INVALID_OPCODE);
        cpu.set_pc(cpu.get_pc() + 1);
        
        // If no handler was set up (interrupts disabled or no handler registered), halt
        if (!cpu.get_interrupt_controller().has_pending_interrupts()) {
            running = false;
            std::string opcode_hex = fmt::format("{:02X}", static_cast<int>(opcode));
            throw CPUException("Invalid opcode: 0x" + opcode_hex);
        }
    }

    // Dispatch table initialization (thread-safe: atomic double-checked locking)
    // Note: std::call_once with lambdas cannot capture label addresses (&&label);
    // atomic acquire/release is the correct pattern for computed-goto dispatch tables.
init_dispatch_table:
    DEBUG_TRACE(Logging::DebugCategory::CPU_DISPATCHER, "[DISPATCH_INLINED] Reached init_dispatch_table", "");
    static std::atomic<bool> initialized{false};
    if (!initialized.load(std::memory_order_acquire)) {
        DEBUG_TRACE(Logging::DebugCategory::CPU_DISPATCHER, "[DISPATCH_INLINED] Initializing dispatch table", "");
        // Initialize dispatch table with inlined operations first
        for (int i = 0; i < 256; i++) {
            dispatch_table[i] = &&op_invalid;
        }
        
        // Most common operations get inlined handlers
        dispatch_table[0x00] = &&op_nop;        // NOP - most common in many programs
        dispatch_table[0x01] = &&op_load_imm;   // LOAD_IMM - very common for constants
        dispatch_table[0x02] = &&op_add;        // ADD - very common arithmetic
        dispatch_table[0x04] = &&op_mov;        // MOV - very common register moves
        dispatch_table[0x53] = &&op_load_imm64; // LOAD_IMM64 - common for 64-bit constants
        dispatch_table[0xCD] = &&op_handler;    // INT - software interrupt
        dispatch_table[0xCF] = &&op_handler;    // IRET - interrupt return
        dispatch_table[0xFA] = &&op_handler;    // CLI - clear interrupt flag
        dispatch_table[0xFB] = &&op_handler;    // STI - set interrupt flag
        dispatch_table[0xFF] = &&op_halt;       // HALT - important for termination
        
        // Less common operations use generic handler
        dispatch_table[0x03] = &&op_handler;    // SUB
        dispatch_table[0x05] = &&op_handler;    // JMP  
        dispatch_table[0x06] = &&op_handler;    // LOAD
        dispatch_table[0x07] = &&op_handler;    // STORE
        dispatch_table[0x08] = &&op_handler;    // PUSH
        dispatch_table[0x09] = &&op_handler;    // POP
        dispatch_table[0x0A] = &&op_handler;    // CMP
        dispatch_table[0x0B] = &&op_handler;    // JZ
        dispatch_table[0x0C] = &&op_handler;    // JNZ
        dispatch_table[0x0D] = &&op_handler;    // JS
        dispatch_table[0x0E] = &&op_handler;    // JNS
        dispatch_table[0x0F] = &&op_handler;    // JC
        // Basic arithmetic operations
        dispatch_table[0x10] = &&op_handler;    // MUL
        dispatch_table[0x11] = &&op_handler;    // DIV
        dispatch_table[0x12] = &&op_handler;    // INC
        dispatch_table[0x13] = &&op_handler;    // DEC
        dispatch_table[0x14] = &&op_handler;    // AND
        dispatch_table[0x15] = &&op_handler;    // OR
        dispatch_table[0x16] = &&op_handler;    // XOR
        dispatch_table[0x17] = &&op_handler;    // NOT
        dispatch_table[0x18] = &&op_handler;    // SHL
        dispatch_table[0x19] = &&op_handler;    // SHR
        dispatch_table[0x1A] = &&op_handler;    // CALL
        dispatch_table[0x1B] = &&op_handler;    // RET
        dispatch_table[0x1C] = &&op_handler;    // PUSH_ARG
        dispatch_table[0x1D] = &&op_handler;    // POP_ARG
        dispatch_table[0x1E] = &&op_handler;    // PUSH_FLAG
        dispatch_table[0x1F] = &&op_handler;    // POP_FLAG
        
        // Address and jump operations
        dispatch_table[0x20] = &&op_handler;    // LEA
        dispatch_table[0x21] = &&op_handler;    // SWAP
        dispatch_table[0x22] = &&op_handler;    // JNC
        dispatch_table[0x23] = &&op_handler;    // JO
        dispatch_table[0x24] = &&op_handler;    // JNO
        dispatch_table[0x25] = &&op_handler;    // JG
        dispatch_table[0x26] = &&op_handler;    // JL
        dispatch_table[0x27] = &&op_handler;    // JGE
        dispatch_table[0x28] = &&op_handler;    // JLE
        dispatch_table[0x29] = &&op_handler;    // MOD
        dispatch_table[0x2A] = &&op_handler;    // CPUID
        dispatch_table[0x2B] = &&op_handler;    // RDTSC
        dispatch_table[0x2C] = &&op_handler;    // SYSCALL
        dispatch_table[0x2D] = &&op_handler;    // SYSENTER
        dispatch_table[0x2E] = &&op_handler;    // ENTER
        dispatch_table[0x2F] = &&op_handler;    // REP
        
        // I/O operations
        dispatch_table[0x30] = &&op_handler;    // IN
        dispatch_table[0x31] = &&op_handler;    // OUT
        dispatch_table[0x32] = &&op_handler;    // INB
        dispatch_table[0x33] = &&op_handler;    // OUTB
        dispatch_table[0x34] = &&op_handler;    // INW
        dispatch_table[0x35] = &&op_handler;    // OUTW
        dispatch_table[0x36] = &&op_handler;    // INL
        dispatch_table[0x37] = &&op_handler;    // OUTL
        dispatch_table[0x38] = &&op_handler;    // INSTR
        dispatch_table[0x39] = &&op_handler;    // OUTSTR
        dispatch_table[0x3A] = &&op_handler;    // CMOVO
        dispatch_table[0x3B] = &&op_handler;    // CMOVNO
        dispatch_table[0x3C] = &&op_handler;    // CMOVC
        dispatch_table[0x3D] = &&op_handler;    // CMOVNC
        dispatch_table[0x3E] = &&op_handler;    // CMOVZ
        dispatch_table[0x3F] = &&op_handler;    // CMOVNZ
        
        // Data and memory operations
        dispatch_table[static_cast<uint8_t>(Opcode::DB)] = &&op_handler;    // DB
        dispatch_table[0x41] = &&op_handler;    // LOADR
        dispatch_table[0x42] = &&op_debug;      // DEBUG
        dispatch_table[0x43] = &&op_handler;    // STORER
        dispatch_table[0x44] = &&op_handler;    // ADC
        dispatch_table[0x45] = &&op_handler;    // SBB
        dispatch_table[0x46] = &&op_handler;    // IMUL
        dispatch_table[0x47] = &&op_handler;    // IDIV
        dispatch_table[0x48] = &&op_handler;    // SAL
        dispatch_table[0x49] = &&op_handler;    // SAR
        dispatch_table[0x4A] = &&op_handler;    // CLC
        dispatch_table[0x4B] = &&op_handler;    // STC
        dispatch_table[0x4C] = &&op_handler;    // CMC
        dispatch_table[0x4D] = &&op_handler;    // CLD
        dispatch_table[0x4E] = &&op_handler;    // STD
        dispatch_table[0x4F] = &&op_handler;    // LAHF
        dispatch_table[0x74] = &&op_handler;    // SAHF
        dispatch_table[0x75] = &&op_handler;    // CBW
        dispatch_table[0x76] = &&op_handler;    // CWDE
        dispatch_table[0x77] = &&op_handler;    // CWD
        dispatch_table[0x78] = &&op_handler;    // CDQ
        dispatch_table[0x79] = &&op_handler;    // ROL
        dispatch_table[0x7A] = &&op_handler;    // ROR
        dispatch_table[0x7B] = &&op_handler;    // LOOP
        dispatch_table[0x7C] = &&op_handler;    // LOOPE
        dispatch_table[0x7D] = &&op_handler;    // LOOPNE
        dispatch_table[0x7E] = &&op_handler;    // RCL
        dispatch_table[0x7F] = &&op_handler;    // RCR
        
        // 64-bit operations (0x50-0x5F range)
        dispatch_table[0x50] = &&op_handler;    // ADD64
        dispatch_table[0x51] = &&op_handler;    // SUB64
        dispatch_table[0x52] = &&op_handler;    // MOV64
        dispatch_table[0x54] = &&op_handler;    // MUL64
        dispatch_table[0x55] = &&op_handler;    // DIV64
        dispatch_table[0x56] = &&op_handler;    // AND64
        dispatch_table[0x57] = &&op_handler;    // OR64
        dispatch_table[0x58] = &&op_handler;    // XOR64
        dispatch_table[0x59] = &&op_handler;    // NOT64
        dispatch_table[0x5A] = &&op_handler;    // SHL64
        dispatch_table[0x5B] = &&op_handler;    // SHR64
        dispatch_table[0x5C] = &&op_handler;    // CMP64
        dispatch_table[0x5D] = &&op_handler;    // INC64
        dispatch_table[0x5E] = &&op_handler;    // DEC64
        dispatch_table[0x5F] = &&op_handler;    // MOD64
        
        // Extended operations (0x60-0x6F range)
        dispatch_table[0x60] = &&op_handler;    // MOVEX
        dispatch_table[0x61] = &&op_handler;    // ADDEX
        dispatch_table[0x62] = &&op_handler;    // SUBEX
        dispatch_table[0x63] = &&op_handler;    // MULEX
        dispatch_table[0x64] = &&op_handler;    // DIVEX
        dispatch_table[0x65] = &&op_handler;    // CMPEX
        dispatch_table[0x66] = &&op_handler;    // LOADEX
        dispatch_table[0x67] = &&op_handler;    // STOREX
        dispatch_table[0x68] = &&op_handler;    // PUSHEX
        dispatch_table[0x69] = &&op_handler;    // POPEX
        dispatch_table[0x6A] = &&op_handler;    // CMOVS
        dispatch_table[0x6B] = &&op_handler;    // CMOVNS
        dispatch_table[0x6C] = &&op_handler;    // CMOVG
        dispatch_table[0x6D] = &&op_handler;    // CMOVGE
        dispatch_table[0x6E] = &&op_handler;    // CMOVL
        dispatch_table[0x6F] = &&op_handler;    // CMOVLE
        
        // CPU mode operations
        dispatch_table[0x70] = &&op_handler;    // MODE32
        dispatch_table[0x71] = &&op_handler;    // MODE64
        dispatch_table[0x72] = &&op_handler;    // MODECMP
        dispatch_table[0x73] = &&op_handler;    // MODEFLAG
        
        // SIMD Operations (0x80-0x9F range)
        for (int i = 0x80; i <= 0x99; i++) {
            dispatch_table[i] = &&op_handler;
        dispatch_table[0x9A] = &&op_handler;    // MOVSB
        dispatch_table[0x9B] = &&op_handler;    // MOVSW
        dispatch_table[0x9C] = &&op_handler;    // MOVSD
        dispatch_table[0x9D] = &&op_handler;    // STOSB
        dispatch_table[0x9E] = &&op_handler;    // STOSW
        dispatch_table[0x9F] = &&op_handler;    // STOSD
        }
        
        // FPU Operations (0xA0-0xBF range)
        for (int i = 0xA0; i <= 0xB6; i++) {
            dispatch_table[i] = &&op_handler;
        }
        dispatch_table[0xB7] = &&op_handler;    // LODSB
        dispatch_table[0xB8] = &&op_handler;    // LODSW
        dispatch_table[0xB9] = &&op_handler;    // LODSD
        dispatch_table[0xBA] = &&op_handler;    // BT
        dispatch_table[0xBB] = &&op_handler;    // BTS
        dispatch_table[0xBC] = &&op_handler;    // BTR
        dispatch_table[0xBD] = &&op_handler;    // BTC
        dispatch_table[0xBE] = &&op_handler;    // CMPXCHG
        dispatch_table[0xBF] = &&op_handler;    // XADD
        
        // AVX Operations (0xC0-0xDF range)
        for (int i = 0xC0; i <= 0xDF; i++) {
            dispatch_table[i] = &&op_handler;
        }
        
        // MMX Operations (0xE0-0xEF range)
        for (int i = 0xE0; i <= 0xEA; i++) {
            dispatch_table[i] = &&op_handler;
        }
        
        // SETcc Conditional Set Byte (0xEB-0xF6)
        dispatch_table[0xEB] = &&op_handler;    // SETZ
        dispatch_table[0xEC] = &&op_handler;    // SETNZ
        dispatch_table[0xED] = &&op_handler;    // SETC
        dispatch_table[0xEE] = &&op_handler;    // SETNC
        dispatch_table[0xEF] = &&op_handler;    // SETO
        dispatch_table[0xF0] = &&op_handler;    // SETNO
        dispatch_table[0xF1] = &&op_handler;    // SETS
        dispatch_table[0xF2] = &&op_handler;    // SETNS
        dispatch_table[0xF3] = &&op_handler;    // SETG
        dispatch_table[0xF4] = &&op_handler;    // SETGE
        dispatch_table[0xF5] = &&op_handler;    // SETL
        dispatch_table[0xF6] = &&op_handler;    // SETLE
        dispatch_table[0xF7] = &&op_handler;    // XCHG
        dispatch_table[0xF8] = &&op_handler;    // BSWAP
        dispatch_table[0xF9] = &&op_handler;    // MOVSX
        dispatch_table[0xFC] = &&op_handler;    // MOVZX
        dispatch_table[0xFD] = &&op_handler;    // CMOVA
        dispatch_table[0xFE] = &&op_handler;    // CMOVBE
        
        initialized.store(true, std::memory_order_release);
        DEBUG_TRACE(Logging::DebugCategory::CPU_DISPATCHER, "Inlined threaded dispatcher initialized", "");
    } // end atomic init guard

    DEBUG_TRACE(Logging::DebugCategory::CPU_DISPATCHER, "[DISPATCH_INLINED] About to goto dispatch_start", "");
    // Jump to main dispatch loop
    goto dispatch_start;

dispatch_start:
    DEBUG_TRACE(Logging::DebugCategory::CPU_DISPATCHER, "[DISPATCH_INLINED] Reached dispatch_start, PC=0x{:04X}", cpu.get_pc());
    // Main interpreter loop
    if (__builtin_expect(cpu.get_pc() >= program.size(), 0)) {
        DEBUG_TRACE(Logging::DebugCategory::CPU_DISPATCHER, "[DISPATCH_INLINED] PC out of bounds, stopping", "");
        running = false;
        return;
    }

    // opcode is uint8_t so the index is always in [0, 255] — the table dimension.
    // Guard against a null entry in case the table was only partially initialised
    // (e.g. a TOCTOU race on the atomic flag, or a future table-init change).
    // A null pointer here would cause an immediate segfault on any corrupt or
    // malicious bytecode; the check costs one comparison on the hot path and is
    // hinted cold with __builtin_expect so it does not disturb branch prediction.
    {
        uint8_t opcode = program[cpu.get_pc()];
        // Profile opcode execution (TASK-005)
        OPCODE_PROFILE(opcode);
        void* target = dispatch_table[opcode];
        if (__builtin_expect(target == nullptr, 0)) {
            Logging::DebugHandler::instance().report(
                Logging::DebugCategory::CPU_DISPATCHER,
                fmt::format("Dispatch table null entry for opcode 0x{:02X} at PC={}",
                            opcode, cpu.get_pc()),
                Logging::DebugLevel::CRITICAL);
            running = false;
            return;
        }
        goto *target;
    }
}

#else
// Fallback for compilers without computed goto support
void dispatch_opcode_inlined(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    // Use optimized switch-case fallback with inlined operations
    dispatch_opcode_inlined_fallback(cpu, program, running);
}
#endif

// Optimized fallback implementation with inlined operations
void dispatch_opcode_inlined_fallback(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    while (running && cpu.get_pc() < program.size()) {
        // Try instruction fusion before dispatching each opcode
        if (InstructionFusion::try_instruction_fusion(cpu, program, running)) {
            continue; // Fusion handled this instruction
        }
        
        uint8_t opcode = program[cpu.get_pc()];
        uint32_t pc = cpu.get_pc();
        
        // Profile opcode execution (TASK-005)
        OPCODE_PROFILE(opcode);
        
        switch (opcode) {
            // Inlined NOP
            case 0x00: {
                #ifndef NDEBUG
                if (Config::trace) {
                    Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION,
                        fmt::format("[PC=0x{:04X}] [NOP] PC={}", pc, pc), Logging::DebugLevel::DETAIL);
                    cpu.print_state("NOP");
                }
                #endif
                cpu.set_pc(pc + 1);
                break;
            }
            
            // Inlined LOAD_IMM  
            case 0x01: {
                #ifndef NDEBUG
                if (__builtin_expect(pc + 5 >= program.size(), 0)) {
                    running = false;
                    break;
                }
                #endif
                
                uint8_t reg = program[pc + 1];
                // Read 32-bit value (4 bytes) in little-endian format
                uint32_t value = 0;
                value |= static_cast<uint32_t>(program[pc + 2]);
                value |= static_cast<uint32_t>(program[pc + 3]) << 8;
                value |= static_cast<uint32_t>(program[pc + 4]) << 16;
                value |= static_cast<uint32_t>(program[pc + 5]) << 24;
                
                #ifndef NDEBUG
                if (__builtin_expect(reg >= TOTAL_REGISTERS, 0)) {
                    running = false;
                    break;
                }
                
                if (Config::trace) {
                    Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION,
                        fmt::format("[PC=0x{:04X}] [LOAD_IMM] R{} = {}", pc, reg, value), Logging::DebugLevel::DETAIL);
                }
                #endif
                
                cpu.get_registers()[reg] = value;
                // Also update the 64-bit register array to maintain consistency
                if (reg < cpu.get_registers_64().size()) {
                    cpu.get_registers_64()[reg] = static_cast<uint64_t>(value);
                }
                cpu.set_pc(pc + 6);
                
                #ifndef NDEBUG
                if (Config::trace) {
                    cpu.print_state("LOAD_IMM");
                }
                #endif
                break;
            }
            
            // Inlined ADD
            case 0x02: {
                #ifndef NDEBUG
                if (__builtin_expect(pc + 2 >= program.size(), 0)) {
                    running = false;
                    break;
                }
                #endif
                
                uint8_t reg1 = program[pc + 1];
                uint8_t reg2 = program[pc + 2];
                
                #ifndef NDEBUG
                if (__builtin_expect(reg1 >= TOTAL_REGISTERS || reg2 >= TOTAL_REGISTERS, 0)) {
                    running = false;
                    break;
                }
                
                if (Config::trace) {
                    Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION,
                        fmt::format("[PC=0x{:04X}] [ADD] R{} += R{}", pc, reg1, reg2), Logging::DebugLevel::DETAIL);
                }
                #endif
                
                uint32_t old_val = cpu.get_registers()[reg1];
                uint32_t new_val = old_val + cpu.get_registers()[reg2];
                cpu.get_registers()[reg1] = new_val;
                
                // Update flags
                uint32_t flags = cpu.get_flags();
                flags = (new_val == 0) ? (flags | FLAG_ZERO) : (flags & ~FLAG_ZERO);
                flags = (new_val < old_val) ? (flags | FLAG_CARRY) : (flags & ~FLAG_CARRY);
                flags = (((old_val ^ new_val) & (cpu.get_registers()[reg2] ^ new_val) & 0x80000000) != 0) ? (flags | FLAG_OVERFLOW) : (flags & ~FLAG_OVERFLOW);
                cpu.set_flags(flags);
                
                cpu.set_pc(pc + 3);
                
                #ifndef NDEBUG
                if (Config::trace) {
                    cpu.print_state("ADD");
                }
                #endif
                break;
            }
            
            // Inlined MOV
            case 0x04: {
                #ifndef NDEBUG
                if (__builtin_expect(pc + 2 >= program.size(), 0)) {
                    running = false;
                    break;
                }
                #endif
                
                uint8_t reg_dst = program[pc + 1];
                uint8_t reg_src = program[pc + 2];
                
                #ifndef NDEBUG
                if (__builtin_expect(reg_dst >= TOTAL_REGISTERS || reg_src >= TOTAL_REGISTERS, 0)) {
                    running = false;
                    break;
                }
                
                if (Config::trace) {
                    Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION,
                        fmt::format("[PC=0x{:04X}] [MOV] R{} = R{}", pc, reg_dst, reg_src), Logging::DebugLevel::DETAIL);
                }
                #endif
                
                // Copy from both register arrays for consistency
                uint32_t value = cpu.get_registers()[reg_src];
                cpu.get_registers()[reg_dst] = value;
                cpu.get_registers_64()[reg_dst] = value; // Also update 64-bit array
                cpu.set_pc(pc + 3);
                
                #ifndef NDEBUG
                if (Config::trace) {
                    cpu.print_state("MOV");
                }
                #endif
                break;
            }
            
            // Inlined HALT
            case 0xFF: {
                #ifndef NDEBUG
                if (Config::trace) {
                    Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION,
                        fmt::format("[PC=0x{:04X}] [HALT]", pc), Logging::DebugLevel::DETAIL);
                    cpu.print_state("HALT");
                }
                #endif
                running = false;
                break;
            }
            
            // Inlined DEBUG
            case 0x42: {
                #ifndef NDEBUG
                if (__builtin_expect(pc + 2 >= program.size(), 0)) {
                    running = false;
                    break;
                }
                #endif
                
                uint8_t sub_opcode = program[pc + 1];
                uint32_t next_pc = pc + 2;
                
                if (sub_opcode == 0) { // PRINT
                    uint8_t type = program[pc + 2];
                    if (type == 1) { // String
                        uint8_t len = program[pc + 3];
                        std::string s;
                        for (int i = 0; i < len; ++i) s += (char)program[pc + 4 + i];
                        std::cout << s << std::endl;
                        next_pc = pc + 4 + len;
                    } else if (type == 0) { // Register
                        uint8_t reg = program[pc + 3];
                        std::cout << cpu.get_registers()[reg] << std::endl;
                        next_pc = pc + 4;
                    }
                } else if (sub_opcode == 1) { // BREAK
                    std::cout << "BREAKPOINT at 0x" << std::hex << pc << std::dec << std::endl;
                } else if (sub_opcode == 2) { // DUMP
                    cpu.print_state("DUMP");
                } else if (sub_opcode == 3) { // MEMDUMP
                    uint32_t start = 0;
                    start |= (uint32_t)program[pc + 2];
                    start |= (uint32_t)program[pc + 3] << 8;
                    start |= (uint32_t)program[pc + 4] << 16;
                    start |= (uint32_t)program[pc + 5] << 24;
                    
                    uint32_t len = 0;
                    len |= (uint32_t)program[pc + 6];
                    len |= (uint32_t)program[pc + 7] << 8;
                    len |= (uint32_t)program[pc + 8] << 16;
                    len |= (uint32_t)program[pc + 9] << 24;
                    
                    cpu.print_memory(start, start + len);
                    next_pc = pc + 10;
                } else if (sub_opcode == 4) { // TRACE
                    uint8_t val = program[pc + 2];
                    if (val == 2) {
                        Config::debug = !Config::debug;
                    } else {
                        Config::debug = (val != 0);
                    }
                    next_pc = pc + 3;
                }
                
                cpu.set_pc(next_pc);
                break;
            }

            // Inlined LOAD_IMM64
            case 0x53: {
                #ifndef NDEBUG
                if (__builtin_expect(pc + 9 >= program.size(), 0)) {
                    running = false;
                    break;
                }
                #endif
                
                uint8_t reg = program[pc + 1];
                
                #ifndef NDEBUG
                if (__builtin_expect(reg >= TOTAL_REGISTERS, 0)) {
                    running = false;
                    break;
                }
                #endif
                
                // Read 64-bit value (8 bytes) in little-endian format
                uint64_t value = 0;
                value |= static_cast<uint64_t>(program[pc + 2]);
                value |= static_cast<uint64_t>(program[pc + 3]) << 8;
                value |= static_cast<uint64_t>(program[pc + 4]) << 16;
                value |= static_cast<uint64_t>(program[pc + 5]) << 24;
                value |= static_cast<uint64_t>(program[pc + 6]) << 32;
                value |= static_cast<uint64_t>(program[pc + 7]) << 40;
                value |= static_cast<uint64_t>(program[pc + 8]) << 48;
                value |= static_cast<uint64_t>(program[pc + 9]) << 56;
                
                #ifndef NDEBUG
                if (Config::trace) {
                    Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION,
                        fmt::format("[PC=0x{:04X}] [LOAD_IMM64] R{} = {}", pc, reg, value), Logging::DebugLevel::DETAIL);
                }
                #endif
                
                // Store in both 64-bit and 32-bit register arrays for compatibility
                cpu.get_registers_64()[reg] = value;
                cpu.get_registers()[reg] = static_cast<uint32_t>(value); // Also update legacy 32-bit array
                cpu.set_pc(pc + 10); // opcode + register + 8 bytes
                break;
            }
            
            // Non-inlined operations - delegate to original handlers
            default: {
                // Delegate to the consolidated dispatcher for anything not inlined above.
                // Guard against a stall: if dispatch_opcode returns without advancing PC
                // (and running is still true), the opcode was not recognised — stop
                // execution rather than looping forever on corrupt or malicious bytecode.
                uint32_t pre_pc = cpu.get_pc();
                dispatch_opcode(cpu, program, running);
                if (__builtin_expect(running && cpu.get_pc() == pre_pc, 0)) {
                    #ifndef NDEBUG
                    Logging::DebugHandler::instance().report(
                        Logging::DebugCategory::CPU_DISPATCHER,
                        fmt::format("[FALLBACK] Opcode 0x{:02X} at PC={} made no progress "
                                    "after consolidated dispatch; treating as invalid",
                                    opcode, pre_pc),
                        Logging::DebugLevel::CRITICAL);
                    #endif
                    running = false;
                }
                break;
            }
        }
    }
}
#pragma GCC diagnostic pop