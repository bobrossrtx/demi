#include "elf32_writer.hpp"
#include <cstring>
#include <algorithm>
#include <unordered_map>

namespace Assembler {

namespace {
constexpr uint32_t SHT_NULL = 0;
constexpr uint32_t SHT_PROGBITS = 1;
constexpr uint32_t SHT_SYMTAB = 2;
constexpr uint32_t SHT_STRTAB = 3;
constexpr uint32_t SHT_NOBITS = 8;
constexpr uint32_t SHT_NOTE = 7;
constexpr uint32_t SHF_WRITE = 0x1;
constexpr uint32_t SHF_ALLOC = 0x2;
constexpr uint32_t SHF_EXECINSTR = 0x4;
constexpr uint8_t STB_LOCAL = 0;
constexpr uint8_t STB_GLOBAL = 1;
constexpr uint8_t STT_NOTYPE = 0;
constexpr uint8_t STT_OBJECT = 1;
constexpr uint8_t STT_FUNC = 2;
}

static void w32(std::vector<uint8_t>& v, size_t off, uint32_t val) {
    v[off] = static_cast<uint8_t>(val & 0xFF);
    v[off+1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    v[off+2] = static_cast<uint8_t>((val >> 16) & 0xFF);
    v[off+3] = static_cast<uint8_t>((val >> 24) & 0xFF);
}
static uint32_t r32(const std::vector<uint8_t>& v, size_t off) {
    return (uint32_t)v[off] | ((uint32_t)v[off+1] << 8) | ((uint32_t)v[off+2] << 16) | ((uint32_t)v[off+3] << 24);
}
static void push32(std::vector<uint8_t>& v, uint32_t val) {
    v.push_back(val & 0xFF); v.push_back((val>>8)&0xFF);
    v.push_back((val>>16)&0xFF); v.push_back((val>>24)&0xFF);
}
static void push16(std::vector<uint8_t>& v, uint16_t val) {
    v.push_back(val & 0xFF); v.push_back((val>>8)&0xFF);
}
static void w16at(std::vector<uint8_t>& v, size_t off, uint16_t val) {
    v[off] = static_cast<uint8_t>(val & 0xFF);
    v[off+1] = static_cast<uint8_t>((val >> 8) & 0xFF);
}
static uint32_t align_up(uint32_t addr, uint32_t a) { return (addr + a - 1) & ~(a - 1); }

std::vector<uint8_t> ELF32ObjectWriter::write_executable(
    const IRProgram& program,
    std::vector<std::string>& errors,
    const std::vector<uint8_t>* text_bytes,
    const std::vector<uint8_t>* data_bytes,
    const std::vector<uint8_t>* rodata_bytes,
    const std::vector<IRRelocation>* text_relocations) {

    static const std::vector<uint8_t> empty;
    if (!text_bytes) text_bytes = &empty;
    if (!data_bytes) data_bytes = &empty;
    if (!rodata_bytes) rodata_bytes = &empty;

    // Find entry point
    uint64_t entry_off = 0;
    for (auto& sym : program.symbols)
        if (sym.name == "_start" && sym.defined) { entry_off = sym.offset; break; }

    // Build section payloads (use provided bytes or empty — caller is responsible)
    std::vector<uint8_t> text_payload = *text_bytes;
    std::vector<uint8_t> data_payload = *data_bytes;
    std::vector<uint8_t> rodata_payload = *rodata_bytes;

    // Collect BSS size
    uint64_t bss_size = 0;
    for (auto& rec : program.data_records)
        if (rec.section == IRSectionKind::Bss && rec.directive == ".resb" && !rec.values.empty())
            bss_size += static_cast<uint64_t>(std::max<int64_t>(0, std::get<IRImmediateOperand>(rec.values[0].value).value));

    // Virtual address layout
    const uint32_t PAGE = 0x1000;
    const uint32_t BASE = 0x08048000;
    uint32_t text_vaddr = BASE;
    uint32_t rodata_vaddr = rodata_payload.empty() ? text_vaddr : align_up(text_vaddr + text_payload.size(), PAGE);
    uint32_t data_vaddr = rodata_payload.empty() ?
        align_up(text_vaddr + text_payload.size(), PAGE) :
        align_up(rodata_vaddr + rodata_payload.size(), PAGE);
    uint32_t bss_vaddr = align_up(data_vaddr + data_payload.size(), PAGE);

    uint32_t entry = text_vaddr + static_cast<uint32_t>(entry_off);

    // Build symbol table with virtual addresses
    std::unordered_map<std::string, uint32_t> sym_vaddrs;
    for (auto& sym : program.symbols) {
        if (!sym.defined) continue;
        uint32_t vaddr = 0;
        switch (sym.section) {
            case IRSectionKind::Text: vaddr = text_vaddr; break;
            case IRSectionKind::Data: vaddr = data_vaddr; break;
            case IRSectionKind::Rodata: vaddr = rodata_vaddr; break;
            case IRSectionKind::Bss: vaddr = bss_vaddr; break;
            default: break;
        }
        sym_vaddrs[sym.name] = vaddr + static_cast<uint32_t>(sym.offset);
    }

    // Resolve relocations using backend-provided byte-offset relocations
    if (text_relocations) {
        for (auto& rel : *text_relocations) {
            auto it = sym_vaddrs.find(rel.symbol);
            if (it == sym_vaddrs.end()) continue;
            uint32_t S = it->second;
            int64_t A = rel.addend;
            uint32_t P = text_vaddr + static_cast<uint32_t>(rel.offset);

            if (rel.offset >= text_payload.size()) continue;

            int32_t value = 0;
            if (rel.kind == IRRelocationKind::PcRelative32) {
                value = static_cast<int32_t>(S + A - P);
            } else if (rel.kind == IRRelocationKind::Absolute32) {
                value = static_cast<int32_t>(S + A);
            }
            w32(text_payload, rel.offset, static_cast<uint32_t>(value));
        }
    }

    // Build sections list
    struct Sec {
        std::string name; uint32_t name_off=0, type=0, flags=0, addr=0, off=0, size=0, link=0, info=0, align=1, entsize=0;
        std::vector<uint8_t> data;
    };
    std::vector<Sec> secs;

    secs.push_back({"", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {}}); // NULL
    uint32_t tidx=1; secs.push_back({".text",0,1,SHF_ALLOC|SHF_EXECINSTR,text_vaddr,0,(uint32_t)text_payload.size(),0,0,16,0,text_payload});
    uint32_t ridx=secs.size(); secs.push_back({".rodata",0,1,SHF_ALLOC,rodata_vaddr,0,(uint32_t)rodata_payload.size(),0,0,4,0,rodata_payload});
    uint32_t didx=secs.size(); secs.push_back({".data",0,1,SHF_ALLOC|SHF_WRITE,data_vaddr,0,(uint32_t)data_payload.size(),0,0,4,0,data_payload});
    uint32_t bidx=secs.size(); secs.push_back({".bss",0,SHT_NOBITS,SHF_ALLOC|SHF_WRITE,bss_vaddr,0,(uint32_t)bss_size,0,0,4,0,{}});
    std::string cstr="DASM x86 assembler (DemiEngine v1.0)"; std::vector<uint8_t> cdata(cstr.begin(),cstr.end()); cdata.push_back(0);
    secs.push_back({".comment",0,1,0,0,0,(uint32_t)cdata.size(),0,0,1,0,cdata});
    secs.push_back({".note.GNU-stack",0,SHT_NOTE,0,0,0,0,0,0,1,0,{}});

    // Symbol table
    std::vector<uint8_t> strtab(1,0), symtab;
    // NULL symbol
    symtab.insert(symtab.end(),16,0);
    for (auto& sym : program.symbols) {
        if (!sym.defined) continue;
        uint32_t name_off = strtab.size();
        strtab.insert(strtab.end(), sym.name.begin(), sym.name.end());
        strtab.push_back(0);
        uint32_t sv = sym_vaddrs.count(sym.name) ? sym_vaddrs[sym.name] : 0;
        uint16_t shndx = 0;
        if (sym.section == IRSectionKind::Text) shndx = tidx;
        else if (sym.section == IRSectionKind::Data) shndx = didx;
        else if (sym.section == IRSectionKind::Rodata) shndx = ridx;
        else if (sym.section == IRSectionKind::Bss) shndx = bidx;
        uint8_t bind = (sym.binding == IRSymbolBinding::Global) ? STB_GLOBAL : STB_LOCAL;
        uint8_t type = sym.is_function ? STT_FUNC : STT_OBJECT;
        uint8_t info = static_cast<uint8_t>((bind << 4) | type);
        push32(symtab, name_off);
        push32(symtab, sv);
        push32(symtab, static_cast<uint32_t>(sym.size));
        symtab.push_back(info); symtab.push_back(0);
        push16(symtab, shndx);
    }
    uint32_t sidx = secs.size();
    secs.push_back({".symtab",0,SHT_SYMTAB,0,0,0,(uint32_t)symtab.size(),0,1,4,16,symtab});
    uint32_t stridx = secs.size();
    secs.push_back({".strtab",0,SHT_STRTAB,0,0,0,(uint32_t)strtab.size(),0,0,1,0,strtab});
    secs[sidx].link = stridx;

    // shstrtab
    std::vector<uint8_t> shstrtab(1,0);
    for (auto& s : secs) {
        s.name_off = shstrtab.size();
        shstrtab.insert(shstrtab.end(), s.name.begin(), s.name.end());
        shstrtab.push_back(0);
    }
    uint32_t shidx = secs.size();
    secs.push_back({".shstrtab",0,SHT_STRTAB,0,0,0,(uint32_t)shstrtab.size(),0,0,1,0,shstrtab});

    // Compute file offsets — pad to page alignment for proper mmap
    const size_t EHDR = 52, PHDR = 32;
    const uint16_t phnum = 2;
    // Pad to page boundary so LOAD segments are properly aligned
    uint32_t file_off = EHDR + phnum * PHDR;
    file_off = align_up(file_off, PAGE); // page-align the first section

    // LOAD segment 0: .text + .rodata (R|X)
    uint32_t load0_start = text_vaddr;
    uint32_t load0_fsize = text_payload.size() + rodata_payload.size();
    uint32_t load0_msize = (rodata_payload.empty() ? align_up(text_vaddr + text_payload.size(), PAGE) : rodata_vaddr + rodata_payload.size()) - load0_start;

    // LOAD segment 1: .data + .bss (R|W)
    uint32_t load1_start = data_vaddr;
    uint32_t load1_fsize = data_payload.size();
    uint32_t load1_msize = bss_vaddr + bss_size - load1_start;

    // Assign offsets
    for (auto& s : secs) {
        if (s.type == SHT_NOBITS || s.type == SHT_NULL) continue;
        s.off = file_off;
        file_off += s.size;
    }

    // Build ELF
    std::vector<uint8_t> elf(EHDR + phnum * PHDR, 0);
    // Pad to page alignment
    elf.resize(file_off, 0);
    std::memcpy(elf.data(), "\x7f""ELF", 4);
    elf[4]=1; elf[5]=1; elf[6]=1;
    elf[16]=2; elf[17]=0; // ET_EXEC
    elf[18]=3; elf[19]=0; // EM_386
    w32(elf,20,1);
    w32(elf, 24, entry);
    w32(elf, 28, EHDR);
    w32(elf, 32, 0); // shoff placeholder, set after all data appended
    w32(elf,36,0);
    w32(elf,40,EHDR); w32(elf,42,PHDR); w32(elf,44,phnum);
    w32(elf,46,40); w32(elf,48,secs.size()); w32(elf,50,shidx);

    // Write program headers — only emit LOAD segments with non-zero size
    int phdr_count = 0;
    if (load0_fsize > 0) {
        w32(elf, EHDR + phdr_count * PHDR, 1); // PT_LOAD
        w32(elf, EHDR + phdr_count * PHDR + 4, secs[tidx].off);
        w32(elf, EHDR + phdr_count * PHDR + 8, load0_start);
        w32(elf, EHDR + phdr_count * PHDR + 12, load0_start);
        w32(elf, EHDR + phdr_count * PHDR + 16, load0_fsize);
        w32(elf, EHDR + phdr_count * PHDR + 20, load0_msize);
        w32(elf, EHDR + phdr_count * PHDR + 24, 5);
        w32(elf, EHDR + phdr_count * PHDR + 28, PAGE);
        phdr_count++;
    }
    if (load1_fsize > 0 || bss_size > 0) {
        w32(elf, EHDR + phdr_count * PHDR, 1);
        w32(elf, EHDR + phdr_count * PHDR + 4, secs[didx].off);
        w32(elf, EHDR + phdr_count * PHDR + 8, load1_start);
        w32(elf, EHDR + phdr_count * PHDR + 12, load1_start);
        w32(elf, EHDR + phdr_count * PHDR + 16, load1_fsize);
        w32(elf, EHDR + phdr_count * PHDR + 20, load1_msize);
        w32(elf, EHDR + phdr_count * PHDR + 24, 6);
        w32(elf, EHDR + phdr_count * PHDR + 28, PAGE);
        phdr_count++;
    }
    w16at(elf, 44, static_cast<uint16_t>(phdr_count));

    // Section data
    for (auto& s : secs) {
        if (s.type == SHT_NOBITS || s.type == SHT_NULL) continue;
        elf.insert(elf.end(), s.data.begin(), s.data.end());
    }

    // Section headers
    for (auto& s : secs) {
        push32(elf, s.name_off);
        push32(elf, s.type);
        push32(elf, s.flags);
        push32(elf, s.addr);
        push32(elf, s.off);
        push32(elf, s.size);
        push32(elf, s.link);
        push32(elf, s.info);
        push32(elf, s.align);
        push32(elf, s.entsize);
    }

    // Fix up shoff — section headers start after all section data
    uint32_t actual_shoff = file_off;
    for (auto& s : secs)
        if (s.type != SHT_NOBITS && s.type != SHT_NULL)
            actual_shoff += s.size;
    w32(elf, 32, actual_shoff);

    return elf;
}

} // namespace Assembler
