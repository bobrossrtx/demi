#include "opcode_dispatcher_predictive.hpp"
#include "opcode_dispatcher_inlined.hpp"
#include "../cpu.hpp"
#include "../branch_prediction.hpp"
#include "../cpu_flags.hpp"
#include "../../debug/debug_handler.hpp"
#include "../../config.hpp"  // For Config::debug
#include "opcode_profiler.hpp"

void dispatch_opcode_with_prediction(CPU& cpu, 
                                   const std::vector<uint8_t>& program, 
                                   bool& running) {
    DEBUG_TRACE(Logging::DebugCategory::CPU_DISPATCHER, "Dispatcher entry at PC=0x{:04X}", cpu.get_pc());
    
    // Use predictive dispatcher if branch prediction is enabled
    if (cpu.get_branch_predictor().is_enabled()) {
        dispatch_opcode_predictive(cpu, program, running);
    } else {
        // Fall back to standard optimized dispatcher
        dispatch_opcode_inlined_optimized(cpu, program, running);
    }
}
#include "../cpu.hpp"
#include "../cpu_flags.hpp"
#include "../../config.hpp"
#include <fmt/format.h>

// External handler declarations for non-inlined operations
extern void handle_sub(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_jmp(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_call(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_load(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_store(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_push(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_pop(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_cmp(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_jz(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void handle_jnz(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
extern void dispatch_opcode(CPU& cpu, const std::vector<uint8_t>& program, bool& running);

void dispatch_opcode_predictive(CPU& cpu, const std::vector<uint8_t>& program, bool& running) {
    uint32_t pc = cpu.get_pc();
    
    // Bounds checking
    if (__builtin_expect(pc >= program.size(), 0)) {
        DEBUG_TRACE(Logging::DebugCategory::CPU_DISPATCHER, "[DISPATCH_PREDICTIVE] PC out of bounds{}", "");
        running = false;
        return;
    }
    
    uint8_t opcode = program[pc];

    // Profile opcode execution (TASK-005)
    OPCODE_PROFILE(opcode);

    // opcode is a uint8_t so its value is always in [0, 255].  The switch
    // below handles every value either explicitly or via `default`, so there
    // is no risk of an out-of-range switch index here.  The PC bounds check
    // above already guarantees program[pc] is a valid memory access.
    //
    // Check if this is a branch instruction that should be predicted
    if (BranchPrediction::BranchPredictor::is_branch_instruction(opcode)) {
        if (handle_predictive_branch(cpu, program, running, opcode)) {
            return; // Branch handled predictively
        }
        // Fall through to standard handling if prediction failed
    }
    
    // Execute instruction using optimized inlined dispatcher
    // This maintains the performance benefits of inlining while adding prediction
    switch (opcode) {
        case static_cast<uint8_t>(Opcode::NOP): {
            #ifndef NDEBUG
            // Config::debug is a runtime toggle (--debug flag). When disabled,
            // debug-only code paths are compiled out in release — intentional.
            if (Config::debug) {
                DEBUG_TRACE(Logging::DebugCategory::CPU_DISPATCHER, "[PC=0x{:04X}] [NOP] PC={}", pc, pc);
                cpu.print_state("NOP");
            }
            #endif
            cpu.set_pc(pc + 1);
            break;
        }
        
        case static_cast<uint8_t>(Opcode::LOAD_IMM): {
            #ifndef NDEBUG
            if (__builtin_expect(pc + 5 >= program.size(), 0)) {
                #ifdef VM_DEBUG_BOUNDS
                Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION,
                    fmt::format("[LOAD_IMM] Out of bounds at PC={}", pc), Logging::DebugLevel::CRITICAL);
                #endif
                running = false;
                return;
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
            if (__builtin_expect(reg >= cpu.get_registers_64().size(), 0)) {
                #ifdef VM_DEBUG_BOUNDS
                Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION,
                    fmt::format("[LOAD_IMM] Invalid register R{}", reg), Logging::DebugLevel::CRITICAL);
                #endif
                running = false;
                return;
            }
            #endif
            
            #ifndef NDEBUG
            // Config::debug is a runtime toggle (--debug flag). When disabled,
            // debug-only code paths are compiled out in release — intentional.
            if (Config::debug) {
                DEBUG_TRACE(Logging::DebugCategory::CPU_DISPATCHER, 
                    "[PC=0x{:04X}] [LOAD_IMM] R{} = {}", pc, reg, value);
            }
            #endif
            
            cpu.set_register_mode_aware(static_cast<Register>(reg), value);
            cpu.set_pc(pc + 6);
            break;
        }
        
        case static_cast<uint8_t>(Opcode::ADD): {
            #ifndef NDEBUG
            if (__builtin_expect(pc + 2 >= program.size(), 0)) {
                running = false;
                return;
            }
            #endif
            
            uint8_t reg1 = program[pc + 1];
            uint8_t reg2 = program[pc + 2];
            
            #ifndef NDEBUG
            if (__builtin_expect(reg1 >= TOTAL_REGISTERS || 
                                 reg2 >= TOTAL_REGISTERS, 0)) {
                running = false;
                return;
            }
            #endif
            
            uint64_t old_value = cpu.get_register_mode_aware(static_cast<Register>(reg1));
            uint64_t result = old_value + cpu.get_register_mode_aware(static_cast<Register>(reg2));
            
            // Set flags for overflow and carry
            uint32_t flags = cpu.get_flags();
            if (result < old_value) {
                flags |= FLAG_CARRY;
            } else {
                flags &= ~FLAG_CARRY;
            }
            
            if ((old_value & 0x80000000) == (cpu.get_register_mode_aware(static_cast<Register>(reg2)) & 0x80000000) &&
                (result & 0x80000000) != (old_value & 0x80000000)) {
                flags |= FLAG_OVERFLOW;
            } else {
                flags &= ~FLAG_OVERFLOW;
            }
            
            // result==0 is a legitimate arithmetic outcome checked here;
            // cppcheck cannot determine its value statically
            if (result == 0) {
                flags |= FLAG_ZERO;
            } else {
                flags &= ~FLAG_ZERO;
            }
            
            cpu.set_flags(flags);
            cpu.set_register_mode_aware(static_cast<Register>(reg1), result);
            
            // Also update the 64-bit register array to maintain consistency
            if (reg1 < cpu.get_registers_64().size()) {
                cpu.get_registers_64()[reg1] = static_cast<uint64_t>(result);
            }
            
            #ifndef NDEBUG
            // Config::debug is a runtime toggle (--debug flag). When disabled,
            // debug-only code paths are compiled out in release — intentional.
            if (Config::debug) {
                DEBUG_TRACE(Logging::DebugCategory::CPU_DISPATCHER, 
                    "[PC=0x{:04X}] [ADD] R{} = {} + {} = {}", 
                    pc, reg1, old_value, cpu.get_register_mode_aware(static_cast<Register>(reg2)), result);
            }
            #endif
            
            cpu.set_pc(pc + 3);
            break;
        }
        
        case static_cast<uint8_t>(Opcode::MOV): {
            #ifndef NDEBUG
            if (__builtin_expect(pc + 2 >= program.size(), 0)) {
                running = false;
                return;
            }
            #endif
            
            uint8_t reg1 = program[pc + 1];
            uint8_t reg2 = program[pc + 2];
            
            #ifndef NDEBUG
            if (__builtin_expect(reg1 >= TOTAL_REGISTERS || 
                                 reg2 >= TOTAL_REGISTERS, 0)) {
                running = false;
                return;
            }
            #endif
            
            // Use mode-aware register access to ensure consistency between legacy and 64-bit registers
            uint64_t val = cpu.get_register_mode_aware(static_cast<Register>(reg2));
            cpu.set_register_mode_aware(static_cast<Register>(reg1), val);
            
            // Also update legacy registers directly if needed
            if (reg1 < 8) {
                cpu.set_register_mode_aware(static_cast<Register>(reg1), static_cast<uint32_t>(val));
            }
            
            #ifndef NDEBUG
            // Config::debug is a runtime toggle (--debug flag). When disabled,
            // debug-only code paths are compiled out in release — intentional.
            if (Config::debug) {
                DEBUG_TRACE(Logging::DebugCategory::CPU_DISPATCHER, 
                    "[PC=0x{:04X}] [MOV] R{} = R{} ({})", 
                    pc, reg1, reg2, val);
            }
            #endif
            
            cpu.set_pc(pc + 3);
            break;
        }
        
        case static_cast<uint8_t>(Opcode::HALT): {
            #ifndef NDEBUG
            // Config::debug is a runtime toggle (--debug flag). When disabled,
            // debug-only code paths are compiled out in release — intentional.
            if (Config::debug) {
                DEBUG_TRACE(Logging::DebugCategory::CPU_DISPATCHER, "[PC=0x{:04X}] [HALT]", cpu.get_pc());
            }
            #endif
            
            running = false;
            cpu.set_pc(cpu.get_pc() + 1);  // Advance PC past HALT instruction
            break;
        }
        
        case static_cast<uint8_t>(Opcode::INC64): {
            #ifndef NDEBUG
            if (__builtin_expect(pc + 1 >= program.size(), 0)) {
                running = false;
                return;
            }
            #endif
            uint8_t reg = program[pc + 1];
            #ifndef NDEBUG
            if (__builtin_expect(reg >= cpu.get_registers_64().size(), 0)) {
                running = false;
                return;
            }
            #endif
            uint64_t value = cpu.get_registers_64()[reg];
            uint64_t result = value + 1;
            uint32_t flags = cpu.get_flags();
            if (value == 0x7FFFFFFFFFFFFFFF) {
                flags |= FLAG_OVERFLOW;
            } else {
                flags &= ~FLAG_OVERFLOW;
            }
            // result==0 is a legitimate arithmetic outcome checked here;
            // cppcheck cannot determine its value statically
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
            cpu.get_registers_64()[reg] = result;
            if (reg < 8) {
                cpu.set_register_mode_aware(static_cast<Register>(reg), static_cast<uint32_t>(result));
            }
            cpu.set_pc(pc + 2);
            break;
        }
        
        case static_cast<uint8_t>(Opcode::DEC64): {
            #ifndef NDEBUG
            if (__builtin_expect(pc + 1 >= program.size(), 0)) {
                running = false;
                return;
            }
            #endif
            uint8_t reg = program[pc + 1];
            #ifndef NDEBUG
            if (__builtin_expect(reg >= cpu.get_registers_64().size(), 0)) {
                running = false;
                return;
            }
            #endif
            uint64_t value = cpu.get_registers_64()[reg];
            uint64_t result = value - 1;
            uint32_t flags = cpu.get_flags();
            if (value == 0x8000000000000000) {
                flags |= FLAG_OVERFLOW;
            } else {
                flags &= ~FLAG_OVERFLOW;
            }
            // result==0 is a legitimate arithmetic outcome checked here;
            // cppcheck cannot determine its value statically
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
            cpu.get_registers_64()[reg] = result;
            if (reg < 8) {
                cpu.set_register_mode_aware(static_cast<Register>(reg), static_cast<uint32_t>(result));
            }
            cpu.set_pc(pc + 2);
            break;
        }
        
        // For complex operations, delegate to the consolidated dispatcher
        default: {
            uint32_t fallback_pc = cpu.get_pc();
            dispatch_opcode(cpu, program, running);
            if (__builtin_expect(running && cpu.get_pc() == fallback_pc, 0)) {
                #ifndef NDEBUG
                Logging::DebugHandler::instance().report(
                    Logging::DebugCategory::CPU_DISPATCHER,
                    fmt::format("[DISPATCH_PREDICTIVE] Opcode 0x{:02X} at PC={} "
                                "made no progress; treating as invalid",
                                opcode, fallback_pc),
                    Logging::DebugLevel::CRITICAL);
                #endif
                running = false;
            }
            break;
        }
    }
}

bool handle_predictive_branch(CPU& cpu, const std::vector<uint8_t>& program, 
                             bool& running, uint8_t opcode) {
    uint32_t pc = cpu.get_pc();
    auto& branch_predictor = cpu.get_branch_predictor();
    
    // Get branch prediction
    auto prediction = branch_predictor.predict(pc, opcode);
    
    // For unconditional branches, prediction is always correct
    if (opcode == static_cast<uint8_t>(Opcode::JMP) ||
        opcode == static_cast<uint8_t>(Opcode::CALL)) {
        
        // Execute the branch normally
        if (opcode == static_cast<uint8_t>(Opcode::JMP)) {
            handle_jmp(cpu, program, running);
        } else if (opcode == static_cast<uint8_t>(Opcode::CALL)) {
            handle_call(cpu, program, running);
        }
        
        // Update predictor with outcome
        BranchPrediction::BranchOutcome outcome;
        outcome.pc = pc;
        outcome.actually_taken = true;
        outcome.actual_target = cpu.get_pc();
        outcome.opcode = opcode;
        branch_predictor.update(outcome);
        
        return true;
    }
    
    // For conditional branches, we need to evaluate the condition
    bool condition_met = false;
    uint32_t target_address = 0;
    
    // Extract target address (4-byte operand)
    if (pc + 5 <= program.size()) {
        target_address |= static_cast<uint32_t>(program[pc + 1]);
        target_address |= static_cast<uint32_t>(program[pc + 2]) << 8;
        target_address |= static_cast<uint32_t>(program[pc + 3]) << 16;
        target_address |= static_cast<uint32_t>(program[pc + 4]) << 24;
    }
    
    // Evaluate branch condition
    uint32_t flags = cpu.get_flags();
    DEBUG_TRACE(Logging::DebugCategory::CPU_PREDICTION, "handle_predictive_branch called for opcode 0x{:02X} at PC=0x{:04X}", opcode, pc);
    switch (static_cast<Opcode>(opcode)) {
        case Opcode::JZ:
            condition_met = (flags & FLAG_ZERO) != 0;
            break;
        case Opcode::JNZ:
            condition_met = (flags & FLAG_ZERO) == 0;
            break;
        case Opcode::JC:
            condition_met = (flags & FLAG_CARRY) != 0;
            break;
        case Opcode::JNC:
            condition_met = (flags & FLAG_CARRY) == 0;
            break;
        case Opcode::JS:
            condition_met = (flags & FLAG_SIGN) != 0;
            break;
        case Opcode::JNS:
            condition_met = (flags & FLAG_SIGN) == 0;
            break;
        case Opcode::JO:
            condition_met = (flags & FLAG_OVERFLOW) != 0;
            break;
        case Opcode::JNO:
            condition_met = (flags & FLAG_OVERFLOW) == 0;
            break;
        default:
            return false; // Unknown branch type
    }
    
    // Check if prediction was correct
    bool prediction_correct = (prediction.predicted_taken == condition_met);
    
    if (prediction_correct) {
        // Prediction was correct
        // Execute branch normally
        if (condition_met) {
            cpu.set_pc(target_address);
        } else {
            cpu.set_pc(pc + 5); // Skip to next instruction (opcode + 4 bytes address)
        }
    } else {
        // Prediction was wrong - execute correctly
        if (condition_met) {
            cpu.set_pc(target_address);
        } else {
            cpu.set_pc(pc + 5);
        }
    }
    
    // Update branch predictor with actual outcome
    BranchPrediction::BranchOutcome outcome;
    outcome.pc = pc;
    outcome.actually_taken = condition_met;
    outcome.actual_target = condition_met ? target_address : (pc + 5);
    outcome.opcode = opcode;
    branch_predictor.update(outcome);
    
    #ifndef NDEBUG
    if (Config::debug) {
        Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION,
            fmt::format("[BRANCH_PREDICTION] PC=0x{:04X}, opcode=0x{:02X}, predicted={}, actual={}, correct={}",
            pc, opcode, prediction.predicted_taken, condition_met, prediction_correct), Logging::DebugLevel::DETAIL);
    }
    #endif
    
    return true;
}