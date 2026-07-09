#include "native_compiler.hpp"
#include "ir_generator.hpp"
#include "../../codegen/disa_compiler.hpp"
#include "../../codegen/elf_emitter.hpp"
#include <iostream>
#include <fstream>

namespace DemiLanguage {

bool compile_dem_to_native(const std::string& source_path,
                           const std::string& output_path,
                           const CodegenResult& result) {
    // Compile Demi bytecode → native x86-64
    CodeGen::DISAToX86Compiler compiler;
    auto native_code = compiler.compile_program(result.bytecode, 0, nullptr, true);
    
    if (native_code.empty()) {
        std::cerr << "Error: Native compilation produced no code" << std::endl;
        return false;
    }
    
    std::cout << "Compiled to " << native_code.size() << " bytes of native x86-64 code" << std::endl;
    
    // Generate ELF executable
    CodeGen::ELFEmitter elf_emitter;
    auto elf_data = elf_emitter.generate_executable(native_code, "_start", false, source_path, nullptr);
    
    std::cout << "ELF image: " << elf_data.size() << " bytes" << std::endl;
    
    // Write to file
        std::string cmd = "chmod +x " + output_path; system(cmd.c_str());
    if (elf_emitter.write_to_file(elf_data, output_path)) {
        std::cout << "Successfully compiled to: " << output_path << std::endl;
        std::string run_path = output_path;
        if (run_path.substr(0, 2) != "./" && run_path.find('/') == std::string::npos) {
            run_path = "./" + run_path;
        }
        std::cout << "Run with: " << run_path << std::endl;
        return true;
    }
    
    std::cerr << "Error: Failed to write ELF file" << std::endl;
    return false;
}

} // namespace DemiLanguage
