// DWARF 4 debug info emitter for ELF executables
// Generates minimal .debug_line, .debug_info, .debug_abbrev, .debug_str
// sufficient for gdb to set breakpoints by source line and step.

#pragma once
#include <vector>
#include <cstdint>
#include <string>

namespace Assembler {

struct DWARFSections {
    std::vector<uint8_t> debug_line;
    std::vector<uint8_t> debug_info;
    std::vector<uint8_t> debug_abbrev;
    std::vector<uint8_t> debug_str;
    std::string source_filename;
};

struct LineEntry {
    uint64_t address;   // virtual address in text section
    uint32_t line;      // source line number (1-based)
};

// Generate DWARF sections from line entries.
// source_file: name of the .asm source file
// text_vaddr: virtual address where .text section is loaded (e.g. 0x400000)
// line_entries: sorted list of (address, line) pairs
// compilation_dir: optional compilation directory
DWARFSections generate_dwarf(
    const std::string& source_file,
    uint64_t text_vaddr,
    const std::vector<LineEntry>& line_entries,
    const std::string& compilation_dir = ".");

} // namespace Assembler
