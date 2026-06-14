// ELF64 executable converter — converts relocatable .o to standalone executable

#include "elf32_executable.hpp"   // reuse IRProgram
#include "dwarf_emitter.hpp"
#include <cstring>
#include <unordered_map>

namespace Assembler {

static void w64(std::vector<uint8_t>& v, size_t off, uint64_t val) {
    for (int i = 0; i < 8; i++) v[off + i] = static_cast<uint8_t>((val >> (i * 8)) & 0xFF);
}
static void w32(std::vector<uint8_t>& v, size_t off, uint32_t val) {
    for (int i = 0; i < 4; i++) v[off + i] = static_cast<uint8_t>((val >> (i * 8)) & 0xFF);
}
static void w16(std::vector<uint8_t>& v, size_t off, uint16_t val) {
    v[off] = static_cast<uint8_t>(val & 0xFF);
    v[off+1] = static_cast<uint8_t>((val >> 8) & 0xFF);
}
static uint64_t r64(const std::vector<uint8_t>& v, size_t off) {
    uint64_t r = 0;
    for (int i = 0; i < 8; i++) r |= static_cast<uint64_t>(v[off + i]) << (i * 8);
    return r;
}
static uint32_t r32(const std::vector<uint8_t>& v, size_t off) {
    uint32_t r = 0;
    for (int i = 0; i < 4; i++) r |= static_cast<uint32_t>(v[off + i]) << (i * 8);
    return r;
}
static uint16_t r16(const std::vector<uint8_t>& v, size_t off) {
    return (uint16_t)v[off] | ((uint16_t)v[off+1] << 8);
}
static uint64_t align_up(uint64_t addr, uint64_t a) { return (addr + a - 1) & ~(a - 1); }

std::vector<uint8_t> make_elf64_executable(
    const std::vector<uint8_t>& rel,
    const IRProgram& program,
    std::vector<std::string>& errors,
    const std::vector<LineEntry>* line_entries,
    const std::string& source_file) {

    if (rel.size() < 64) { errors.push_back("ELF too small"); return {}; }
    if (std::memcmp(rel.data(), "\x7f""ELF", 4) != 0) { errors.push_back("not ELF"); return {}; }
    if (rel[4] != 2) { errors.push_back("not ELF64"); return {}; }

    uint64_t entry_off = 0;
    for (auto& sym : program.symbols)
        if (sym.name == "_start" && sym.defined && sym.section == IRSectionKind::Text)
            { entry_off = sym.offset; break; }

    // Parse ELF64 section headers (64 bytes each)
    uint64_t shoff = r64(rel, 40);
    uint16_t shnum = r16(rel, 60);
    uint16_t shstrndx = r16(rel, 62);
    uint64_t strtab_off = r64(rel, shoff + shstrndx * 64 + 24);

    uint32_t text_idx = 0, data_idx = 0, rodata_idx = 0, bss_idx = 0;
    uint64_t text_off = 0, text_sz = 0, data_off = 0, data_sz = 0;
    uint64_t rodata_off = 0, rodata_sz = 0, bss_sz = 0;
    uint64_t symtab_off = 0, symtab_sz = 0, strtab_ofs = 0, strtab_sz = 0;
    uint64_t rela_text_off = 0, rela_text_sz = 0;

    for (uint16_t i = 0; i < shnum; i++) {
        uint64_t base = shoff + i * 64;
        uint32_t name_off = r32(rel, base);
        uint32_t sh_type = r32(rel, base + 4);
        uint64_t sh_sz = r64(rel, base + 32);
        uint64_t sh_off = r64(rel, base + 24);

        std::string name;
        for (size_t j = strtab_off + name_off; j < rel.size() && rel[j]; j++)
            name += (char)rel[j];

        if (name == ".text") { text_idx = i; text_off = sh_off; text_sz = sh_sz; }
        else if (name == ".data") { data_idx = i; data_off = sh_off; data_sz = sh_sz; }
        else if (name == ".rodata") { rodata_idx = i; rodata_off = sh_off; rodata_sz = sh_sz; }
        else if (name == ".bss") { bss_idx = i; bss_sz = sh_sz; }
        else if (name == ".symtab") { symtab_off = sh_off; symtab_sz = sh_sz; }
        else if (name == ".strtab") { strtab_ofs = sh_off; strtab_sz = sh_sz; }
        else if (name == ".rela.text") { rela_text_off = sh_off; rela_text_sz = sh_sz; }
    }

    auto get_sec = [&](uint64_t off, uint64_t sz) {
        if (off == 0 || sz == 0) return std::vector<uint8_t>();
        if (off + sz > rel.size()) return std::vector<uint8_t>();
        return std::vector<uint8_t>(rel.begin() + off, rel.begin() + off + sz);
    };
    auto text_sec = get_sec(text_off, text_sz);
    auto data_sec = get_sec(data_off, data_sz);
    auto rodata_sec = get_sec(rodata_off, rodata_sz);
    auto symtab = get_sec(symtab_off, symtab_sz);
    auto strtab = get_sec(strtab_ofs, strtab_sz);
    auto rela_text = get_sec(rela_text_off, rela_text_sz);

    // Pad removal
    while (!text_sec.empty() && text_sec.back() == 0xCC) text_sec.pop_back();

    const uint64_t PAGE = 0x1000;
    const uint64_t BASE = 0x400000;
    uint64_t text_vaddr = BASE;
    uint64_t data_vaddr = BASE + PAGE;
    if (!data_sec.empty() || !rodata_sec.empty()) data_vaddr = BASE + PAGE;
    uint64_t entry = text_vaddr + entry_off;

    // Symbol vaddr map
    std::unordered_map<std::string, uint64_t> sym_vaddrs;
    for (auto& sym : program.symbols) {
        if (!sym.defined) continue;
        uint64_t base = 0;
        switch (sym.section) {
            case IRSectionKind::Text: base = text_vaddr; break;
            case IRSectionKind::Data: base = data_vaddr; break;
            case IRSectionKind::Rodata: base = text_vaddr; break; // simplified
            default: continue;
        }
        sym_vaddrs[sym.name] = base + sym.offset;
    }

    // Resolve RELA relocations (24 bytes each)
    for (uint64_t r = 0; r + 24 <= rela_text.size(); r += 24) {
        uint64_t r_offset = r64(rela_text, r);
        uint64_t r_info = r64(rela_text, r + 8);
        int64_t r_addend = static_cast<int64_t>(r64(rela_text, r + 16));
        uint32_t sym_idx = static_cast<uint32_t>(r_info >> 32);
        uint32_t rel_type = static_cast<uint32_t>(r_info & 0xFFFFFFFF);
        if (r_offset >= text_sec.size()) continue;

        uint64_t S = 0;
        if (sym_idx > 0 && (sym_idx + 1) * 24 <= symtab.size()) {
            uint32_t s_name = r32(symtab, sym_idx * 24);
            std::string sname;
            for (size_t j = s_name; j < strtab.size() && strtab[j]; j++)
                sname += (char)strtab[j];
            auto it = sym_vaddrs.find(sname);
            S = (it != sym_vaddrs.end()) ? it->second : r64(symtab, sym_idx * 24 + 8);
        }

        if (rel_type == 1) { // R_X86_64_64
            int64_t val = static_cast<int64_t>(S) + r_addend;
            w64(text_sec, r_offset, static_cast<uint64_t>(val));
        } else if (rel_type == 2) { // R_X86_64_PC32
            uint64_t P = text_vaddr + r_offset;
            int64_t val = static_cast<int64_t>(S) + r_addend - static_cast<int64_t>(P);
            w32(text_sec, r_offset, static_cast<uint32_t>(val));
        }
    }

    // Section names
    std::vector<uint8_t> shstrtab; shstrtab.push_back(0);
    auto add_name = [&](const std::string& n) -> uint32_t {
        uint32_t pos = static_cast<uint32_t>(shstrtab.size());
        shstrtab.insert(shstrtab.end(), n.begin(), n.end());
        shstrtab.push_back(0);
        return pos;
    };
    std::vector<uint32_t> name_offs;
    name_offs.push_back(add_name(""));
    name_offs.push_back(add_name(".text"));
    name_offs.push_back(add_name(".rodata"));
    name_offs.push_back(add_name(".data"));
    name_offs.push_back(add_name(".bss"));
    name_offs.push_back(add_name(".comment"));
    name_offs.push_back(add_name(".note.GNU-stack"));
    uint32_t debug_line_idx = 0, debug_info_idx = 0, debug_abbrev_idx = 0, debug_str_idx = 0;
    if (line_entries && !line_entries->empty()) {
        debug_line_idx = static_cast<uint32_t>(name_offs.size());
        name_offs.push_back(add_name(".debug_line"));
        debug_info_idx = static_cast<uint32_t>(name_offs.size());
        name_offs.push_back(add_name(".debug_info"));
        debug_abbrev_idx = static_cast<uint32_t>(name_offs.size());
        name_offs.push_back(add_name(".debug_abbrev"));
        debug_str_idx = static_cast<uint32_t>(name_offs.size());
        name_offs.push_back(add_name(".debug_str"));
    }
    name_offs.push_back(add_name(".symtab"));
    name_offs.push_back(add_name(".strtab"));
    name_offs.push_back(add_name(".shstrtab"));

    // Symbol table (24-byte entries)
    std::vector<uint8_t> new_strtab; new_strtab.push_back(0);
    std::vector<uint8_t> new_symtab;
    // NULL entry: 24 zero bytes
    new_symtab.insert(new_symtab.end(), 24, 0);

    auto push64 = [](std::vector<uint8_t>& v, uint64_t val) {
        for (int i = 0; i < 8; i++) v.push_back((val >> (i * 8)) & 0xFF);
    };
    auto push32 = [](std::vector<uint8_t>& v, uint32_t val) {
        for (int i = 0; i < 4; i++) v.push_back((val >> (i * 8)) & 0xFF);
    };
    auto push16 = [](std::vector<uint8_t>& v, uint16_t val) {
        v.push_back(val & 0xFF); v.push_back((val >> 8) & 0xFF);
    };

    for (auto& sym : program.symbols) {
        if (!sym.defined) continue;
        uint64_t base = 0; uint16_t shn = 0;
        switch (sym.section) {
            case IRSectionKind::Text: base = text_vaddr; shn = 1; break;
            case IRSectionKind::Data: base = data_vaddr; shn = 3; break;
            case IRSectionKind::Rodata: base = text_vaddr; shn = 2; break;
            case IRSectionKind::Bss: shn = 4; break;
            default: break;
        }
        uint8_t bind = (sym.binding == IRSymbolBinding::Global) ? 1 : 0;
        uint8_t type = sym.is_function ? 2 : 1;
        uint32_t nm_off = static_cast<uint32_t>(new_strtab.size());
        new_strtab.insert(new_strtab.end(), sym.name.begin(), sym.name.end());
        new_strtab.push_back(0);
        push32(new_symtab, nm_off);           // st_name
        new_symtab.push_back(static_cast<uint8_t>((bind << 4) | type)); // st_info
        new_symtab.push_back(0);              // st_other
        push16(new_symtab, shn);              // st_shndx
        push64(new_symtab, base + sym.offset); // st_value
        push64(new_symtab, sym.size);          // st_size
    }

    std::string cstr = "DASM x86-64 assembler (DemiEngine v1.0)";
    std::vector<uint8_t> cdata(cstr.begin(), cstr.end()); cdata.push_back(0);

    uint64_t bss_size = 0;
    for (auto& rec : program.data_records)
        if (rec.section == IRSectionKind::Bss && rec.directive == ".resb" && !rec.values.empty())
            bss_size += static_cast<uint64_t>(std::max<int64_t>(0, std::get<IRImmediateOperand>(rec.values[0].value).value));

    const size_t EHDR = 64, PHDR = 56;
    uint16_t phnum = 1;

    uint32_t page_pad = PAGE - (EHDR + phnum * PHDR) % PAGE;
    if (page_pad == PAGE) page_pad = 0;
    uint64_t exe_foff = EHDR + phnum * PHDR + page_pad;

    // Section sizes and file offsets
    struct FSec { std::vector<uint8_t> data; uint64_t off, sz, vaddr; };
    FSec ftx{text_sec, 0, text_sec.size(), text_vaddr};
    FSec fdt{data_sec, 0, data_sec.size(), data_vaddr};
    FSec fro{rodata_sec, 0, rodata_sec.size(), text_vaddr};
    FSec fcm{cdata, 0, cdata.size(), 0};
    FSec fsm{new_symtab, 0, new_symtab.size(), 0};
    FSec fst{new_strtab, 0, new_strtab.size(), 0};
    FSec fsh{shstrtab, 0, shstrtab.size(), 0};
    FSec fdl, fdi, fda, fds;  // DWARF sections

    uint64_t foff = exe_foff;
    ftx.off = foff; foff += ftx.sz;
    fdt.off = foff; foff += fdt.sz;
    fro.off = foff; foff += fro.sz;
    fcm.off = foff; foff += fcm.sz;
    if (line_entries && !line_entries->empty()) {
        DWARFSections dwarf = generate_dwarf(source_file, text_vaddr, *line_entries, ".");
        fdl = {dwarf.debug_line, 0, dwarf.debug_line.size(), 0};
        fdi = {dwarf.debug_info, 0, dwarf.debug_info.size(), 0};
        fda = {dwarf.debug_abbrev, 0, dwarf.debug_abbrev.size(), 0};
        fds = {dwarf.debug_str, 0, dwarf.debug_str.size(), 0};
        fdl.off = foff; foff += fdl.sz;
        fdi.off = foff; foff += fdi.sz;
        fda.off = foff; foff += fda.sz;
        fds.off = foff; foff += fds.sz;
    }
    fsm.off = foff; foff += fsm.sz;
    fst.off = foff; foff += fst.sz;
    fsh.off = foff; foff += fsh.sz;

    std::vector<uint8_t> elf;
    elf.resize(exe_foff, 0);

    // ELF64 header
    std::memcpy(elf.data(), "\x7f""ELF", 4);
    elf[4] = 2; elf[5] = 1; elf[6] = 1;  // 64-bit, LE, v1
    w16(elf, 16, 2); // ET_EXEC
    w16(elf, 18, 62); // EM_X86_64
    w32(elf, 20, 1); // version
    w64(elf, 24, entry);
    w64(elf, 32, EHDR); // phoff
    w64(elf, 40, 0); // shoff placeholder
    w32(elf, 48, 0);
    w16(elf, 52, EHDR); w16(elf, 54, PHDR); w16(elf, 56, phnum);
    w16(elf, 58, 64); w16(elf, 60, 10); w16(elf, 62, 9); // shstrndx=9

    // Program header: LOAD .text + .rodata (R|X)
    uint64_t load0_fsize = ftx.sz + fro.sz;
    uint64_t load0_msize = align_up(load0_fsize, PAGE);
    w32(elf, EHDR, 1); w32(elf, EHDR+4, 5);
    w64(elf, EHDR+8, exe_foff); w64(elf, EHDR+16, text_vaddr);
    w64(elf, EHDR+24, text_vaddr); w64(elf, EHDR+32, load0_fsize);
    w64(elf, EHDR+40, load0_msize); w64(elf, EHDR+48, PAGE);

    // LOAD .data + .bss (R|W) — separate page-aligned segment
    uint64_t data_off_file = align_up(exe_foff + load0_fsize, PAGE);
    while (elf.size() < data_off_file) elf.push_back(0);
    // Recalculate data offsets
    foff = data_off_file;
    fdt.off = foff; foff += fdt.sz;
    fcm.off = foff; foff += fcm.sz;
    fsm.off = foff; foff += fsm.sz;
    fst.off = foff; foff += fst.sz;
    fsh.off = foff; foff += fsh.sz;

    uint64_t load1_fsize = fdt.sz;
    uint64_t load1_msize = align_up(load1_fsize + bss_size, PAGE);
    phnum = 2;
    w16(elf, 56, phnum);

    w32(elf, EHDR + PHDR, 1); w32(elf, EHDR + PHDR + 4, 6);
    w64(elf, EHDR + PHDR + 8, data_off_file);
    w64(elf, EHDR + PHDR + 16, data_vaddr);
    w64(elf, EHDR + PHDR + 24, data_vaddr);
    w64(elf, EHDR + PHDR + 32, load1_fsize);
    w64(elf, EHDR + PHDR + 40, load1_msize);
    w64(elf, EHDR + PHDR + 48, PAGE);

    // Write section data at offsets
    auto write_at = [&](std::vector<uint8_t>& dst, uint64_t off, const std::vector<uint8_t>& src) {
        if (dst.size() < off + src.size()) dst.resize(off + src.size(), 0);
        std::memcpy(dst.data() + off, src.data(), src.size());
    };
    write_at(elf, ftx.off, ftx.data);
    write_at(elf, fdt.off, fdt.data);
    write_at(elf, fcm.off, fcm.data);
    if (line_entries && !line_entries->empty()) {
        write_at(elf, fdl.off, fdl.data);
        write_at(elf, fdi.off, fdi.data);
        write_at(elf, fda.off, fda.data);
        write_at(elf, fds.off, fds.data);
    }
    uint64_t symtab_exe_off = fsm.off;
    write_at(elf, fsm.off, fsm.data);
    uint64_t strtab_exe_off = fst.off;
    write_at(elf, fst.off, fst.data);
    uint64_t shstr_exe_off = fsh.off;
    write_at(elf, fsh.off, fsh.data);

    // Section headers
    uint64_t shdr_start = elf.size();
    w64(elf, 40, shdr_start);

    auto push_shdr64 = [&](uint32_t name_off, uint32_t type, uint64_t flags,
                            uint64_t addr, uint64_t off, uint64_t size,
                            uint32_t link, uint32_t info, uint64_t align, uint64_t entsize) {
        push32(elf, name_off); push32(elf, type); push64(elf, flags);
        push64(elf, addr); push64(elf, off); push64(elf, size);
        push32(elf, link); push32(elf, info); push64(elf, align); push64(elf, entsize);
    };

    push_shdr64(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    push_shdr64(name_offs[1], 1, 6, text_vaddr, ftx.off, ftx.sz, 0, 0, 16, 0);
    push_shdr64(name_offs[2], 1, 2, text_vaddr, fro.off, fro.sz, 0, 0, 4, 0);
    push_shdr64(name_offs[3], 1, 3, data_vaddr, fdt.off, fdt.sz, 0, 0, 4, 0);
    push_shdr64(name_offs[4], 8, 3, data_vaddr + fdt.sz, 0, bss_size, 0, 0, 4, 0);
    push_shdr64(name_offs[5], 1, 0, 0, fcm.off, fcm.sz, 0, 0, 1, 0);
    push_shdr64(name_offs[6], 7, 0, 0, 0, 0, 0, 0, 1, 0);
    uint32_t next_idx = 7;
    if (line_entries && !line_entries->empty()) {
        push_shdr64(name_offs[debug_line_idx], 1, 0, 0, fdl.off, fdl.sz, 0, 0, 1, 0);
        push_shdr64(name_offs[debug_info_idx], 1, 0, 0, fdi.off, fdi.sz, 0, 0, 1, 0);
        push_shdr64(name_offs[debug_abbrev_idx], 1, 0, 0, fda.off, fda.sz, 0, 0, 1, 0);
        push_shdr64(name_offs[debug_str_idx], 1, 0, 0, fds.off, fds.sz, 0, 0, 1, 0);
        next_idx += 4;
    }
    push_shdr64(name_offs[next_idx], 2, 0, 0, symtab_exe_off, new_symtab.size(), 8, 1, 8, 24);
    push_shdr64(name_offs[next_idx+1], 3, 0, 0, strtab_exe_off, new_strtab.size(), 0, 0, 1, 0);
    push_shdr64(name_offs[next_idx+2], 3, 0, 0, shstr_exe_off, shstrtab.size(), 0, 0, 1, 0);

    // Fix shnum in ELF header
    uint16_t shnum_total = static_cast<uint16_t>(next_idx + 3);
    w16(elf, 60, shnum_total);

    return elf;
}

} // namespace Assembler
