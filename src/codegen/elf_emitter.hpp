#pragma once
#include <vector>
#include <cstdint>
#include <string>

namespace CodeGen {

// Generates ELF64 executable binaries from compiled x86-64 machine code.
// Produces standalone Linux ELF executables with a _start stub that
// sets up the VM register file and memory, calls the compiled code, and exits.
class ELFEmitter {
public:
    ELFEmitter() = default;

    // Generate a complete ELF64 executable binary
    // compiled_code: native x86-64 machine code (from DISAToX86Compiler)
    // entry_name: entry point symbol name (default: _start)
    // Returns the complete ELF binary data ready to write to a file
    std::vector<uint8_t> generate_executable(
        const std::vector<uint8_t>& compiled_code,
        const std::string& entry_name = "_start",
        bool include_debug_info = false,
        const std::string& source_file = "",
        const std::vector<std::pair<uint64_t, uint32_t>>* line_entries = nullptr);

    // Write the generated ELF to a file
    bool write_to_file(const std::vector<uint8_t>& elf_data,
                       const std::string& output_path);

private:
    // Build the _start runtime stub that sets up VM state and calls compiled code
    // Returns the stub machine code bytes
    // stub_size_out: receives the stub size in bytes
    // call_offset_out: receives the offset within the stub where the CALL instruction
    //   operand (4 bytes) should be written
    std::vector<uint8_t> build_start_stub(size_t& stub_size_out,
                                          size_t& call_patch_offset_out);

    // ELF64 structure helpers
    void write_ehdr(std::vector<uint8_t>& buf, uint64_t entry, uint64_t phoff);
    void write_phdr(std::vector<uint8_t>& buf, uint32_t flags,
                    uint64_t offset, uint64_t vaddr,
                    uint64_t filesz, uint64_t memsz);

    // Add ELF section headers with DWARF debug info stubs
    void add_debug_sections(std::vector<uint8_t>& elf,
        uint64_t code_start, size_t stub_size, size_t code_size,
        const std::string& source_file,
        const std::vector<std::pair<uint64_t, uint32_t>>* line_entries);
};

} // namespace CodeGen
