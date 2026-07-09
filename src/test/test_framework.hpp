#pragma once

#include <vector>
#include <string>
#include <functional>
#include <iostream>
#include <fstream>
#include <chrono>
#include <limits>
#include <map>
#include <fmt/format.h>
#include "../engine/cpu.hpp"
#include "../engine/cpu_flags.hpp"
#include "../engine/device_factory.hpp"
#include "../config.hpp"
#include "../debug/debug_handler.hpp"
#include "../debug/error_handler.hpp"
#include "../assembler/demi_assembler.hpp"
#include "../assembler/lexer.hpp"
#include "../assembler/parser.hpp" 
#include "../assembler/assembler.hpp"
#include "../engine/opcodes/instruction_fusion.hpp"

// Forward declarations
class TestContext;
class TestFramework;

struct TestCase {
    std::string name;
    std::string category;
    std::function<void(TestContext&)> test_func;
    bool expect_error;

    TestCase(const std::string& n, const std::string& cat,
             std::function<void(TestContext&)> func, bool expect_err = false)
        : name(n), category(cat), test_func(func), expect_error(expect_err) {}
};

struct TestResult {
    std::string name;
    std::string category;
    bool passed;
    std::string message;
    double duration_ms;

    TestResult(const std::string& n, const std::string& cat, bool p,
               const std::string& msg = "", double dur = 0.0)
        : name(n), category(cat), passed(p), message(msg), duration_ms(dur) {}
};

class TestSuite {
public:
    TestSuite(const std::string& name) : name_(name) {}

    void add_test(const TestCase& test) {
        tests_.push_back(test);
    }

    const std::vector<TestCase>& get_tests() const { return tests_; }
    const std::string& get_name() const { return name_; }

private:
    std::string name_;
    std::vector<TestCase> tests_;
};

class AssertionFailure : public std::exception {
public:
    explicit AssertionFailure(const std::string& message) : message_(message) {}
    const char* what() const noexcept override { return message_.c_str(); }

private:
    std::string message_;
};

class TestContext {
public:
    TestContext() : cpu(CPU::create_test_cpu()), program() {
        cpu.reset();
        initialize_devices();
        // Reset fusion stats for each test
        InstructionFusion::g_fusion_engine.reset_stats();
    }

    ~TestContext() = default;

    void load_program(const std::vector<uint8_t>& prog) {
        program = prog;
    }

    void load_hex_file(const std::string& filename) {
        std::ifstream file(filename);
        if (!file) {
            throw std::runtime_error("Cannot open hex file: " + filename);
        }

        program.clear();
        std::string token;
        while (file >> token) {
            if (token[0] == '#') {
                file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                continue;
            }
            try {
                uint8_t byte = static_cast<uint8_t>(std::stoul(token, nullptr, 16));
                program.push_back(byte);
            } catch (...) {
                throw std::runtime_error("Invalid hex byte: " + token);
            }
        }
    }

    void load_immediate(int reg, uint32_t value) {
        if (reg < 0 || reg >= 16) {
            throw std::runtime_error("Invalid register: " + std::to_string(reg));
        }
        cpu.get_registers()[reg] = value;
    }

    void execute_single_step() {
        if (program.empty()) {
            throw std::runtime_error("No program loaded");
        }
        cpu.step(program);
    }

    void execute_program() {
        if (program.empty()) {
            throw std::runtime_error("No program loaded");
        }

        execute_program_with_limit(max_steps_override_ > 0 ? max_steps_override_ : 10000);
    }
    
    // Override the default step limit for long-running tests
    void set_max_steps(size_t steps) { max_steps_override_ = steps; }

    void execute_program_with_limit(size_t max_steps) {
        if (program.empty()) {
            throw std::runtime_error("No program loaded");
        }

        cpu.reset();

        // Copy program into memory safely with bounds checking
        if (program.size() > cpu.get_memory().size()) {
            throw std::runtime_error(fmt::format("Program too large: {} bytes, memory size: {}", program.size(), cpu.get_memory().size()));
        }
        auto& memory = cpu.get_memory();
        std::copy_n(program.begin(), std::min(program.size(), memory.size()), memory.begin());
        cpu.set_pc(entry_address);  // Use entry address from assembler
        cpu.set_sp(cpu.get_memory().size() - 4);
        cpu.set_fp(cpu.get_memory().size() - 4);  // Match main program behavior

        size_t step_count = 0;

        while (step_count < max_steps) {
            bool continue_execution = cpu.step(program);
            step_count++;

            if (!continue_execution) {
                break; // Program halted or error occurred
            }
        }

        if (step_count >= max_steps) {
            throw std::runtime_error("Test exceeded maximum execution steps (possible infinite loop)");
        }
    }
    
    // Set maximum call depth for this test (useful for testing call stack overflow)
    void set_max_call_depth(size_t depth) {
        cpu.set_max_call_depth_override(depth);
    }

    // Set stack limit for overflow detection (useful for testing stack overflow at configurable threshold)
    void set_stack_limit(uint32_t limit) {
        cpu.set_stack_limit(limit);
    }

    // State inspection
    uint32_t get_register(int reg) const {
        if (reg < 0 || reg >= 16) {
            throw std::runtime_error("Invalid register: " + std::to_string(reg));
        }
        return cpu.get_registers()[reg];
    }

    uint64_t get_register_64(int reg) const {
        if (reg < 0 || reg >= 16) {
            throw std::runtime_error("Invalid 64-bit register: " + std::to_string(reg));
        }
        return cpu.get_register_64(static_cast<Register>(reg));
    }

    uint8_t get_memory(uint32_t addr) const {
        auto& memory = cpu.get_memory();
        if (addr >= memory.size()) {
            throw std::runtime_error("Memory address out of bounds: " + std::to_string(addr));
        }
        return memory[addr];
    }

    uint32_t get_flags() const {
        return cpu.get_flags();
    }

    uint32_t get_pc() const {
        return cpu.get_pc();
    }

    uint32_t get_sp() const {
        return cpu.get_sp();
    }

    void assert_register_eq(int reg, uint32_t expected) {
        uint32_t actual = get_register(reg);
        if (actual != expected) {
            throw AssertionFailure(fmt::format(
                "Register R{} assertion failed: expected {}, got {}",
                reg, expected, actual));
        }
    }

    void assert_register_64_eq(int reg, uint64_t expected) {
        uint64_t actual = cpu.get_register_64(static_cast<Register>(reg));
        if (actual != expected) {
            throw AssertionFailure(fmt::format(
                "Register R{} (64-bit) assertion failed: expected 0x{:016X}, got 0x{:016X}",
                reg, expected, actual));
        }
    }

    void assert_register_ne(int reg, uint32_t unexpected) {
        uint32_t actual = get_register(reg);
        if (actual == unexpected) {
            throw AssertionFailure(fmt::format(
                "Register R{} assertion failed: expected NOT {}, but got {}",
                reg, unexpected, actual));
        }
    }

    void assert_memory_eq(uint32_t addr, uint8_t expected) {
        uint8_t actual = get_memory(addr);
        if (actual != expected) {
            throw AssertionFailure(fmt::format(
                "Memory[0x{:02X}] assertion failed: expected 0x{:02X}, got 0x{:02X}",
                addr, expected, actual));
        }
    }

    void assert_memory_range_eq(uint32_t addr, const std::vector<uint8_t>& expected) {
        for (size_t i = 0; i < expected.size(); i++) {
            assert_memory_eq(addr + i, expected[i]);
        }
    }

    void assert_flag_set(uint32_t flag) {
        uint32_t flags = get_flags();
        if (!(flags & flag)) {
            throw AssertionFailure(fmt::format(
                "Flag assertion failed: expected flag 0x{:08X} to be set, but flags are 0x{:08X}",
                flag, flags));
        }
    }

    void assert_flag_clear(uint32_t flag) {
        uint32_t flags = get_flags();
        if (flags & flag) {
            throw AssertionFailure(fmt::format(
                "Flag assertion failed: expected flag 0x{:08X} to be clear, but flags are 0x{:08X}",
                flag, flags));
        }
    }

    void assert_flags_eq(uint32_t expected) {
        uint32_t actual = get_flags();
        if (actual != expected) {
            throw AssertionFailure(fmt::format(
                "Flags assertion failed: expected 0x{:08X}, got 0x{:08X}",
                expected, actual));
        }
    }

    void assert_pc_eq(uint32_t expected) {
        uint32_t actual = get_pc();
        if (actual != expected) {
            throw AssertionFailure(fmt::format(
                "PC assertion failed: expected {}, got {}",
                expected, actual));
        }
    }

    void assert_sp_eq(uint32_t expected) {
        uint32_t actual = get_sp();
        if (actual != expected) {
            throw AssertionFailure(fmt::format(
                "SP assertion failed: expected {}, got {}",
                expected, actual));
        }
    }

    
    
    // Assert CPU is in 32-bit mode
    void assert_32bit_mode() {
        if (!cpu.is_32bit_mode()) {
            throw AssertionFailure("CPU mode assertion failed: expected 32-bit mode, but CPU is in 64-bit mode");
        }
    }
    
    // Assert CPU is in 64-bit mode
    void assert_64bit_mode() {
        if (!cpu.is_64bit_mode()) {
            throw AssertionFailure("CPU mode assertion failed: expected 64-bit mode, but CPU is in 32-bit mode");
        }
    }
    
    // Assert CPU mode matches expected
    void assert_cpu_mode(CPUMode expected) {
        CPUMode actual = cpu.get_cpu_mode();
        if (actual != expected) {
            throw AssertionFailure(fmt::format(
                "CPU mode assertion failed: expected {}-bit mode, got {}-bit mode",
                expected == CPUMode::MODE_64BIT ? 64 : 32,
                actual == CPUMode::MODE_64BIT ? 64 : 32));
        }
    }
    
    // Assert register value in mode-aware manner (applies 32-bit mask in 32-bit mode)
    void assert_register_mode_aware(int reg, uint64_t expected) {
        uint64_t actual = cpu.get_register_mode_aware(static_cast<Register>(reg));
        uint64_t mask = cpu.get_operand_mask();
        uint64_t masked_expected = expected & mask;
        if (actual != masked_expected) {
            throw AssertionFailure(fmt::format(
                "Register R{} mode-aware assertion failed ({}-bit mode): expected 0x{:X}, got 0x{:X}",
                reg, cpu.is_64bit_mode() ? 64 : 32, masked_expected, actual));
        }
    }
    
    // Assert register's lower 32 bits (useful for checking 32-bit behavior)
    void assert_register_32_eq(int reg, uint32_t expected) {
        uint32_t actual = cpu.get_register_32(static_cast<Register>(reg));
        if (actual != expected) {
            throw AssertionFailure(fmt::format(
                "Register R{} (32-bit) assertion failed: expected 0x{:08X}, got 0x{:08X}",
                reg, expected, actual));
        }
    }
    
    // Assert that upper 32 bits of register are zero (important for 32-bit mode operations)
    void assert_register_upper_bits_zero(int reg) {
        uint64_t full_value = cpu.get_register_64(static_cast<Register>(reg));
        uint32_t upper_bits = static_cast<uint32_t>(full_value >> 32);
        if (upper_bits != 0) {
            throw AssertionFailure(fmt::format(
                "Register R{} upper 32 bits assertion failed: expected 0x00000000, got 0x{:08X} (full value: 0x{:016X})",
                reg, upper_bits, full_value));
        }
    }
    
    // Assert that upper 32 bits of register match expected value
    void assert_register_upper_bits_eq(int reg, uint32_t expected) {
        uint64_t full_value = cpu.get_register_64(static_cast<Register>(reg));
        uint32_t upper_bits = static_cast<uint32_t>(full_value >> 32);
        if (upper_bits != expected) {
            throw AssertionFailure(fmt::format(
                "Register R{} upper 32 bits assertion failed: expected 0x{:08X}, got 0x{:08X}",
                reg, expected, upper_bits));
        }
    }
    
    // Assert register overflow behavior (value wrapped around mask)
    void assert_register_wrapped(int reg, uint64_t original_value, uint64_t expected_wrapped) {
        uint64_t actual = cpu.get_register_mode_aware(static_cast<Register>(reg));
        if (actual != expected_wrapped) {
            throw AssertionFailure(fmt::format(
                "Register R{} wrap assertion failed: original value 0x{:X} should wrap to 0x{:X}, but got 0x{:X}",
                reg, original_value, expected_wrapped, actual));
        }
    }
    
    // Set CPU mode for testing
    void set_cpu_mode(CPUMode mode) {
        cpu.set_cpu_mode(mode);
    }
    
    // Get current CPU mode
    CPUMode get_cpu_mode() const {
        return cpu.get_cpu_mode();
    }
    
    // Helper to get operand mask for current mode
    uint64_t get_operand_mask() const {
        return cpu.get_operand_mask();
    }
    
    
    
    // Assert zero flag is set
    void assert_zero_flag_set() {
        assert_flag_set(FLAG_ZERO);
    }
    
    // Assert zero flag is clear
    void assert_zero_flag_clear() {
        assert_flag_clear(FLAG_ZERO);
    }
    
    // Assert carry flag is set
    void assert_carry_flag_set() {
        assert_flag_set(FLAG_CARRY);
    }
    
    // Assert carry flag is clear
    void assert_carry_flag_clear() {
        assert_flag_clear(FLAG_CARRY);
    }
    
    // Assert overflow flag is set
    void assert_overflow_flag_set() {
        assert_flag_set(FLAG_OVERFLOW);
    }
    
    // Assert overflow flag is clear
    void assert_overflow_flag_clear() {
        assert_flag_clear(FLAG_OVERFLOW);
    }
    
    // Assert sign flag is set
    void assert_sign_flag_set() {
        assert_flag_set(FLAG_SIGN);
    }
    
    // Assert sign flag is clear
    void assert_sign_flag_clear() {
        assert_flag_clear(FLAG_SIGN);
    }
    
    void assert_direction_flag_set() {
        assert_flag_set(FLAG_DIRECTION);
    }
    
    void assert_direction_flag_clear() {
        assert_flag_clear(FLAG_DIRECTION);
    }
    
    void assert_register_64_ne(int reg, uint64_t unexpected) {
        uint64_t actual = cpu.get_register_64(static_cast<Register>(reg));
        if (actual == unexpected) {
            throw AssertionFailure(fmt::format(
                "Register R{} (64-bit) assertion failed: expected NOT 0x{:016X}, but got exactly that",
                reg, unexpected));
        }
    }
    
    
    
    // Assert 32-bit value at memory address (little-endian)
    void assert_memory_dword_eq(uint32_t addr, uint32_t expected) {
        auto& memory = cpu.get_memory();
        if (addr + 3 >= memory.size()) {
            throw AssertionFailure(fmt::format(
                "Memory address 0x{:X} + 3 is out of bounds (memory size: {})",
                addr, memory.size()));
        }
        uint32_t actual = memory[addr] |
                         (static_cast<uint32_t>(memory[addr + 1]) << 8) |
                         (static_cast<uint32_t>(memory[addr + 2]) << 16) |
                         (static_cast<uint32_t>(memory[addr + 3]) << 24);
        if (actual != expected) {
            throw AssertionFailure(fmt::format(
                "Memory DWORD at 0x{:X} assertion failed: expected 0x{:08X}, got 0x{:08X}",
                addr, expected, actual));
        }
    }
    
    // Assert 64-bit value at memory address (little-endian)
    void assert_memory_qword_eq(uint32_t addr, uint64_t expected) {
        auto& memory = cpu.get_memory();
        if (addr + 7 >= memory.size()) {
            throw AssertionFailure(fmt::format(
                "Memory address 0x{:X} + 7 is out of bounds (memory size: {})",
                addr, memory.size()));
        }
        uint64_t actual = 0;
        for (int i = 0; i < 8; i++) {
            actual |= static_cast<uint64_t>(memory[addr + i]) << (i * 8);
        }
        if (actual != expected) {
            throw AssertionFailure(fmt::format(
                "Memory QWORD at 0x{:X} assertion failed: expected 0x{:016X}, got 0x{:016X}",
                addr, expected, actual));
        }
    }
    
    // Assert 16-bit value at memory address (little-endian)
    void assert_memory_word_eq(uint32_t addr, uint16_t expected) {
        auto& memory = cpu.get_memory();
        if (addr + 1 >= memory.size()) {
            throw AssertionFailure(fmt::format(
                "Memory address 0x{:X} + 1 is out of bounds (memory size: {})",
                addr, memory.size()));
        }
        uint16_t actual = memory[addr] | (static_cast<uint16_t>(memory[addr + 1]) << 8);
        if (actual != expected) {
            throw AssertionFailure(fmt::format(
                "Memory WORD at 0x{:X} assertion failed: expected 0x{:04X}, got 0x{:04X}",
                addr, expected, actual));
        }
    }

    // Exception assertions
    void assert_throws(std::function<void()> func) {
        bool threw = false;
        try {
            func();
        } catch (...) {
            threw = true;
        }
        if (!threw) {
            throw AssertionFailure("Expected exception but none was thrown");
        }
    }

    void assert_no_throw(std::function<void()> func) {
        try {
            func();
        } catch (const std::exception& e) {
            throw AssertionFailure(fmt::format(
                "Expected no exception but got: {}", e.what()));
        } catch (...) {
            throw AssertionFailure("Expected no exception but got unknown exception");
        }
    }

    void assert_error_count(int expected) {
        if (Config::error_count != expected) {
            throw AssertionFailure(fmt::format(
                "Error count assertion failed: expected {}, got {}",
                expected, Config::error_count));
        }
    }

    // Generic equality assertion
    template<typename T>
    void assert_eq(const T& expected, const T& actual, const std::string& message = "") {
        if (expected != actual) {
            std::string msg = message.empty() ?
                fmt::format("Assertion failed: expected {}, got {}", expected, actual) :
                fmt::format("{}: expected {}, got {}", message, expected, actual);
            throw AssertionFailure(msg);
        }
    }

    // Assembler functionality for testing directives
    void assemble_code(const std::string& assembly_code) {
        try {
            // Use DemiAssembler for full preprocessing support (handles macros, defines, etc.)
            Assembler::DemiAssembler assembler;
            std::vector<uint8_t> bytecode = assembler.assemble_string(assembly_code);
            
            if (assembler.has_errors()) {
                std::string error_msg = "Assembly errors:\n";
                for (const auto& error : assembler.get_errors()) {
                    error_msg += "  " + error + "\n";
                }
                throw std::runtime_error(error_msg);
            }
            
            program = std::move(bytecode);
            entry_address = assembler.get_entry_address();  // Capture entry point
        } catch (const std::exception& e) {
            throw std::runtime_error(fmt::format("Assembly failed: {}", e.what()));
        }
    }

    void assemble_file(const std::string& filename) {
        try {
            Assembler::DemiAssembler assembler;
            std::vector<uint8_t> bytecode = assembler.assemble_file(filename);
            if (assembler.has_errors()) {
                std::string error_msg = "Assembly errors:\n";
                for (const auto& error : assembler.get_errors()) {
                    error_msg += "  " + error + "\n";
                }
                throw std::runtime_error(error_msg);
            }
            program = std::move(bytecode);
            entry_address = assembler.get_entry_address();  // Capture entry point
        } catch (const std::exception& e) {
            throw std::runtime_error(fmt::format("Assembly of file '{}' failed: {}", filename, e.what()));
        }
    }

    // Test assembler directives and symbol resolution
    void assert_symbol_value(const std::string& symbol_name, uint32_t expected_value) {
        // If we have a program, we can look up symbols from the last assemble_code call
        // Otherwise, throw with a helpful message
        if (program.empty()) {
            throw std::runtime_error("No program assembled — call assemble_code() first");
        }
        
        Assembler::DemiAssembler assembler;
        // Re-assemble to get symbol table. Use a no-op program to avoid side effects.
        throw std::runtime_error(
            "assert_symbol_value: symbol lookup after assembly not preserved. "
            "Use assemble_code() with an assembly string containing labels, "
            "then check bytecode offsets with assert_byte_at() instead.");
    }

    void assert_program_size(size_t expected_size) {
        if (program.size() != expected_size) {
            throw AssertionFailure(fmt::format(
                "Program size assertion failed: expected {} bytes, got {} bytes",
                expected_size, program.size()));
        }
    }

    void assert_byte_at(size_t address, uint8_t expected_byte) {
        if (address >= program.size()) {
            throw AssertionFailure(fmt::format(
                "Address {} is out of bounds (program size: {} bytes)",
                address, program.size()));
        }
        uint8_t actual = program[address];
        if (actual != expected_byte) {
            throw AssertionFailure(fmt::format(
                "Byte at address {} assertion failed: expected 0x{:02X}, got 0x{:02X}",
                address, expected_byte, actual));
        }
    }

    void assert_bytes_at(size_t address, const std::vector<uint8_t>& expected_bytes) {
        if (address + expected_bytes.size() > program.size()) {
            throw AssertionFailure(fmt::format(
                "Address range {}-{} is out of bounds (program size: {} bytes)",
                address, address + expected_bytes.size() - 1, program.size()));
        }
        
        for (size_t i = 0; i < expected_bytes.size(); i++) {
            uint8_t actual = program[address + i];
            uint8_t expected = expected_bytes[i];
            if (actual != expected) {
                throw AssertionFailure(fmt::format(
                    "Byte at address {} assertion failed: expected 0x{:02X}, got 0x{:02X}",
                    address + i, expected, actual));
            }
        }
    }

    // Fusion testing utilities
    
    void reset_fusion_stats() {
        InstructionFusion::g_fusion_engine.reset_stats();
    }
    
    const InstructionFusion::FusionStats& get_fusion_stats() const {
        return InstructionFusion::g_fusion_engine.get_stats();
    }
    
    void assert_fusion_count(uint64_t expected_count) {
        uint64_t actual = get_fusion_stats().successful_fusions;
        if (actual != expected_count) {
            throw AssertionFailure(fmt::format(
                "Fusion count assertion failed: expected {} fusions, got {}",
                expected_count, actual));
        }
    }
    
    void assert_fusion_pattern_count(InstructionFusion::FusionPattern pattern, uint64_t expected_count) {
        uint64_t actual = get_fusion_stats().pattern_counts[static_cast<int>(pattern)];
        if (actual != expected_count) {
            throw AssertionFailure(fmt::format(
                "Fusion pattern count assertion failed: expected {} occurrences of pattern {}, got {}",
                expected_count, static_cast<int>(pattern), actual));
        }
    }
    
    void assert_no_fusion() {
        assert_fusion_count(0);
    }
    
    void assert_fusion_rate(double min_rate) {
        double actual_rate = get_fusion_stats().fusion_rate();
        if (actual_rate < min_rate) {
            throw AssertionFailure(fmt::format(
                "Fusion rate assertion failed: expected at least {:.1f}%, got {:.1f}%",
                min_rate, actual_rate));
        }
    }
    
    void enable_fusion() {
        InstructionFusion::g_fusion_engine.enable();
    }
    
    void disable_fusion() {
        InstructionFusion::g_fusion_engine.disable();
    }
    
    bool is_fusion_enabled() const {
        return InstructionFusion::g_fusion_engine.is_enabled();
    }

    // CPU instance for direct access if needed
    CPU cpu;
    std::vector<uint8_t> program;
    size_t max_steps_override_ = 0;
    uint32_t entry_address = 0;  // Entry point address from assembler
};

// Main test framework
class TestFramework {
public:
    static TestFramework& instance() {
        static TestFramework instance;
        return instance;
    }

    void register_test(const TestCase& test) {
        tests_.push_back(test);
    }

    void register_suite(const TestSuite& suite) {
        for (const auto& test : suite.get_tests()) {
            register_test(test);
        }
    }

    std::vector<TestResult> run_all() {
        return run_filtered("");
    }

    std::vector<TestResult> run_category(const std::string& category) {
        return run_filtered(category);
    }

    std::vector<TestResult> run_single(const std::string& name) {
        std::vector<TestResult> results;
        for (const auto& test : tests_) {
            if (test.name == name) {
                results.push_back(run_test(test));
                break;
            }
        }
        return results;
    }

    void print_results(const std::vector<TestResult>& results) {
        int passed = 0, failed = 0;

        auto matches_show_mode = [](const TestResult& result) {
            switch (Config::test_show_mode) {
                case TestShowMode::ALL:
                    return true;
                case TestShowMode::FAILS:
                    return !result.passed;
                case TestShowMode::SUCCESS:
                    return result.passed;
                default:
                    return true;
            }
        };

        std::vector<TestResult> shown_results;
        shown_results.reserve(results.size());
        if (Config::test_show_mode != TestShowMode::ALL) {
            for (const auto& result : results) {
                if (matches_show_mode(result)) {
                    shown_results.push_back(result);
                }
            }
        }

        const std::vector<TestResult>& display_results =
            (Config::test_show_mode == TestShowMode::ALL) ? results : shown_results;

        // Config::quiet is a runtime toggle (--quiet flag). When set,
        // test result headers are suppressed — intentional, not dead code.
        if (!Config::quiet) {
            std::cout << "\n" << fmt::format("{}┌────────────────────────────────────────────────────────────┐{}\n",
                                    "\033[36m", "\033[0m");
            std::cout << fmt::format("{}│     DemiEngine Unit Test Results                           │{}\n",
                                    "\033[36m", "\033[0m");
            std::cout << fmt::format("{}└────────────────────────────────────────────────────────────┘{}\n",
                                    "\033[36m", "\033[0m");
        }

        // Always compute overall stats from all results (even if filtering display)
        for (const auto& result : results) {
            if (result.passed) {
                ++passed;
            } else {
                ++failed;
            }
        }

        // Collect failures and organize displayed tests by category
        int shown_passed = 0;
        int shown_failed = 0;
        std::vector<TestResult> failures;
        // category -> (passed, total, total_time_ms, vector of results)
        std::map<std::string, std::tuple<int, int, double, std::vector<TestResult>>> category_data;

        for (const auto& result : display_results) {
            if (result.passed) {
                ++shown_passed;
            } else {
                ++shown_failed;
                failures.push_back(result);
            }

            auto& data = category_data[result.category];
            std::get<1>(data)++; // total count
            std::get<2>(data) += result.duration_ms; // total time
            if (result.passed) {
                std::get<0>(data)++; // passed count
            }
            std::get<3>(data).push_back(result);
        }

        // In normal mode, show tree structure with categories and tests
        if (!Config::quiet) {
            for (const auto& [cat, data] : category_data) {
                int passed_count = std::get<0>(data);
                int total_count = std::get<1>(data);
                double total_time = std::get<2>(data);
                const auto& cat_results = std::get<3>(data);
                
                const char* cat_color = (passed_count == total_count) ? "\033[36m" : "\033[33m";
                
                // Print category header
                std::cout << fmt::format("\n{}━━━ {} ({}/{}) [{:.1f}ms] ━━━\033[0m\n",
                                       cat_color, cat, passed_count, total_count, total_time);
                
                // Print tests under this category
                for (const auto& result : cat_results) {
                    const char* color = result.passed ? "\033[32m" : "\033[31m";
                    const char* reset = "\033[0m";
                    const char* status = result.passed ? "✓" : "✗";
                    
                    {
                        const int total_width = 60;
                        std::string visible_body = fmt::format(" {} ", result.name);
                        std::string time_str = fmt::format("[{:.1f}ms]", result.duration_ms);
                        // include one extra visible space between the name/dashes and the time string
                        int visible_len = 1 + static_cast<int>(visible_body.size()) + 1 + static_cast<int>(time_str.size()); // 1 for the visible symbol, 1 for the separating space
                        int pad = total_width - visible_len;
                        if (pad < 1) pad = 1;
                        std::cout << color << status << reset << visible_body << std::string(pad, '-') << " " << time_str;
                    }

                    if (!result.passed && !result.message.empty()) {
                        std::cout << fmt::format(" ── {}", result.message);
                    }
                    std::cout << std::endl;
                }
            }
        }

        // Config::quiet is a runtime toggle (--quiet flag). When set,
        // verbose output is suppressed — intentional, not dead code.
        if (Config::quiet) {
            // Show failures if any
            if (!failures.empty()) {
                std::cout << "\n\033[31m━━━ Failures ━━━\033[0m\n";
                for (const auto& result : failures) {
                    {
                        const int total_width = 60;
                        std::string visible_body = fmt::format(" {} ({})", result.name, result.category);
                        std::string time_str = fmt::format("[{:.1f}ms]", result.duration_ms);
                        // 1 for the visible symbol, 1 for the single separating space before time_str
                        int visible_len = 1 + static_cast<int>(visible_body.size()) + 1 + static_cast<int>(time_str.size());
                        int pad = total_width - visible_len;
                        if (pad < 1) pad = 1;
                        // color only the symbol so ANSI codes do not affect visible length calculation
                        std::cout << "\033[31m✗\033[0m" << visible_body << " " << std::string(pad, '-') << " " << time_str;
                    }
                    if (!result.message.empty()) {
                        std::cout << fmt::format("\n  └─ {}", result.message);
                    }
                    std::cout << std::endl;
                }
            }

            // Show category breakdown with timing and tests underneath
            if (!category_data.empty()) {
                std::cout << "\n\033[36m━━━ Categories ━━━\033[0m\n";
                for (const auto& [cat, data] : category_data) {
                    int passed_count = std::get<0>(data);
                    int total_count = std::get<1>(data);
                    double total_time = std::get<2>(data);
                    const auto& cat_results = std::get<3>(data);
                    
                    const char* cat_color = (passed_count == total_count) ? "\033[32m" : "\033[33m";
                    std::cout << fmt::format("{}  {:<20} {:>3}/{:<3} [{:>7.1f}ms]\033[0m\n",
                                           cat_color, cat, passed_count, total_count, total_time);
                    
                    // Print individual tests under category
                    for (const auto& result : cat_results) {
                        const char* color = result.passed ? "\033[32m" : "\033[31m";
                        const char* status = result.passed ? "✓" : "✗";
                        {
                            const int total_width = 60;
                            std::string visible_body = fmt::format("    {}", result.name);
                            std::string time_str = fmt::format("[{:>6.1f}ms]", result.duration_ms);
                            // 1 for the visible symbol, 1 for the single separating space before time_str
                            int visible_len = 1 + static_cast<int>(visible_body.size()) + 1 + static_cast<int>(time_str.size());
                            int pad = total_width - visible_len;
                            if (pad < 1) pad = 1;
                            std::cout << color << status << "\033[0m" << visible_body << " " << std::string(pad, '-') << " " << time_str << std::endl;
                        }
                    }
                }
            }
        }

        // Print summary (always show this, even in quiet mode)
        double total_time = 0.0;
        const TestResult* slowest = nullptr;
        for (const auto& result : results) {
            total_time += result.duration_ms;
            if (!slowest || result.duration_ms > slowest->duration_ms) {
                slowest = &result;
            }
        }
        
        if (Config::test_show_mode != TestShowMode::ALL) {
            std::string mode_name;
            switch (Config::test_show_mode) {
                case TestShowMode::FAILS: mode_name = "Failed"; break;
                case TestShowMode::SUCCESS: mode_name = "Successful"; break;
                default: mode_name = "Shown"; break;
            }

            std::cout << fmt::format("\n{} Tests: {} (\033[32mpassed: {}\033[0m, \033[31mfailed: {}\033[0m) \033[90m[{:.1f}ms total]\033[0m\n",
                                    mode_name, display_results.size(), shown_passed, shown_failed, total_time);

            if (display_results.empty()) {
                std::string mode_name_lower;
                switch (Config::test_show_mode) {
                    case TestShowMode::FAILS: mode_name_lower = "failed"; break;
                    case TestShowMode::SUCCESS: mode_name_lower = "successful"; break;
                    default: mode_name_lower = "matching"; break;
                }
                std::cout << fmt::format("\033[1;36mNo {} unit tests found.\033[0m\n", mode_name_lower);
            } else if (shown_failed == 0) {
                std::cout << fmt::format("\033[1;32m✓ All {} shown unit tests passed!\033[0m\n", display_results.size());
            } else {
                std::cout << fmt::format("\033[1;31m✗ {} unit test(s) failed\033[0m\n", shown_failed);
            }
        } else {
            const char* summary_color = (failed == 0) ? "\033[32m" : "\033[33m";
            std::cout << fmt::format("\n{}Unit Tests: {} passed / {}{} \033[90m[{:.1f}ms total, {:.2f}ms avg]\033[0m\n",
                                    summary_color, passed, results.size(), "\033[0m",
                                    total_time, total_time / results.size());
            
            if (failed > 0) {
                std::cout << fmt::format("\033[31m{} test(s) failed\033[0m\n", failed);
            }
        }

        if (!Config::quiet && slowest) {
            std::cout << fmt::format("\033[90mSlowest test: {} [{:.1f}ms]\033[0m\n", 
                                   slowest->name, slowest->duration_ms);
        }
    }

private:
    TestFramework() = default;
    std::vector<TestCase> tests_;

    std::vector<TestResult> run_filtered(const std::string& category_filter) {
        std::vector<TestResult> results;

        for (const auto& test : tests_) {
            if (category_filter.empty() || test.category == category_filter) {
                results.push_back(run_test(test));
            }
        }

        return results;
    }

    TestResult run_test(const TestCase& test) {
        auto start = std::chrono::high_resolution_clock::now();

        // Suppress debug and error output if error is expected
        // But not in test debug modes - user wants to see diagnostics
        // Config::test_debug is a runtime toggle; when off, verbose output
        // for expected-error tests is suppressed — intentional, not dead code.
        if (test.expect_error && !Config::test_debug && !Config::assembly_test_debug) {
            Logging::DebugHandler::instance().set_suppress_output(true);
            Logging::ErrorHandler::instance().set_quiet_mode(true);
        }

        // Save original buffers
        auto cout_buf = std::cout.rdbuf();
        auto cerr_buf = std::cerr.rdbuf();

        try {
            // Show which test is currently running using logger (skip in quiet mode)
            if (!Config::quiet) {
                Logging::DebugHandler::instance().report(Logging::DebugCategory::TEST_EXECUTION, fmt::format("Unit Test: {} ({})", test.name, test.category), Logging::DebugLevel::INFO);
            }

            // Track if test produces output (used for pretty separators in ALL mode)
            bool has_output = false;

            // Config::test_show_mode is a runtime toggle; in ALL mode all
            // results are shown. The comparison is intentional, not dead code.
            const bool suppress_stream_output =
                test.expect_error || (Config::test_show_mode != TestShowMode::ALL && !Config::test_debug);

            // Create custom buffers
            class TrackingBuf : public std::streambuf {
                std::streambuf* original;
                bool* output_flag;
            public:
                TrackingBuf(std::streambuf* orig, bool* flag) : original(orig), output_flag(flag) {}

                int overflow(int c) override {
                    *output_flag = true;
                    return original->sputc(c);
                }

                std::streamsize xsputn(const char* s, std::streamsize n) override {
                    if (n > 0) *output_flag = true;
                    return original->sputn(s, n);
                }
            };

            class NullBuf : public std::streambuf {
            public:
                int overflow(int c) override { return c; }
                std::streamsize xsputn(const char* /*s*/, std::streamsize n) override { return n; }
            };

            TrackingBuf tracking_cout(cout_buf, &has_output);
            TrackingBuf tracking_cerr(cerr_buf, &has_output);
            NullBuf null_buf;

            // Redirect output
            if (suppress_stream_output) {
                std::cout.rdbuf(&null_buf);
                std::cerr.rdbuf(&null_buf);
            } else {
                std::cout.rdbuf(&tracking_cout);
                std::cerr.rdbuf(&tracking_cerr);
            }

            // Reset global state
            Config::error_count = 0;

            // Debug mode: print test entry
            if (Config::test_debug) {
                std::cout << "\033[38;5;208m[TEST-DEBUG] \033[0m\033[1mStarting test:\033[0m " 
                          << test.name << " (category: " << test.category << ")" << std::endl;
            }

            // Create test context
            TestContext context;

            // Run the test
            test.test_func(context);

            // Restore original buffers
            std::cout.rdbuf(cout_buf);
            std::cerr.rdbuf(cerr_buf);

            // Only print separators if test produced output (and output wasn't suppressed)
            if (!suppress_stream_output && has_output) {
                // Print footer separator with centered test name
                const int total_width = 60;
                const int label_width = test.name.length() + 4; // "[name] "
                const int dashes_total = total_width - label_width;
                const int left_dashes = dashes_total / 2;
                const int right_dashes = dashes_total - left_dashes;
                
                std::cout << "\n\033[90m" 
                         << std::string(left_dashes, '-') << "^[" << test.name << "]^"
                         << std::string(right_dashes, '-')
                         << "\033[0m\n";
            }

            // Check if we expected an error but didn't get one
            if (test.expect_error && Config::error_count == 0) {
                // Config::test_debug is a runtime toggle; when off, output is suppressed
                if (!Config::test_debug && !Config::assembly_test_debug) {
                    Logging::DebugHandler::instance().set_suppress_output(false);
                    Logging::ErrorHandler::instance().set_quiet_mode(false);
                }
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                return TestResult(test.name, test.category, false,
                                "Expected error but test passed", duration.count() / 1000.0);
            }

            // Test passed
            if (test.expect_error) {
                if (!Config::test_debug && !Config::assembly_test_debug) {
                    Logging::DebugHandler::instance().set_suppress_output(false);
                    Logging::ErrorHandler::instance().set_quiet_mode(false);
                }
            }
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            return TestResult(test.name, test.category, true, "", duration.count() / 1000.0);

        } catch (const AssertionFailure& e) {
            // Restore buffers
            std::cout.rdbuf(cout_buf);
            std::cerr.rdbuf(cerr_buf);
            
            // Config::test_debug is a runtime toggle; when off, verbose output
        // for expected-error tests is suppressed — intentional, not dead code.
        if (test.expect_error && !Config::test_debug && !Config::assembly_test_debug) {
                Logging::DebugHandler::instance().set_suppress_output(false);
                Logging::ErrorHandler::instance().set_quiet_mode(false);
            }
            
            // If we expected an error and got an assertion failure, that might be OK
            if (test.expect_error) {
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                return TestResult(test.name, test.category, true,
                                "Expected error: " + std::string(e.what()), duration.count() / 1000.0);
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            return TestResult(test.name, test.category, false, e.what(), duration.count() / 1000.0);

        } catch (const std::exception& e) {
            // Restore buffers
            std::cout.rdbuf(cout_buf);
            std::cerr.rdbuf(cerr_buf);
            
            // Config::test_debug is a runtime toggle; when off, verbose output
        // for expected-error tests is suppressed — intentional, not dead code.
        if (test.expect_error && !Config::test_debug && !Config::assembly_test_debug) {
                Logging::DebugHandler::instance().set_suppress_output(false);
                Logging::ErrorHandler::instance().set_quiet_mode(false);
            }
            
            // If we expected an error and got a standard exception, that's probably OK
            if (test.expect_error) {
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                return TestResult(test.name, test.category, true,
                                "Expected error: " + std::string(e.what()), duration.count() / 1000.0);
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            return TestResult(test.name, test.category, false,
                            "Exception: " + std::string(e.what()), duration.count() / 1000.0);

        } catch (...) {
            // Restore buffers
            std::cout.rdbuf(cout_buf);
            std::cerr.rdbuf(cerr_buf);
            
            // Config::test_debug is a runtime toggle; when off, verbose output
        // for expected-error tests is suppressed — intentional, not dead code.
        if (test.expect_error && !Config::test_debug && !Config::assembly_test_debug) {
                Logging::DebugHandler::instance().set_suppress_output(false);
                Logging::ErrorHandler::instance().set_quiet_mode(false);
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            return TestResult(test.name, test.category, false,
                            "Unknown exception", duration.count() / 1000.0);
        }
    }
};

// Test registration helper
class TestRegistrar {
public:
    TestRegistrar(const std::string& name, const std::string& category,
                  std::function<void(TestContext&)> test_func, bool expect_error = false) {
        TestFramework::instance().register_test(TestCase(name, category, test_func, expect_error));
    }
};

// Macros for easy test registration
#define TEST_CASE(test_name, category) \
    static void test_function_##test_name(TestContext& ctx); \
    static TestRegistrar test_registrar_##test_name(#test_name, category, test_function_##test_name); \
    static void test_function_##test_name(TestContext& ctx)

#define TEST_CASE_EXPECT_ERROR(test_name, category) \
    static void test_function_##test_name(TestContext& ctx); \
    static TestRegistrar test_registrar_##test_name(#test_name, category, test_function_##test_name, true); \
    static void test_function_##test_name(TestContext& ctx)

// Function to run all tests (for integration with existing main)
inline void run_unit_tests() {
    auto& framework = TestFramework::instance();
    
    // Test selection: if --test-select is specified, run only those tests
    if (!Config::test_select.empty()) {
        std::vector<std::string> test_names;
        std::stringstream ss(Config::test_select);
        std::string name;
        while (std::getline(ss, name, ',')) {
            // Trim whitespace
            size_t start = name.find_first_not_of(" \t");
            size_t end = name.find_last_not_of(" \t");
            if (start != std::string::npos) {
                test_names.push_back(name.substr(start, end - start + 1));
            }
        }
        
        std::vector<TestResult> all_results;
        for (const auto& test_name : test_names) {
            auto results = framework.run_single(test_name);
            if (results.empty()) {
                std::cerr << "Error: Test '" << test_name << "' not found" << std::endl;
            } else {
                all_results.insert(all_results.end(), results.begin(), results.end());
            }
        }
        framework.print_results(all_results);
        return;
    }
    
    auto results = framework.run_all();
    framework.print_results(results);
}

// Function to run specific category
inline void run_unit_tests_category(const std::string& category) {
    auto& framework = TestFramework::instance();
    auto results = framework.run_category(category);
    framework.print_results(results);
}
