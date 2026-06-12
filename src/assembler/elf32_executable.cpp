#include "elf32_executable.hpp"
#include <cstring>
#include <unordered_map>

namespace Assembler {

static void w32(std::vector<uint8_t>& v, size_t off, uint32_t val) {
    v[off] = static_cast<uint8_t>(val & 0xFF);
    v[off+1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    v[off+2] = static_cast<uint8_t>((val >> 16) & 0xFF);
    v[off+3] = static_cast<uint8_t>((val >> 24) & 0xFF);
}
static uint32_t r32(const std::vector<uint8_t>& v, size_t off) {
    return (uint32_t)v[off] | ((uint32_t)v[off+1] << 8) | ((uint32_t)v[off+2] << 16) | ((uint32_t)v[off+3] << 24);
}
static uint32_t align_up(uint32_t addr, uint32_t a) { return (addr + a - 1) & ~(a - 1); }
static uint16_t r16(const std::vector<uint8_t>& v, size_t off) {
    return (uint16_t)v[off] | ((uint16_t)v[off+1] << 8);
}

// Convert relocatable ELF32 .o to standalone executable
std::vector<uint8_t> make_elf32_executable(
    const std::vector<uint8_t>& rel,
    const IRProgram& program,
    std::vector<std::string>& errors) {

    if (rel.size() < 52) { errors.push_back("ELF too small"); return {}; }
    if (std::memcmp(rel.data(), "\x7f""ELF", 4) != 0) { errors.push_back("not ELF"); return {}; }

    // Find entry point
    uint64_t entry_off = 0;
    for (auto& sym : program.symbols)
        if (sym.name == "_start" && sym.defined && sym.section == IRSectionKind::Text)
            { entry_off = sym.offset; break; }

    // Parse original ELF to find section indices and content
    uint32_t shoff = r32(rel, 32);
    uint16_t shnum = r16(rel, 48);
    uint16_t shstrndx = r16(rel, 50);
    uint32_t strtab_off = r32(rel, shoff + shstrndx * 40 + 16);

    uint32_t text_idx = 0, data_idx = 0, rodata_idx = 0, bss_idx = 0;
    uint32_t text_off = 0, text_sz = 0, data_off = 0, data_sz = 0;
    uint32_t rodata_off = 0, rodata_sz = 0, bss_sz = 0;
    uint32_t symtab_off = 0, symtab_sz = 0, strtab_ofs = 0, strtab_sz = 0;
    uint32_t reltext_off = 0, reltext_sz = 0;

    for (uint16_t i = 0; i < shnum; i++) {
        uint32_t base = shoff + i * 40;
        uint32_t name_off = r32(rel, base);
        uint32_t sh_type = r32(rel, base + 4);
        uint32_t sh_sz = r32(rel, base + 20);
        uint32_t sh_off = r32(rel, base + 16);

        std::string name;
        for (size_t j = strtab_off + name_off; j < rel.size() && rel[j]; j++)
            name += (char)rel[j];

        if (name == ".text") { text_idx = i; text_off = sh_off; text_sz = sh_sz; }
        else if (name == ".data") { data_idx = i; data_off = sh_off; data_sz = sh_sz; }
        else if (name == ".rodata") { rodata_idx = i; rodata_off = sh_off; rodata_sz = sh_sz; }
        else if (name == ".bss") { bss_idx = i; bss_sz = sh_sz; }
        else if (name == ".symtab") { symtab_off = sh_off; symtab_sz = sh_sz; }
        else if (name == ".strtab") { strtab_ofs = sh_off; strtab_sz = sh_sz; }
        else if (name == ".rel.text") { reltext_off = sh_off; reltext_sz = sh_sz; }
    }

    // Extract section data
    auto get_sec = [&](uint32_t off, uint32_t sz) {
        if (off == 0 || sz == 0) return std::vector<uint8_t>();
        return std::vector<uint8_t>(rel.begin() + off, rel.begin() + off + sz);
    };
    std::vector<uint8_t> text_sec = get_sec(text_off, text_sz);
    std::vector<uint8_t> data_sec = get_sec(data_off, data_sz);
    std::vector<uint8_t> rodata_sec = get_sec(rodata_off, rodata_sz);
    std::vector<uint8_t> symtab = get_sec(symtab_off, symtab_sz);
    std::vector<uint8_t> strtab = get_sec(strtab_ofs, strtab_sz);
    std::vector<uint8_t> reltext = get_sec(reltext_off, reltext_sz);

    // Clean padding bytes from text section tail
    while (!text_sec.empty() && text_sec.back() == 0xCC)
        text_sec.pop_back();

    // Virtual address layout
    const uint32_t PAGE = 0x1000;
    const uint32_t BASE = 0x08048000;
    uint32_t text_vaddr = BASE;
    uint32_t rodata_vaddr = rodata_sec.empty() ? text_vaddr : BASE + text_sz;  // simplified
    uint32_t data_vaddr = BASE + 0x1000;  // simplified: next page
    uint32_t entry = text_vaddr + static_cast<uint32_t>(entry_off);

    // Build symbol vaddr map from program symbols
    std::unordered_map<std::string, uint32_t> sym_vaddrs;
    for (auto& sym : program.symbols) {
        if (!sym.defined) continue;
        uint32_t base = 0;
        switch (sym.section) {
            case IRSectionKind::Text: base = text_vaddr; break;
            case IRSectionKind::Data: base = data_vaddr; break;
            case IRSectionKind::Rodata: base = rodata_vaddr; break;
            default: continue;
        }
        sym_vaddrs[sym.name] = base + static_cast<uint32_t>(sym.offset);
    }

    // Resolve relocations using .rel.text from the relocatable
    for (uint32_t r = 0; r + 8 <= reltext.size(); r += 8) {
        uint32_t r_offset = r32(reltext, r);
        uint32_t r_info = r32(reltext, r + 4);
        uint32_t sym_idx = r_info >> 8;
        uint32_t rel_type = r_info & 0xFF;
        if (r_offset >= text_sec.size()) continue;

        // Get symbol name from symtab
        if (sym_idx > 0 && (sym_idx + 1) * 16 <= symtab.size()) {
            uint32_t s_off = sym_idx * 16;
            uint32_t s_name = r32(symtab, s_off);
            uint32_t s_value = r32(symtab, s_off + 4);
            std::string sname;
            for (size_t j = s_name; j < strtab.size() && strtab[j]; j++)
                sname += (char)strtab[j];

            auto it = sym_vaddrs.find(sname);
            uint32_t S = (it != sym_vaddrs.end()) ? it->second : s_value;

            if (rel_type == 1) { // R_386_32
                int32_t val = static_cast<int32_t>(S);
                w32(text_sec, r_offset, static_cast<uint32_t>(val));
            } else if (rel_type == 2) { // R_386_PC32
                uint32_t P = text_vaddr + r_offset;
                int32_t val = static_cast<int32_t>(S - P);
                w32(text_sec, r_offset, static_cast<uint32_t>(val));
            }
        }
    }

    // Build minimal section name list
    struct SecInfo { uint32_t idx; std::string name; uint32_t vaddr; };
    std::vector<SecInfo> sec_map = {
        {1, ".text", text_vaddr},
        {2, ".rodata", rodata_vaddr},
        {3, ".data", data_vaddr}
    };

    // Build new shstrtab with section names
    std::vector<uint8_t> shstrtab; shstrtab.push_back(0);
    auto add_name = [&](const std::string& n) -> uint32_t {
        uint32_t pos = static_cast<uint32_t>(shstrtab.size());
        shstrtab.insert(shstrtab.end(), n.begin(), n.end());
        shstrtab.push_back(0);
        return pos;
    };

    std::vector<uint32_t> name_offs;
    name_offs.push_back(add_name(""));    // NULL
    name_offs.push_back(add_name(".text"));
    name_offs.push_back(add_name(".rodata"));
    name_offs.push_back(add_name(".data"));
    name_offs.push_back(add_name(".bss"));
    name_offs.push_back(add_name(".comment"));
    name_offs.push_back(add_name(".note.GNU-stack"));
    name_offs.push_back(add_name(".symtab"));
    name_offs.push_back(add_name(".strtab"));
    name_offs.push_back(add_name(".shstrtab"));

    // Build symbol table with resolved addresses (add_sym unused, kept for reference)
    std::vector<uint8_t> new_strtab; new_strtab.push_back(0);
    std::vector<uint8_t> new_symtab;
    new_symtab.insert(new_symtab.end(), 16, 0);

    struct SymEnt { std::string name; uint32_t val, sz; uint8_t info; uint16_t shndx; };
    std::vector<SymEnt> syms;
    for (auto& sym : program.symbols) {
        if (!sym.defined) continue;
        uint32_t base = 0; uint16_t shn = 0;
        switch (sym.section) {
            case IRSectionKind::Text: base = text_vaddr; shn = 1; break;
            case IRSectionKind::Data: base = data_vaddr; shn = 3; break;
            case IRSectionKind::Rodata: base = rodata_vaddr; shn = 2; break;
            case IRSectionKind::Bss: shn = 4; break;
            default: break;
        }
        uint8_t bind = (sym.binding == IRSymbolBinding::Global) ? 1 : 0;
        uint8_t type = sym.is_function ? 2 : 1;
        syms.push_back({sym.name, base + static_cast<uint32_t>(sym.offset),
                        static_cast<uint32_t>(sym.size),
                        static_cast<uint8_t>((bind << 4) | type), shn});
    }

    auto push32 = [](std::vector<uint8_t>& v, uint32_t val) {
        v.push_back(val & 0xFF); v.push_back((val>>8)&0xFF);
        v.push_back((val>>16)&0xFF); v.push_back((val>>24)&0xFF);
    };
    auto push16 = [](std::vector<uint8_t>& v, uint16_t val) {
        v.push_back(val & 0xFF); v.push_back((val>>8)&0xFF);
    };

    new_symtab.clear(); new_symtab.insert(new_symtab.end(), 16, 0);
    for (auto& s : syms) {
        uint32_t nm_off = static_cast<uint32_t>(new_strtab.size());
        new_strtab.insert(new_strtab.end(), s.name.begin(), s.name.end());
        new_strtab.push_back(0);
        push32(new_symtab, nm_off);
        push32(new_symtab, s.val);
        push32(new_symtab, s.sz);
        new_symtab.push_back(s.info); new_symtab.push_back(0);
        push16(new_symtab, s.shndx);
    }

    // Comment string
    std::string cstr = "DASM x86 assembler (DemiEngine v1.0)";
    std::vector<uint8_t> cdata(cstr.begin(), cstr.end()); cdata.push_back(0);

    // BSS size
    uint64_t bss_size = 0;
    for (auto& rec : program.data_records)
        if (rec.section == IRSectionKind::Bss && rec.directive == ".resb" && !rec.values.empty())
            bss_size += static_cast<uint64_t>(std::max<int64_t>(0, std::get<IRImmediateOperand>(rec.values[0].value).value));

    // Layout
    const size_t EHDR = 52, PHDR = 32;
    uint16_t phnum = 1; // may increase to 2

    // File layout: EHDR + PHDR + text + data + rodata + comment + symtab + strtab + shstrtab + section headers
    uint32_t foff = EHDR + phnum * PHDR;

    struct FileSec { std::vector<uint8_t> data; uint32_t off, sz, vaddr, flags; };
    std::vector<FileSec> fsecs;

    FileSec ftx; ftx.data = text_sec; ftx.vaddr = text_vaddr; ftx.flags = 5;
    FileSec fdt; fdt.data = data_sec; fdt.vaddr = data_vaddr; fdt.flags = 6;
    FileSec fro; fro.data = rodata_sec; fro.vaddr = rodata_vaddr; fro.flags = 4;
    FileSec fcm; fcm.data = cdata; fcm.vaddr = 0; fcm.flags = 0;
    FileSec fsm; fsm.data = new_symtab; fsm.vaddr = 0; fsm.flags = 0;
    FileSec fst; fst.data = new_strtab; fst.vaddr = 0; fst.flags = 0;
    FileSec fsh; fsh.data = shstrtab; fsh.vaddr = 0; fsh.flags = 0;

    // Offsets assigned below after page alignment computation

    // Build ELF
    // Pad to page alignment for proper mmap
    uint32_t page_pad = PAGE - (EHDR + phnum * PHDR) % PAGE;
    if (page_pad == PAGE) page_pad = 0;
    uint32_t exe_foff = EHDR + phnum * PHDR + page_pad;

    // Reassign section offsets from page-aligned base
    ftx.sz = static_cast<uint32_t>(ftx.data.size());
    fdt.sz = static_cast<uint32_t>(fdt.data.size());
    fro.sz = static_cast<uint32_t>(fro.data.size());
    fcm.sz = static_cast<uint32_t>(fcm.data.size());
    fsm.sz = static_cast<uint32_t>(fsm.data.size());
    fst.sz = static_cast<uint32_t>(fst.data.size());
    fsh.sz = static_cast<uint32_t>(fsh.data.size());

    foff = exe_foff;
    ftx.off = foff; foff += ftx.sz;
    fdt.off = foff; foff += fdt.sz;
    fro.off = foff; foff += fro.sz;
    fcm.off = foff; foff += fcm.sz;
    fsm.off = foff; foff += fsm.sz;
    fst.off = foff; foff += fst.sz;
    fsh.off = foff; foff += fsh.sz;

    std::vector<uint8_t> elf;
    elf.resize(exe_foff, 0);

    std::memcpy(elf.data(), "\x7f""ELF", 4);
    elf[4]=1; elf[5]=1; elf[6]=1;
    elf[16]=2; elf[17]=0; // ET_EXEC
    elf[18]=3; elf[19]=0; // EM_386
    w32(elf, 20, 1);
    w32(elf, 24, entry);
    w32(elf, 28, EHDR);
    w32(elf, 32, 0); // shoff placeholder
    w32(elf, 36, 0);
    w32(elf, 40, EHDR); w32(elf, 42, PHDR); w32(elf, 44, phnum);
    w32(elf, 46, 40); w32(elf, 48, 10); w32(elf, 50, 9); // shstrndx=9

    // Program headers: two LOAD segments
    // LOAD 0: .text + .rodata (R|X)
    uint32_t load0_off = exe_foff;
    uint32_t load0_vaddr = text_vaddr;
    uint32_t load0_fsize = ftx.sz + fro.sz;
    uint32_t load0_msize = align_up(load0_fsize, PAGE);
    w32(elf, EHDR, 1); w32(elf, EHDR+4, load0_off); w32(elf, EHDR+8, load0_vaddr);
    w32(elf, EHDR+12, load0_vaddr); w32(elf, EHDR+16, load0_fsize); w32(elf, EHDR+20, load0_msize);
    w32(elf, EHDR+24, 5); w32(elf, EHDR+28, PAGE);

    // LOAD 1: .data + .bss (R|W) — page-aligned separately
    uint32_t load1_vaddr = data_vaddr;
    uint32_t load1_off = align_up(load0_off + load0_fsize, PAGE);
    // Pad the file to the page-aligned data offset
    while (elf.size() < load1_off) elf.push_back(0);
    // Recalculate data section file offset
    uint32_t data_file_off = load1_off;
    fdt.off = data_file_off;
    fcm.off = data_file_off + fdt.sz;
    // Recalculate remaining offsets
    uint32_t tail_off = fcm.off + fcm.sz;
    fsm.off = tail_off; tail_off += fsm.sz;
    fst.off = tail_off; tail_off += fst.sz;
    fsh.off = tail_off; tail_off += fsh.sz;

    uint32_t load1_fsize = fdt.sz;
    uint32_t load1_msize = align_up(load1_fsize + static_cast<uint32_t>(bss_size), PAGE);
    w32(elf, EHDR+32, 1); w32(elf, EHDR+36, load1_off);
    w32(elf, EHDR+40, load1_vaddr); w32(elf, EHDR+44, load1_vaddr);
    w32(elf, EHDR+48, load1_fsize); w32(elf, EHDR+52, load1_msize);
    w32(elf, EHDR+56, 6); w32(elf, EHDR+60, PAGE);

    // Update phnum
    w32(elf, 44, 2);
    phnum = 2;

    // Append section data at correct file offsets
    auto write_at = [&](std::vector<uint8_t>& dst, uint32_t off, const std::vector<uint8_t>& src) {
        if (dst.size() < off + src.size()) dst.resize(off + src.size(), 0);
        std::memcpy(dst.data() + off, src.data(), src.size());
    };

    write_at(elf, ftx.off, ftx.data);
    write_at(elf, fdt.off, fdt.data);
    write_at(elf, fcm.off, fcm.data);

    uint32_t symtab_off_exe = fsm.off;
    write_at(elf, fsm.off, fsm.data);
    uint32_t strtab_off_exe = fst.off;
    write_at(elf, fst.off, fst.data);
    uint32_t shstr_off_exe = fsh.off;
    write_at(elf, fsh.off, fsh.data);

    // Section headers
    uint32_t shdr_start = static_cast<uint32_t>(elf.size());
    w32(elf, 32, shdr_start); // shoff

    auto push_shdr = [&](uint32_t name_off, uint32_t type, uint32_t flags, uint32_t addr,
                           uint32_t off, uint32_t size, uint32_t link, uint32_t info,
                           uint32_t align, uint32_t entsize) {
        push32(elf, name_off); push32(elf, type); push32(elf, flags);
        push32(elf, addr); push32(elf, off); push32(elf, size);
        push32(elf, link); push32(elf, info); push32(elf, align); push32(elf, entsize);
    };

    // [0] NULL
    push_shdr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    // [1] .text
    push_shdr(name_offs[1], 1, 6, text_vaddr, ftx.off, ftx.sz, 0, 0, 16, 0);
    // [2] .rodata
    push_shdr(name_offs[2], 1, 2, rodata_vaddr, fro.off, fro.sz, 0, 0, 4, 0);
    // [3] .data
    push_shdr(name_offs[3], 1, 3, data_vaddr, fdt.off, fdt.sz, 0, 0, 4, 0);
    // [4] .bss
    push_shdr(name_offs[4], 8, 3, data_vaddr + data_sz, 0, static_cast<uint32_t>(bss_size), 0, 0, 4, 0);
    // [5] .comment
    push_shdr(name_offs[5], 1, 0, 0, fcm.off, fcm.sz, 0, 0, 1, 0);
    // [6] .note.GNU-stack
    push_shdr(name_offs[6], 7, 0, 0, 0, 0, 0, 0, 1, 0);
    // [7] .symtab
    push_shdr(name_offs[7], 2, 0, 0, symtab_off_exe, static_cast<uint32_t>(new_symtab.size()), 8, 1, 4, 16);
    // [8] .strtab
    push_shdr(name_offs[8], 3, 0, 0, strtab_off_exe, static_cast<uint32_t>(new_strtab.size()), 0, 0, 1, 0);
    // [9] .shstrtab
    push_shdr(name_offs[9], 3, 0, 0, shstr_off_exe, static_cast<uint32_t>(shstrtab.size()), 0, 0, 1, 0);

    return elf;
}

} // namespace Assembler
