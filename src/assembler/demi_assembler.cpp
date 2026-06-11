#include "demi_assembler.hpp"
#include "backend.hpp"
#include "../config.hpp"
#include "lowering.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "preprocessor.hpp"
#include "x86_backend.hpp"
#include "../test/in_assembly_test_validator.hpp"
#include "../debug/debug_handler.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace Assembler {

std::vector<uint8_t> DemiAssembler::assemble_string(const std::string& source) {
    return assemble_internal(source, "");
}

std::vector<uint8_t> DemiAssembler::assemble_string(const std::string& source, const std::string& base_path) {
    return assemble_internal(source, base_path);
}

std::vector<uint8_t> DemiAssembler::assemble_file(const std::string& filename) {
    std::string source = read_file(filename);
    if (source.empty() && has_errors()) {
        return {};
    }
    std::string base_path = std::filesystem::path(filename).parent_path().string();
    return assemble_internal(source, base_path);
}

std::vector<uint8_t> DemiAssembler::assemble_internal(const std::string& source, const std::string& base_path) {
    clear_errors();

    // Preprocessing
    Preprocessor preprocessor;
    std::string preprocessed_source;
    if (base_path.empty()) {
        preprocessed_source = preprocessor.preprocess(source);
    } else {
        preprocessed_source = preprocessor.preprocess(source, base_path);
    }

    if (preprocessor.has_errors()) {
        collect_errors(preprocessor.get_errors());
        return {};
    }

    // Lexical analysis
    Lexer lexer(preprocessed_source);
    auto tokens = lexer.tokenize();

    if (lexer.has_errors()) {
        collect_errors(lexer.get_errors());
        return {};
    }

    // Parsing
    Parser parser(tokens);
    auto program = parser.parse();

    if (parser.has_errors()) {
        collect_errors(parser.get_errors());
        return {};
    }

    if (target_ == AssemblyTarget::DemiBytecode) {
        // Code generation
        AssemblerEngine assembler;
        assembler.set_entry_point_symbol(entry_point_symbol);
        if (Config::architecture == Architecture::AUTO) {
            Architecture detected = assembler.detect_architecture(*program);
            Config::architecture = detected;
        }
        auto bytecode = assembler.assemble(*program);

        if (assembler.has_errors()) {
            collect_errors(assembler.get_errors());
            return {};
        }

        symbols = assembler.get_symbols();
        entry_address = assembler.get_entry_address();
        memory_size = assembler.get_memory_size();

        return bytecode;
    }

    auto ir_program = lower_program(*program);
    ir_program.target = (target_ == AssemblyTarget::X86Elf32) ? IRTarget::X86Elf32 : IRTarget::X86Elf64;
    ir_program.entry_symbol = entry_point_symbol;

    std::unique_ptr<AssemblerBackend> backend;
    if (target_ == AssemblyTarget::X86Elf32) {
        backend = std::make_unique<X86Backend>(X86BackendMode::X86_32);
    } else {
        backend = std::make_unique<X86Backend>(X86BackendMode::X86_64);
    }

    auto artifact = backend->emit(ir_program);
    if (!artifact.ok()) {
        collect_errors(artifact.errors);
        return {};
    }

    entry_address = 0;
    memory_size = 0;
    return artifact.bytes;
}

void DemiAssembler::clear_errors() {
    all_errors.clear();
    symbols.clear();
    memory_size = 0;
}

void DemiAssembler::collect_errors(const std::vector<std::string>& errors) {
    all_errors.insert(all_errors.end(), errors.begin(), errors.end());
}

std::string DemiAssembler::read_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        all_errors.push_back("Cannot open file: " + filename);
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int DemiAssembler::validate_and_print_tests(const std::string& filename) {
    clear_errors();
    
    std::string source = read_file(filename);
    if (source.empty()) {
        return 0;
    }
    
    // Lexical analysis
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    
    if (lexer.has_errors()) {
        collect_errors(lexer.get_errors());
        std::cerr << "Lexer errors while parsing tests:\n";
        for (const auto& error : lexer.get_errors()) {
            std::cerr << "  " << error << "\n";
        }
        return 0;
    }
    
    // Parsing
    Parser parser(tokens);
    auto program = parser.parse();
    
    if (parser.has_errors()) {
        collect_errors(parser.get_errors());
        std::cerr << "Parser errors while parsing tests:\n";
        for (const auto& error : parser.get_errors()) {
            std::cerr << "  " << error << "\n";
        }
        return 0;
    }
    
    // Validate tests
    auto validation_results = InAssemblyTestValidator::validate_tests(*program);
    
    // Print results
    InAssemblyTestValidator::print_validation_results(validation_results);
    
    // Count valid tests
    int valid_count = 0;
    for (const auto& result : validation_results) {
        if (result.valid) {
            valid_count++;
        }
    }
    
    return valid_count;
}

// Convenience functions
std::vector<uint8_t> assemble(const std::string& source) {
    DemiAssembler assembler;
    auto bytecode = assembler.assemble_string(source);

    if (assembler.has_errors()) {
        std::cerr << "Assembly errors:\n";
        for (const auto& error : assembler.get_errors()) {
            std::cerr << "  " << error << "\n";
        }
    }

    return bytecode;
}

std::vector<uint8_t> assemble_file(const std::string& filename) {
    DemiAssembler assembler;
    auto bytecode = assembler.assemble_file(filename);

    if (assembler.has_errors()) {
        std::cerr << "Assembly errors:\n";
        for (const auto& error : assembler.get_errors()) {
            std::cerr << "  " << error << "\n";
        }
    }

    return bytecode;
}

} // namespace Assembler
