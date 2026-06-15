// DWARF 4 debug info emitter — implementation

#include "dwarf_emitter.hpp"
#include <cstring>
#include <algorithm>

namespace Assembler {

// --- LEB128 encoding helpers ---

static void emit_uleb128(std::vector<uint8_t>& out, uint64_t value) {
    do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value) byte |= 0x80;
        out.push_back(byte);
    } while (value);
}

static void emit_sleb128(std::vector<uint8_t>& out, int64_t value) {
    bool more = true;
    while (more) {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if ((value == 0 && !(byte & 0x40)) || (value == -1 && (byte & 0x40)))
            more = false;
        else
            byte |= 0x80;
        out.push_back(byte);
    }
}

// --- Raw helpers ---
static void w32(std::vector<uint8_t>& v, uint32_t val) {
    v.push_back(val & 0xFF); v.push_back((val >> 8) & 0xFF);
    v.push_back((val >> 16) & 0xFF); v.push_back((val >> 24) & 0xFF);
}
static void w16(std::vector<uint8_t>& v, uint16_t val) {
    v.push_back(val & 0xFF); v.push_back((val >> 8) & 0xFF);
}
static void w64(std::vector<uint8_t>& v, uint64_t val) {
    for (int i = 0; i < 8; i++) v.push_back((val >> (i * 8)) & 0xFF);
}
static void emit_string(std::vector<uint8_t>& out, const std::string& s) {
    out.insert(out.end(), s.begin(), s.end());
    out.push_back(0);
}

// --- .debug_line generation ---
// DWARF 4 line number program with minimum instruction set.
static std::vector<uint8_t> gen_debug_line(
    const std::string& source_file,
    uint64_t text_vaddr,
    const std::vector<LineEntry>& entries)
{
    std::vector<uint8_t> line_hdr;
    std::vector<uint8_t> line_prog;

    // Line number program header fields
    uint8_t minimum_instruction_length = 1;
    uint8_t maximum_operations_per_instruction = 1;
    uint8_t default_is_stmt = 1;
    int8_t line_base = -5;
    uint8_t line_range = 14;
    uint8_t opcode_base = 13;

    // Standard opcode lengths (DWARF 4): opcodes 1-12
    uint8_t std_opcode_lengths[] = {0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1};

    // Include directories (empty — format code 0)
    line_hdr.push_back(0);

    // File names
    // file_entry_format:  DW_LNCT_path(DW_FORM_string), DW_LNCT_directory_index(DW_FORM_udata)
    // But for DWARF 4 simplicity, use traditional format:
    emit_string(line_hdr, source_file);  // file name
    emit_uleb128(line_hdr, 0);           // directory index
    emit_uleb128(line_hdr, 0);           // last modification time
    emit_uleb128(line_hdr, 0);           // file length
    line_hdr.push_back(0);               // end of file names

    // Build line program
    // DW_LNE_set_address: set current address
    // DW_LNS_set_file: set file (always 1)
    // DW_LNS_advance_line / DW_LNS_advance_pc / special opcodes

    uint64_t current_addr = text_vaddr;
    uint32_t current_line = 1;
    uint32_t current_file = 1;

    // Set initial file
    line_prog.push_back(0); // extended opcode
    emit_uleb128(line_prog, 1 + 1); // length
    line_prog.push_back(2); // DW_LNE_set_address
    w64(line_prog, text_vaddr);

    for (const auto& entry : entries) {
        if (entry.address < text_vaddr) continue;

        uint64_t addr = entry.address;
        uint32_t line = entry.line;

        if (addr < current_addr) continue; // skip backwards (shouldn't happen)

        int64_t addr_delta = static_cast<int64_t>(addr - current_addr);
        int64_t line_delta = static_cast<int32_t>(line) - static_cast<int32_t>(current_line);

        // Try special opcode: op = (line_delta - line_base) + (addr_delta * line_range) + opcode_base
        int64_t line_adj = line_delta - line_base;
        if (line_adj >= 0 && addr_delta >= 0) {
            int64_t op = line_adj + addr_delta * line_range + opcode_base;
            if (op >= opcode_base && op <= 255 && addr_delta <= (255 - opcode_base) / line_range) {
                line_prog.push_back(static_cast<uint8_t>(op));
                current_addr = addr;
                current_line = line;
                continue;
            }
        }

        // Advance address
        if (addr_delta > 0) {
            line_prog.push_back(2); // DW_LNS_advance_pc
            emit_uleb128(line_prog, static_cast<uint64_t>(addr_delta));
        }

        // Advance line
        if (line_delta != 0) {
            line_prog.push_back(3); // DW_LNS_advance_line
            emit_sleb128(line_prog, line_delta);
        }

        // Emit row
        line_prog.push_back(1); // DW_LNS_copy
        current_addr = addr;
        current_line = line;
    }

    // End of sequence
    line_prog.push_back(0); // extended opcode
    emit_uleb128(line_prog, 1);
    line_prog.push_back(1); // DW_LNE_end_sequence

    // Build final .debug_line section
    std::vector<uint8_t> out;

    // Unit header
    uint64_t unit_length_offset = out.size();
    w32(out, 0); // placeholder for unit_length
    w16(out, 4); // DWARF version 4

    // Header length placeholder
    uint64_t header_length_offset = out.size();
    w32(out, 0);

    // Header fields
    out.push_back(minimum_instruction_length);
    out.push_back(maximum_operations_per_instruction);
    out.push_back(default_is_stmt);
    out.push_back(static_cast<uint8_t>(line_base));
    out.push_back(line_range);
    out.push_back(opcode_base);
    for (int i = 0; i < opcode_base - 1; i++)
        out.push_back(std_opcode_lengths[i]);

    // Directory + file tables
    out.insert(out.end(), line_hdr.begin(), line_hdr.end());

    // Fix header_length
    uint64_t header_end = out.size();
    uint32_t header_length = static_cast<uint32_t>(header_end - header_length_offset - 4);
    out[header_length_offset] = header_length & 0xFF;
    out[header_length_offset+1] = (header_length >> 8) & 0xFF;
    out[header_length_offset+2] = (header_length >> 16) & 0xFF;
    out[header_length_offset+3] = (header_length >> 24) & 0xFF;

    // Program
    out.insert(out.end(), line_prog.begin(), line_prog.end());

    // Fix unit_length
    uint32_t unit_length = static_cast<uint32_t>(out.size() - unit_length_offset - 4);
    out[unit_length_offset] = unit_length & 0xFF;
    out[unit_length_offset+1] = (unit_length >> 8) & 0xFF;
    out[unit_length_offset+2] = (unit_length >> 16) & 0xFF;
    out[unit_length_offset+3] = (unit_length >> 24) & 0xFF;

    return out;
}

// --- .debug_abbrev generation ---
static std::vector<uint8_t> gen_debug_abbrev() {
    std::vector<uint8_t> out;

    // Abbrev 1: DW_TAG_compile_unit
    emit_uleb128(out, 1);  // abbreviation code
    emit_uleb128(out, 0x11); // DW_TAG_compile_unit
    out.push_back(0);      // DW_CHILDREN_no

    // DW_AT_producer: DW_FORM_string
    emit_uleb128(out, 0x25); // DW_AT_producer
    emit_uleb128(out, 0x08); // DW_FORM_string

    // DW_AT_language: DW_FORM_data2
    emit_uleb128(out, 0x13); // DW_AT_language
    emit_uleb128(out, 0x05); // DW_FORM_data2

    // DW_AT_name: DW_FORM_string
    emit_uleb128(out, 0x03); // DW_AT_name
    emit_uleb128(out, 0x08); // DW_FORM_string

    // DW_AT_comp_dir: DW_FORM_string
    emit_uleb128(out, 0x1B); // DW_AT_comp_dir
    emit_uleb128(out, 0x08); // DW_FORM_string

    // DW_AT_low_pc: DW_FORM_addr
    emit_uleb128(out, 0x11); // DW_AT_low_pc
    emit_uleb128(out, 0x01); // DW_FORM_addr

    // DW_AT_stmt_list: DW_FORM_sec_offset
    emit_uleb128(out, 0x10); // DW_AT_stmt_list
    emit_uleb128(out, 0x17); // DW_FORM_sec_offset

    emit_uleb128(out, 0); emit_uleb128(out, 0); // end of attributes

    // Abbrev 2: DW_TAG_subprogram
    emit_uleb128(out, 2);  // abbreviation code
    emit_uleb128(out, 0x2E); // DW_TAG_subprogram
    out.push_back(0);      // DW_CHILDREN_no

    emit_uleb128(out, 0x03); // DW_AT_name
    emit_uleb128(out, 0x08); // DW_FORM_string

    emit_uleb128(out, 0x11); // DW_AT_low_pc
    emit_uleb128(out, 0x01); // DW_FORM_addr

    emit_uleb128(out, 0x12); // DW_AT_high_pc
    emit_uleb128(out, 0x01); // DW_FORM_addr

    emit_uleb128(out, 0); emit_uleb128(out, 0); // end of attributes

    // End of abbrev table
    emit_uleb128(out, 0);

    return out;
}

// --- .debug_info generation ---
static std::vector<uint8_t> gen_debug_info(
    const std::string& source_file,
    const std::string& comp_dir,
    uint64_t text_vaddr,
    uint64_t text_size)
{
    std::vector<uint8_t> out;

    // CU header
    uint64_t unit_length_offset = out.size();
    w32(out, 0); // placeholder for unit_length
    w16(out, 4); // DWARF version 4
    w32(out, 0); // debug_abbrev offset (0 — resolved at link time via reloc... for simplicity, 0)
    out.push_back(8); // address size (8 for 64-bit)

    // DW_TAG_compile_unit (abbrev 1)
    emit_uleb128(out, 1);

    // DW_AT_producer
    emit_string(out, "DASM Assembler");

    // DW_AT_language: DW_LANG_assembler (0x8001) or DW_LANG_C89 for gdb compat
    uint16_t lang = 0x8001; // DW_LANG_Mips_Assembler — close enough
    out.push_back(lang & 0xFF);
    out.push_back((lang >> 8) & 0xFF);

    // DW_AT_name
    emit_string(out, source_file);

    // DW_AT_comp_dir
    emit_string(out, comp_dir);

    // DW_AT_low_pc
    w64(out, text_vaddr);

    // DW_AT_stmt_list (offset 0 — .debug_line starts at 0)
    w32(out, 0);

    // End of children (null DIE)
    emit_uleb128(out, 0);

    // Fix unit_length
    uint32_t unit_length = static_cast<uint32_t>(out.size() - unit_length_offset - 4);
    out[unit_length_offset] = unit_length & 0xFF;
    out[unit_length_offset+1] = (unit_length >> 8) & 0xFF;
    out[unit_length_offset+2] = (unit_length >> 16) & 0xFF;
    out[unit_length_offset+3] = (unit_length >> 24) & 0xFF;

    return out;
}

// --- Public API ---
DWARFSections generate_dwarf(
    const std::string& source_file,
    uint64_t text_vaddr,
    const std::vector<LineEntry>& line_entries,
    const std::string& compilation_dir)
{
    DWARFSections s;
    s.source_filename = source_file;
    s.debug_abbrev = gen_debug_abbrev();
    s.debug_line = gen_debug_line(source_file, text_vaddr, line_entries);
    s.debug_info = gen_debug_info(source_file, compilation_dir, text_vaddr, line_entries.empty() ? 0 : (line_entries.back().address - text_vaddr + 16));

    return s;
}

} // namespace Assembler
