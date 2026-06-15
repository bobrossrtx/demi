#include "elf_emitter.hpp"
#include "../assembler/dwarf_emitter.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <sys/stat.h>

namespace CodeGen {

// ELF64 constants
static constexpr uint8_t  ELF_MAGIC[4]     = {0x7F, 'E', 'L', 'F'};
static constexpr uint8_t  ELF_CLASS_64     = 2;
static constexpr uint8_t  ELF_DATA_2LSB    = 1;
static constexpr uint8_t  ELF_OSABI_SYSV   = 0;
static constexpr uint8_t  ELF_ABIVERSION   = 0;
static constexpr uint16_t ET_EXEC          = 2;
static constexpr uint16_t EM_X86_64        = 62;
static constexpr uint32_t EV_CURRENT       = 1;
static constexpr uint16_t ELF_EH_SIZE      = 64;
static constexpr uint16_t ELF_PH_SIZE      = 56;
static constexpr uint64_t BASE_ADDR        = 0x400000;

static constexpr uint32_t PT_LOAD   = 1;
static constexpr uint32_t PF_R      = 4;
static constexpr uint32_t PF_W      = 2;
static constexpr uint32_t PF_X      = 1;

// Write a little-endian uint16
static void le16(uint8_t* buf, uint16_t val) {
    buf[0] = static_cast<uint8_t>(val & 0xFF);
    buf[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
}

// Write a little-endian uint32
static void le32(uint8_t* buf, uint32_t val) {
    for (int i = 0; i < 4; i++) {
        buf[i] = static_cast<uint8_t>((val >> (i * 8)) & 0xFF);
    }
}

// Write a little-endian uint64
static void le64(uint8_t* buf, uint64_t val) {
    for (int i = 0; i < 8; i++) {
        buf[i] = static_cast<uint8_t>((val >> (i * 8)) & 0xFF);
    }
}

void ELFEmitter::write_ehdr(std::vector<uint8_t>& buf, uint64_t entry, uint64_t phoff) {
    buf.resize(buf.size() + ELF_EH_SIZE);
    uint8_t* e = &buf[buf.size() - ELF_EH_SIZE];

    // e_ident[16]
    // Safe: ELF_MAGIC is a compile-time constant 4-byte array; e is buffer-sized
    // to ELF_EH_SIZE (64 bytes), guaranteed larger than 4
    std::memcpy(e, ELF_MAGIC, 4);
    e[4] = ELF_CLASS_64;
    e[5] = ELF_DATA_2LSB;
    e[6] = EV_CURRENT;
    e[7] = ELF_OSABI_SYSV;
    e[8] = ELF_ABIVERSION;
    std::memset(e + 9, 0, 7); // padding

    le16(e + 16, ET_EXEC);
    le16(e + 18, EM_X86_64);
    le32(e + 20, EV_CURRENT);
    le64(e + 24, entry);
    le64(e + 32, phoff);
    le64(e + 40, 0);          // shoff = 0 (no section headers)
    le32(e + 48, 0);          // flags
    le16(e + 52, ELF_EH_SIZE);
    le16(e + 54, ELF_PH_SIZE);
    le16(e + 56, 1);          // phnum = 1
    le16(e + 58, 0);          // shentsize
    le16(e + 60, 0);          // shnum
    le16(e + 62, 0);          // shstrndx
}

void ELFEmitter::write_phdr(std::vector<uint8_t>& buf, uint32_t flags,
                            uint64_t offset, uint64_t vaddr,
                            uint64_t filesz, uint64_t memsz) {
    buf.resize(buf.size() + ELF_PH_SIZE);
    uint8_t* p = &buf[buf.size() - ELF_PH_SIZE];

    le32(p + 0,  PT_LOAD);
    le32(p + 4,  flags);
    le64(p + 8,  offset);
    le64(p + 16, vaddr);
    le64(p + 24, vaddr);      // p_paddr = p_vaddr
    le64(p + 32, filesz);
    le64(p + 40, memsz);
    le64(p + 48, 0x1000);     // alignment
}

// Build the _start runtime stub
// Stack layout after stub:
//   RSP -> [memory: 64KB]   <- RSI
//          [regfile: 1104B] <- RDI
//
// _start:
//   sub rsp, 1104            ; allocate register file  (48 81 EC 50 04 00 00)
//   sub rsp, 65536           ; allocate memory         (48 81 EC 00 00 01 00)
//   mov rsi, rsp             ; RSI = memory            (48 89 E6)
//   mov rdi, rsp             ; RDI = memory_base       (48 89 E7)
//   add rdi, 65536           ; RDI = regfile           (48 81 C7 00 00 01 00)
//   call compiled_func       ;                          (E8 XX XX XX XX)
//   xor edi, edi             ; exit(0)                 (31 FF)
//   mov eax, 60              ;                          (B8 3C 00 00 00)
//   syscall                  ;                          (0F 05)
std::vector<uint8_t> ELFEmitter::build_start_stub(size_t& stub_size_out,
                                                   size_t& call_patch_offset_out) {
    std::vector<uint8_t> stub;

    // sub rsp, 1104  (REX.W + 81 /5 + id) = 7 bytes
    // 48 81 EC 50 04 00 00
    uint8_t sub_reg1[] = {0x48, 0x81, 0xEC, 0x50, 0x04, 0x00, 0x00};
    stub.insert(stub.end(), sub_reg1, sub_reg1 + sizeof(sub_reg1));

    // sub rsp, 65536
    // 48 81 EC 00 00 01 00
    uint8_t sub_reg2[] = {0x48, 0x81, 0xEC, 0x00, 0x00, 0x01, 0x00};
    stub.insert(stub.end(), sub_reg2, sub_reg2 + sizeof(sub_reg2));

    // mov rsi, rsp  (REX.W + 89 /r) = 3 bytes
    // 48 89 E6
    uint8_t mov_rsi_rsp[] = {0x48, 0x89, 0xE6};
    stub.insert(stub.end(), mov_rsi_rsp, mov_rsi_rsp + sizeof(mov_rsi_rsp));

    // mov rdi, rsp  (REX.W + 89 /r) = 3 bytes
    // 48 89 E7
    uint8_t mov_rdi_rsp[] = {0x48, 0x89, 0xE7};
    stub.insert(stub.end(), mov_rdi_rsp, mov_rdi_rsp + sizeof(mov_rdi_rsp));

    // add rdi, 65536  (REX.W + 81 /0 + id) = 7 bytes
    // 48 81 C7 00 00 01 00
    uint8_t add_rdi[] = {0x48, 0x81, 0xC7, 0x00, 0x00, 0x01, 0x00};
    stub.insert(stub.end(), add_rdi, add_rdi + sizeof(add_rdi));

    // call rel32  (E8 + 4 byte relative offset)
    // Save the position of the call offset for patching
    call_patch_offset_out = stub.size() + 1;  // +1 to skip the E8 opcode byte
    stub.push_back(0xE8);
    stub.push_back(0x00); stub.push_back(0x00);
    stub.push_back(0x00); stub.push_back(0x00);

    // xor edi, edi  (31 FF)
    uint8_t xor_edi[] = {0x31, 0xFF};
    stub.insert(stub.end(), xor_edi, xor_edi + sizeof(xor_edi));

    // mov eax, 60  (B8 + id) = 5 bytes (32-bit mov)
    uint8_t mov_eax[] = {0xB8, 0x3C, 0x00, 0x00, 0x00};
    stub.insert(stub.end(), mov_eax, mov_eax + sizeof(mov_eax));

    // syscall  (0F 05)
    uint8_t syscall[] = {0x0F, 0x05};
    stub.insert(stub.end(), syscall, syscall + sizeof(syscall));

    stub_size_out = stub.size();
    return stub;
}

std::vector<uint8_t> ELFEmitter::generate_executable(
    const std::vector<uint8_t>& compiled_code,
    const std::string& entry_name,
    bool include_debug_info,
    const std::string& source_file,
    const std::vector<std::pair<uint64_t, uint32_t>>* line_entries)
{
    // Build the _start stub
    size_t stub_size = 0;
    size_t call_patch_offset = 0;
    std::vector<uint8_t> stub = build_start_stub(stub_size, call_patch_offset);

    // Calculate layout:
    // [ELF Header]  (64 bytes)
    // [Program Header] (56 bytes)
    // [Code segment: _start stub + compiled code]
    //
    // The PT_LOAD segment covers the entire file from offset 0
    // Entry point = BASE_ADDR + stub_offset (start of _start)
    // code_start = ELF_EH_SIZE + ELF_PH_SIZE
    // compiled_code_start = code_start + stub_size

    const uint64_t code_start = ELF_EH_SIZE + ELF_PH_SIZE;
    const uint64_t total_file_size = code_start + stub_size + compiled_code.size();

    // Entry point = BASE_ADDR + code_start (pointing to _start)
    const uint64_t entry_point = BASE_ADDR + code_start;

    // Build the ELF binary
    std::vector<uint8_t> elf;
    elf.reserve(total_file_size);

    // Write ELF header
    write_ehdr(elf, entry_point, ELF_EH_SIZE);

    // Write program header: PT_LOAD covering the entire file
    // The segment is loaded at BASE_ADDR and includes everything
    // After the compiled code returns, execution falls through
    // to the exit syscall in the stub. We need both stub and code
    // to be in the same executable segment.
    //
    // p_offset = 0 (start of file)
    // p_vaddr = BASE_ADDR
    // p_filesz = total_file_size
    // p_memsz = total_file_size
    write_phdr(elf, PF_R | PF_X, 0, BASE_ADDR, total_file_size, total_file_size);

    // Write the _start stub
    size_t stub_start_offset = elf.size();
    elf.insert(elf.end(), stub.begin(), stub.end());

    // Patch the CALL instruction in the stub to point to the compiled code
    // CALL offset = target - (call_instruction_address + 5)
    // target = code_start + stub_size + 0 (compiled code starts right after stub)
    // call_instruction_address = code_start + (call_patch_offset - 1)
    // But we're building the file from scratch, so we need absolute positions:
    // call_instruction_addr_in_file = code_start + (call_patch_offset - 1)
    // target_addr_in_file = code_start + stub_size
    // offset = target - (call_addr + 5)
    //        = (code_start + stub_size) - ((code_start + call_patch_offset - 1) + 5)
    //        = stub_size - call_patch_offset - 4

    // Wait, call_patch_offset is the offset of the 4-byte operand within the stub.
    // In the stub: call_opcode at call_patch_offset - 1, operand at call_patch_offset
    // The instruction is at: stub_start + call_patch_offset - 1 (file position)
    // The next instruction after CALL is at: stub_start + call_patch_offset + 3
    // Target (compiled code) is at: stub_start + stub_size
    // Offset = (stub_start + stub_size) - (stub_start + call_patch_offset + 3)
    int32_t call_offset = static_cast<int32_t>(stub_size) -
                          static_cast<int32_t>(call_patch_offset) - 4;

    uint8_t* call_operand = &elf[stub_start_offset + call_patch_offset];
    le32(call_operand, call_offset);

    // Append the compiled code
    elf.insert(elf.end(), compiled_code.begin(), compiled_code.end());

    // If debug info requested, append ELF section headers with DWARF stubs
    if (include_debug_info) {
        add_debug_sections(elf, code_start, stub_size, compiled_code.size(), source_file, line_entries);
        // Fix PT_LOAD filesz/memsz to cover the appended sections
        le64(&elf[ELF_EH_SIZE + 32], elf.size());  // p_filesz
        le64(&elf[ELF_EH_SIZE + 40], elf.size());  // p_memsz
    }

    return elf;
}

// Add ELF section headers and minimal DWARF debug sections
void ELFEmitter::add_debug_sections(std::vector<uint8_t>& elf,
    uint64_t code_start, size_t stub_size, size_t code_size,
    const std::string& source_file,
    const std::vector<std::pair<uint64_t, uint32_t>>* line_entries)
{
    auto push32 = [&](uint32_t v) { for(int i=0;i<4;i++) elf.push_back((v>>(i*8))&0xFF); };
    auto push64 = [&](uint64_t v) { for(int i=0;i<8;i++) elf.push_back((v>>(i*8))&0xFF); };

    // Section name strings
    const uint8_t shstr_data[] = {
        0, '.','t','e','x','t',0,
        '.','s','h','s','t','r','t','a','b',0,
        '.','s','y','m','t','a','b',0,
        '.','s','t','r','t','a','b',0,
        '.','d','e','b','u','g','_','i','n','f','o',0,
        '.','d','e','b','u','g','_','l','i','n','e',0,
        '.','d','e','b','u','g','_','a','b','b','r','e','v',0,
        '.','d','e','b','u','g','_','s','t','r',0,
    };
    std::string shstr(reinterpret_cast<const char*>(shstr_data), sizeof(shstr_data));
    uint32_t name_null=0, name_text=1, name_shstrtab=7, name_symtab=18, name_strtab=26;
    uint32_t name_debug_info=34, name_debug_line=46, name_debug_abbrev=58, name_debug_str=72;

    uint64_t text_size = stub_size + code_size;
    uint64_t text_offset = code_start;
    uint64_t shstr_offset = elf.size();
    elf.insert(elf.end(), shstr.begin(), shstr.end());

    // Generate real DWARF sections or empty stubs
    std::vector<uint8_t> debug_line_data, debug_info_data, debug_abbrev_data, debug_str_data;

    if (line_entries && !line_entries->empty()) {
        // Build LineEntry vector from the pairs
        std::vector<Assembler::LineEntry> entries;
        for (const auto& [addr, line] : *line_entries) {
            entries.push_back({BASE_ADDR + code_start + stub_size + addr, line});
        }
        auto dwarf = Assembler::generate_dwarf(
            source_file.empty() ? "<stdin>" : source_file,
            BASE_ADDR + code_start + stub_size, entries, ".");
        debug_line_data = dwarf.debug_line;
        debug_info_data = dwarf.debug_info;
        debug_abbrev_data = dwarf.debug_abbrev;
        debug_str_data = dwarf.debug_str;
    }

    // Write DWARF section data
    auto write_dwarf = [&](const std::vector<uint8_t>& data) {
        uint64_t off = elf.size();
        elf.insert(elf.end(), data.begin(), data.end());
        return off;
    };
    uint64_t debug_info_off = write_dwarf(debug_info_data);
    uint64_t debug_line_off = write_dwarf(debug_line_data);
    uint64_t debug_abbrev_off = write_dwarf(debug_abbrev_data);
    uint64_t debug_str_off = write_dwarf(debug_str_data);

    // Build minimal .strtab + .symtab so GDB can resolve "_start" by name
    std::string sym_name = "_start";
    std::vector<uint8_t> strtab = {0};
    strtab.insert(strtab.end(), sym_name.begin(), sym_name.end());
    strtab.push_back(0);

    std::vector<uint8_t> symtab;
    auto push32s = [&](uint32_t v) { for(int i=0;i<4;i++) symtab.push_back((v>>(i*8))&0xFF); };
    auto push64s = [&](uint64_t v) { for(int i=0;i<8;i++) symtab.push_back((v>>(i*8))&0xFF); };
    for(int i=0;i<24;i++) symtab.push_back(0);  // NULL entry
    push32s(1);                                   // st_name = 1 ("_start")
    symtab.push_back(0x12);                      // STB_GLOBAL | STT_FUNC
    symtab.push_back(0);
    symtab.push_back(1); symtab.push_back(0);    // st_shndx = 1
    push64s(BASE_ADDR + code_start + stub_size); // st_value (compiled code, past stub)
    push64s(text_size - stub_size);              // st_size (compiled code only)

    uint64_t symtab_off = elf.size(); elf.insert(elf.end(), symtab.begin(), symtab.end());
    uint64_t strtab_off = elf.size(); elf.insert(elf.end(), strtab.begin(), strtab.end());

    uint8_t shnum = 9;  // NULL + .text + .shstrtab + .symtab + .strtab + 4 DWARF

    // Section headers — rewrite: remove old shnum declaration
    uint64_t shdr_start = elf.size();

    le64(&elf[0x28], shdr_start);
    le16(&elf[0x3C], shnum);
    le16(&elf[0x3E], 2);  // shstrndx
    le16(&elf[0x3A], 64); // shentsize

    for(int i=0;i<64;i++) elf.push_back(0);  // NULL

    push32(name_text); push32(1); push64(6);
    push64(BASE_ADDR + code_start); push64(text_offset); push64(text_size);
    push32(0); push32(0); push64(16); push64(0);

    push32(name_shstrtab); push32(3); push64(0);
    push64(0); push64(shstr_offset); push64(shstr.size());
    push32(0); push32(0); push64(1); push64(0);

    push32(name_symtab); push32(2); push64(0);
    push64(0); push64(symtab_off); push64(symtab.size());
    push32(4); push32(1); push64(8); push64(24);

    push32(name_strtab); push32(3); push64(0);
    push64(0); push64(strtab_off); push64(strtab.size());
    push32(0); push32(0); push64(1); push64(0);

    push32(name_debug_info); push32(1); push64(0);
    push64(0); push64(debug_info_off); push64(debug_info_data.size());
    push32(0); push32(0); push64(1); push64(0);

    push32(name_debug_line); push32(1); push64(0);
    push64(0); push64(debug_line_off); push64(debug_line_data.size());
    push32(0); push32(0); push64(1); push64(0);

    push32(name_debug_abbrev); push32(1); push64(0);
    push64(0); push64(debug_abbrev_off); push64(debug_abbrev_data.size());
    push32(0); push32(0); push64(1); push64(0);

    push32(name_debug_str); push32(1); push64(0);
    push64(0); push64(debug_str_off); push64(debug_str_data.size());
    push32(0); push32(0); push64(1); push64(0);
}

bool ELFEmitter::write_to_file(const std::vector<uint8_t>& elf_data,
                                const std::string& output_path) {
    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        std::cerr << "Error: Cannot create output file: " << output_path << std::endl;
        return false;
    }
    out.write(reinterpret_cast<const char*>(elf_data.data()),
              static_cast<std::streamsize>(elf_data.size()));
    if (!out) {
        std::cerr << "Error: Failed to write ELF data to: " << output_path << std::endl;
        return false;
    }

    // Make the file executable
    if (chmod(output_path.c_str(), 0755) != 0) {
        std::cerr << "Warning: Failed to set executable permission on: " << output_path << std::endl;
    }

    return true;
}

// ============================================================
// ELF32 executable generator
// ============================================================
std::vector<uint8_t> ELFEmitter::generate_executable_32(
    const std::vector<uint8_t>& compiled_code,
    const std::string& entry_name)
{
    // 32-bit stub: allocate regfile + memory, call compiled code, exit
    // Uses int 0x80 for syscalls (32-bit ABI)
    std::vector<uint8_t> stub = {
        0x55,                               // push ebp
        0x89, 0xE5,                         // mov ebp, esp
        0x83, 0xEC, 0x20,                   // sub esp, 32 (scratch space)
        // sys_brk to allocate 64KB memory
        0xB8, 0x2D, 0x00, 0x00, 0x00,       // mov eax, 45 (brk)
        0x31, 0xDB,                         // xor ebx, ebx
        0xCD, 0x80,                         // int 0x80
        0x89, 0xC6,                         // mov esi, eax (memory base)
        0x81, 0xC3, 0x00, 0x00, 0x01, 0x00, // add ebx, 65536
        0xB8, 0x2D, 0x00, 0x00, 0x00,       // mov eax, 45
        0xCD, 0x80,                         // int 0x80
        // Allocate register file (1104 bytes)
        0x89, 0xF7,                         // mov edi, esi
        0x81, 0xC7, 0x50, 0x04, 0x00, 0x00, // add edi, 1104
        // Call compiled code
        0xE8, 0x00, 0x00, 0x00, 0x00,       // call rel32 (patched below)
        // Exit
        0xB8, 0x01, 0x00, 0x00, 0x00,       // mov eax, 1 (exit)
        0x31, 0xDB,                         // xor ebx, ebx
        0xCD, 0x80,                         // int 0x80
    };
    
    // Patch the call instruction to point to compiled_code
    size_t call_offset = 36; // byte offset of call operand in stub
    int32_t call_delta = static_cast<int32_t>(stub.size() - (call_offset + 4));
    stub[call_offset] = call_delta & 0xFF;
    stub[call_offset+1] = (call_delta >> 8) & 0xFF;
    stub[call_offset+2] = (call_delta >> 16) & 0xFF;
    stub[call_offset+3] = (call_delta >> 24) & 0xFF;
    
    const uint32_t BASE = 0x08048000;
    uint32_t entry = BASE + 0x54; // ELF header + program header
    
    std::vector<uint8_t> elf;
    
    // ELF32 header (52 bytes)
    elf.push_back(0x7F); elf.push_back('E'); elf.push_back('L'); elf.push_back('F');
    elf.push_back(1); elf.push_back(1); elf.push_back(1); elf.push_back(0); // 32-bit, LE, v1, sysv
    for(int i=0;i<8;i++) elf.push_back(0); // padding
    auto w16 = [&](uint16_t v){elf.push_back(v&0xFF);elf.push_back(v>>8);};
    auto w32 = [&](uint32_t v){for(int i=0;i<4;i++)elf.push_back((v>>(i*8))&0xFF);};
    w16(2); w16(3); w32(1); // ET_EXEC, EM_386, version
    w32(entry); // entry
    w32(52); w32(0); // phoff=52, shoff=0
    w32(0); w16(52); w16(32); w16(1); w16(0); w16(0);
    
    // Program header (32 bytes)
    w32(1); w32(0); // PT_LOAD, offset=0
    w32(BASE); w32(BASE); // vaddr, paddr
    uint32_t total = 52 + 32 + stub.size() + compiled_code.size();
    w32(total); w32(total); // filesz, memsz
    w32(7); w32(0x1000); // flags=RWX, align
    
    // Stub + compiled code
    elf.insert(elf.end(), stub.begin(), stub.end());
    elf.insert(elf.end(), compiled_code.begin(), compiled_code.end());
    
    return elf;
}

} // namespace CodeGen
