#pragma once
#include "assembler.hpp"
#include "ir.hpp"
#include "preprocessor.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace Assembler {

/**
 * Main assembler interface that combines lexing, parsing, and code generation
 */
class DemiAssembler {
public:
    DemiAssembler() = default;
    
    /**
     * Assemble assembly source code into bytecode
     * @param source Assembly source code string
     * @return Bytecode vector (empty if errors occurred)
     */
    std::vector<uint8_t> assemble_string(const std::string& source);
    
    /**
     * Assemble assembly source code into bytecode with base path for includes
     * @param source Assembly source code string
     * @param base_path Base directory path for resolving relative includes
     * @return Bytecode vector (empty if errors occurred)
     */
    std::vector<uint8_t> assemble_string(const std::string& source, const std::string& base_path);
    
    /**
     * Assemble assembly source file into bytecode
     * @param filename Path to assembly source file
     * @return Bytecode vector (empty if errors occurred)
     */
    std::vector<uint8_t> assemble_file(const std::string& filename);
    
    /**
     * Get all errors from the last assembly operation
     */
    const std::vector<std::string>& get_errors() const { return all_errors; }
    
    /**
     * Check if the last assembly operation had errors
     */
    bool has_errors() const { return !all_errors.empty(); }
    
    /**
     * Get symbol table from the last assembly operation
     */
    const std::unordered_map<std::string, Symbol>& get_symbols() const { return symbols; }
    
    /**
     * Get entry address from the last assembly operation
     */
    uint32_t get_entry_address() const { return entry_address; }
    
    /**
     * Get memory size set by .memory directive (0 if not set)
     */
    size_t get_memory_size() const { return memory_size; }
    
    /**
     * Set entry point symbol (default: "_start")
     * @param symbol Entry point symbol name
     */
    void set_entry_point_symbol(const std::string& symbol) { entry_point_symbol = symbol; }
    void set_target(AssemblyTarget target) { target_ = target; }
    
    /**
     * Clear all errors and reset state
     */
    void clear_errors();
    
    /**
     * Validate and print in-assembly tests from a source file
     * @param filename Path to assembly source file containing tests
     * @return Number of valid tests found
     */
    int validate_and_print_tests(const std::string& filename);

private:
    std::vector<std::string> all_errors;
    std::unordered_map<std::string, Symbol> symbols;
    uint32_t entry_address = 0;
    size_t memory_size = 0; // Memory size set by .memory directive
    std::string entry_point_symbol = "_start"; // Entry point symbol name
    AssemblyTarget target_ = AssemblyTarget::DemiBytecode;
    
    void collect_errors(const std::vector<std::string>& errors);
    std::string read_file(const std::string& filename);
    std::vector<uint8_t> assemble_internal(const std::string& source, const std::string& base_path);
};

/**
 * Convenience function to assemble a string of assembly code
 * @param source Assembly source code
 * @return Bytecode vector (empty if errors occurred)
 */
std::vector<uint8_t> assemble(const std::string& source);

/**
 * Convenience function to assemble an assembly file
 * @param filename Path to assembly source file
 * @return Bytecode vector (empty if errors occurred)
 */
std::vector<uint8_t> assemble_file(const std::string& filename);

} // namespace Assembler
