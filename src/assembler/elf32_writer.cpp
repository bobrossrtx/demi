#include "elf32_writer.hpp"

#include <algorithm>
#include <cstring>
#include <unordered_map>

namespace Assembler {

namespace {

constexpr uint8_t ELF_MAGIC[4] = {0x7F, 'E', 'L', 'F'};
constexpr uint8_t ELF_CLASS_32 = 1;
constexpr uint8_t ELF_DATA_2LSB = 1;
constexpr uint8_t ELF_OSABI_SYSV = 0;
constexpr uint16_t ET_REL = 1;
constexpr uint16_t EM_386 = 3;
constexpr uint32_t EV_CURRENT = 1;
constexpr uint16_t EHDR_SIZE = 52;
constexpr uint16_t SHDR_SIZE = 40;
constexpr uint16_t SHN_UNDEF = 0;
constexpr uint16_t SHN_ABS = 0xFFF1;
constexpr uint32_t SHT_NULL = 0;
constexpr uint32_t SHT_PROGBITS = 1;
constexpr uint32_t SHT_SYMTAB = 2;
constexpr uint32_t SHT_STRTAB = 3;
constexpr uint32_t SHT_NOBITS = 8;
constexpr uint32_t SHT_REL = 9;
constexpr uint32_t SHT_NOTE = 7;
constexpr uint32_t SHF_WRITE = 0x1;
constexpr uint32_t SHF_ALLOC = 0x2;
constexpr uint32_t SHF_EXECINSTR = 0x4;
constexpr uint8_t STB_LOCAL = 0;
constexpr uint8_t STB_GLOBAL = 1;
constexpr uint8_t STT_NOTYPE = 0;
constexpr uint8_t STT_OBJECT = 1;
constexpr uint8_t STT_FUNC = 2;
constexpr uint8_t STT_SECTION = 3;
constexpr uint32_t R_386_32 = 1;
constexpr uint32_t R_386_PC32 = 2;

void write16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void write32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

size_t append_string(std::vector<uint8_t>& table, const std::string& value) {
    const size_t offset = table.size();
    table.insert(table.end(), value.begin(), value.end());
    table.push_back('\0');
    return offset;
}

uint8_t symbol_binding(IRSymbolBinding binding) {
    return binding == IRSymbolBinding::Global ? STB_GLOBAL : STB_LOCAL;
}

size_t align_to(size_t value, size_t alignment) {
    if (alignment <= 1) {
        return value;
    }
    const size_t mask = alignment - 1;
    return (value + mask) & ~mask;
}

std::vector<uint8_t> emit_section_payload(const IRProgram& program, IRSectionKind section) {
    std::vector<uint8_t> out;

    for (const auto& record : program.data_records) {
        if (record.section != section) {
            continue;
        }

        if (record.directive == "DB") {
            for (const auto& value : record.values) {
                if (value.kind == IROperandKind::Immediate) {
                    out.push_back(static_cast<uint8_t>(std::get<IRImmediateOperand>(value.value).value & 0xFF));
                } else if (value.kind == IROperandKind::Symbol) {
                    const auto& text = std::get<IRSymbolOperand>(value.value).name;
                    out.insert(out.end(), text.begin(), text.end());
                }
            }
            continue;
        }

        if (record.directive == ".string") {
            for (const auto& value : record.values) {
                if (value.kind == IROperandKind::Symbol) {
                    const auto& text = std::get<IRSymbolOperand>(value.value).name;
                    out.insert(out.end(), text.begin(), text.end());
                    out.push_back('\0');
                }
            }
            continue;
        }

        if (record.directive == ".dw") {
            for (const auto& value : record.values) {
                const auto imm = static_cast<uint16_t>(std::get<IRImmediateOperand>(value.value).value);
                write16(out, imm);
            }
            continue;
        }

        if (record.directive == ".dd") {
            for (const auto& value : record.values) {
                if (value.kind == IROperandKind::Immediate) {
                    write32(out, static_cast<uint32_t>(std::get<IRImmediateOperand>(value.value).value));
                } else {
                    write32(out, 0);
                }
            }
            continue;
        }

        if (record.directive == ".dq") {
            for (const auto& value : record.values) {
                const auto imm = static_cast<uint64_t>(std::get<IRImmediateOperand>(value.value).value);
                write32(out, static_cast<uint32_t>(imm & 0xFFFFFFFF));
                write32(out, static_cast<uint32_t>((imm >> 32) & 0xFFFFFFFF));
            }
            continue;
        }

        if (record.directive == ".asciz") {
            for (const auto& value : record.values) {
                if (value.kind == IROperandKind::Symbol) {
                    const auto& text = std::get<IRSymbolOperand>(value.value).name;
                    out.insert(out.end(), text.begin(), text.end());
                    out.push_back('\0');
                }
            }
            continue;
        }

        if (record.directive == ".zero") {
            if (!record.values.empty() && record.values.front().kind == IROperandKind::Immediate) {
                const auto count = static_cast<size_t>(std::max<int64_t>(0, std::get<IRImmediateOperand>(record.values.front().value).value));
                out.resize(out.size() + count, 0);
            }
            continue;
        }
    }

    return out;
}

size_t compute_bss_size(const IRProgram& program) {
    size_t total = 0;
    for (const auto& record : program.data_records) {
        if (record.section != IRSectionKind::Bss) {
            continue;
        }
        if (!record.values.empty() && record.values.front().kind == IROperandKind::Immediate) {
            total += static_cast<size_t>(std::max<int64_t>(0, std::get<IRImmediateOperand>(record.values.front().value).value));
        }
    }
    return total;
}

} // namespace

std::vector<uint8_t> ELF32ObjectWriter::write_object(
    const IRProgram& program,
    std::vector<std::string>& errors,
    const std::vector<uint8_t>* text_bytes,
    const std::vector<IRRelocation>* text_relocations) {
    if (program.target != IRTarget::X86Elf32) {
        errors.push_back("ELF32 object writer requires IR target x86-elf32");
        return {};
    }

    if (!program.instructions.empty() && text_bytes == nullptr) {
        errors.push_back("ELF32 object writer currently supports data/section object output only; text instruction emission is not implemented yet");
        return {};
    }

    struct SectionDef {
        std::string name;
        uint32_t name_offset = 0;
        uint32_t type = SHT_NULL;
        uint32_t flags = 0;
        uint32_t offset = 0;
        uint32_t size = 0;
        uint32_t link = 0;
        uint32_t info = 0;
        uint32_t addralign = 1;
        uint32_t entsize = 0;
        std::vector<uint8_t> data;
    };

    std::vector<SectionDef> sections;
    sections.push_back({"", 0, SHT_NULL, 0, 0, 0, 0, 0, 0, 0, {}});

    const uint32_t text_index = static_cast<uint32_t>(sections.size());
    sections.push_back({".text", 0, SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, 0, 0, 0, 0, 16, 0,
        text_bytes ? *text_bytes : emit_section_payload(program, IRSectionKind::Text)});
    sections.back().size = static_cast<uint32_t>(sections.back().data.size());
    const uint32_t data_index = static_cast<uint32_t>(sections.size());
    sections.push_back({".data", 0, SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, 0, 0, 0, 0, 4, 0, emit_section_payload(program, IRSectionKind::Data)});
    sections.back().size = static_cast<uint32_t>(sections.back().data.size());
    const uint32_t rodata_index = static_cast<uint32_t>(sections.size());
    sections.push_back({".rodata", 0, SHT_PROGBITS, SHF_ALLOC, 0, 0, 0, 0, 1, 0, emit_section_payload(program, IRSectionKind::Rodata)});
    sections.back().size = static_cast<uint32_t>(sections.back().data.size());
    const uint32_t bss_index = static_cast<uint32_t>(sections.size());
    sections.push_back({".bss", 0, SHT_NOBITS, SHF_ALLOC | SHF_WRITE, 0, static_cast<uint32_t>(compute_bss_size(program)), 0, 0, 4, 0, {}});

    std::vector<uint8_t> strtab(1, '\0');
    std::vector<uint8_t> symtab;
    std::vector<uint8_t> rel_text;
    std::vector<uint8_t> rel_data;
    std::vector<uint8_t> rel_rodata;
    std::vector<uint8_t> shstrtab(1, '\0');

    auto write_symbol = [&](uint32_t name, uint32_t value, uint32_t size, uint8_t info, uint8_t other, uint16_t shndx) {
        write32(symtab, name);
        write32(symtab, value);
        write32(symtab, size);
        symtab.push_back(info);
        symtab.push_back(other);
        write16(symtab, shndx);
    };

    write_symbol(0, 0, 0, 0, 0, 0);
    write_symbol(0, 0, 0, static_cast<uint8_t>((STB_LOCAL << 4) | STT_SECTION), 0, static_cast<uint16_t>(text_index));
    write_symbol(0, 0, 0, static_cast<uint8_t>((STB_LOCAL << 4) | STT_SECTION), 0, static_cast<uint16_t>(data_index));
    write_symbol(0, 0, 0, static_cast<uint8_t>((STB_LOCAL << 4) | STT_SECTION), 0, static_cast<uint16_t>(rodata_index));
    write_symbol(0, 0, 0, static_cast<uint8_t>((STB_LOCAL << 4) | STT_SECTION), 0, static_cast<uint16_t>(bss_index));

    std::unordered_map<std::string, uint32_t> symbol_indices;
    uint32_t next_symbol_index = 5;
    for (const auto& symbol : program.symbols) {
        if (symbol.binding != IRSymbolBinding::Local) {
            continue;
        }
        const uint32_t name_offset = static_cast<uint32_t>(append_string(strtab, symbol.name));
        uint16_t shndx = SHN_UNDEF;
        if (symbol.defined) {
            switch (symbol.section) {
                case IRSectionKind::Text: shndx = static_cast<uint16_t>(text_index); break;
                case IRSectionKind::Data: shndx = static_cast<uint16_t>(data_index); break;
                case IRSectionKind::Rodata: shndx = static_cast<uint16_t>(rodata_index); break;
                case IRSectionKind::Bss: shndx = static_cast<uint16_t>(bss_index); break;
                case IRSectionKind::Custom: shndx = SHN_ABS; break;
            }
        }
        uint8_t sym_type = symbol.is_function ? STT_FUNC : STT_NOTYPE;
        write_symbol(name_offset, static_cast<uint32_t>(symbol.offset), 0, static_cast<uint8_t>((symbol_binding(symbol.binding) << 4) | sym_type), 0, shndx);
        symbol_indices[symbol.name] = next_symbol_index++;
    }

    const uint32_t first_global_symbol_index = next_symbol_index;
    for (const auto& symbol : program.symbols) {
        if (symbol.binding == IRSymbolBinding::Local) {
            continue;
        }
        const uint32_t name_offset = static_cast<uint32_t>(append_string(strtab, symbol.name));
        uint16_t shndx = SHN_UNDEF;
        if (symbol.defined) {
            switch (symbol.section) {
                case IRSectionKind::Text: shndx = static_cast<uint16_t>(text_index); break;
                case IRSectionKind::Data: shndx = static_cast<uint16_t>(data_index); break;
                case IRSectionKind::Rodata: shndx = static_cast<uint16_t>(rodata_index); break;
                case IRSectionKind::Bss: shndx = static_cast<uint16_t>(bss_index); break;
                case IRSectionKind::Custom: shndx = SHN_ABS; break;
            }
        }
        uint8_t sym_type = symbol.is_function ? STT_FUNC : STT_NOTYPE;
        write_symbol(name_offset, static_cast<uint32_t>(symbol.offset), 0, static_cast<uint8_t>((symbol_binding(symbol.binding) << 4) | sym_type), 0, shndx);
        symbol_indices[symbol.name] = next_symbol_index++;
    }

    auto append_relocation = [&](std::vector<uint8_t>& out, const IRRelocation& relocation) {
        auto it = symbol_indices.find(relocation.symbol);
        if (it == symbol_indices.end()) {
            errors.push_back("Missing symbol for relocation: " + relocation.symbol);
            return;
        }

        uint32_t relocation_type = 0;
        switch (relocation.kind) {
            case IRRelocationKind::Absolute32:
                relocation_type = R_386_32;
                break;
            case IRRelocationKind::PcRelative32:
                relocation_type = R_386_PC32;
                break;
            default:
                errors.push_back("Unsupported ELF32 relocation kind for symbol: " + relocation.symbol);
                return;
        }

        write32(out, static_cast<uint32_t>(relocation.offset));
        const uint32_t info = (it->second << 8) | relocation_type;
        write32(out, info);
    };

    if (text_relocations != nullptr) {
        for (const auto& relocation : *text_relocations) {
            append_relocation(rel_text, relocation);
        }
    }

    for (const auto& relocation : program.relocations) {
        switch (relocation.section) {
            case IRSectionKind::Text:
                if (text_relocations == nullptr) {
                    append_relocation(rel_text, relocation);
                }
                break;
            case IRSectionKind::Data:
                append_relocation(rel_data, relocation);
                break;
            case IRSectionKind::Rodata:
                append_relocation(rel_rodata, relocation);
                break;
            default:
                break;
        }
    }

    const uint32_t rel_text_index = static_cast<uint32_t>(sections.size());
    sections.push_back({".rel.text", 0, SHT_REL, 0, 0, static_cast<uint32_t>(rel_text.size()), 0, text_index, 4, 8, rel_text});
    const uint32_t rel_data_index = static_cast<uint32_t>(sections.size());
    sections.push_back({".rel.data", 0, SHT_REL, 0, 0, static_cast<uint32_t>(rel_data.size()), 0, data_index, 4, 8, rel_data});
    const uint32_t rel_rodata_index = static_cast<uint32_t>(sections.size());
    sections.push_back({".rel.rodata", 0, SHT_REL, 0, 0, static_cast<uint32_t>(rel_rodata.size()), 0, rodata_index, 4, 8, rel_rodata});
    const uint32_t symtab_index = static_cast<uint32_t>(sections.size());
    sections.push_back({".symtab", 0, SHT_SYMTAB, 0, 0, static_cast<uint32_t>(symtab.size()), 0, first_global_symbol_index, 4, 16, symtab});
    const uint32_t strtab_index = static_cast<uint32_t>(sections.size());
    sections.push_back({".strtab", 0, SHT_STRTAB, 0, 0, static_cast<uint32_t>(strtab.size()), 0, 0, 1, 0, strtab});
    const uint32_t shstrtab_index = static_cast<uint32_t>(sections.size());
    sections.push_back({".shstrtab", 0, SHT_STRTAB, 0, 0, 0, 0, 0, 1, 0, shstrtab});
    sections.push_back({".note.GNU-stack", 0, SHT_NOTE, 0, 0, 0, 0, 0, 1, 0, {}});

    for (auto& section : sections) {
        section.name_offset = static_cast<uint32_t>(append_string(shstrtab, section.name));
    }
    sections[shstrtab_index].data = shstrtab;
    sections[shstrtab_index].size = static_cast<uint32_t>(shstrtab.size());
    sections[symtab_index].link = strtab_index;
    sections[rel_text_index].link = symtab_index;
    sections[rel_data_index].link = symtab_index;
    sections[rel_rodata_index].link = symtab_index;

    std::vector<uint8_t> elf(EHDR_SIZE, 0);
    std::memcpy(elf.data(), ELF_MAGIC, 4);
    elf[4] = ELF_CLASS_32;
    elf[5] = ELF_DATA_2LSB;
    elf[6] = EV_CURRENT;
    elf[7] = ELF_OSABI_SYSV;
    elf[16] = static_cast<uint8_t>(ET_REL & 0xFF);
    elf[17] = static_cast<uint8_t>((ET_REL >> 8) & 0xFF);
    elf[18] = static_cast<uint8_t>(EM_386 & 0xFF);
    elf[19] = static_cast<uint8_t>((EM_386 >> 8) & 0xFF);
    elf[20] = static_cast<uint8_t>(EV_CURRENT & 0xFF);
    elf[40] = static_cast<uint8_t>(EHDR_SIZE & 0xFF);
    elf[41] = static_cast<uint8_t>((EHDR_SIZE >> 8) & 0xFF);
    elf[46] = static_cast<uint8_t>(SHDR_SIZE & 0xFF);
    elf[47] = static_cast<uint8_t>((SHDR_SIZE >> 8) & 0xFF);
    elf[48] = static_cast<uint8_t>(sections.size() & 0xFF);
    elf[49] = static_cast<uint8_t>((sections.size() >> 8) & 0xFF);
    elf[50] = static_cast<uint8_t>(shstrtab_index & 0xFF);
    elf[51] = static_cast<uint8_t>((shstrtab_index >> 8) & 0xFF);

    size_t file_offset = EHDR_SIZE;
    for (size_t index = 1; index < sections.size(); ++index) {
        auto& section = sections[index];
        if (section.type == SHT_NOBITS || section.data.empty()) {
            section.offset = 0;
            continue;
        }
        file_offset = align_to(file_offset, section.addralign);
        section.offset = static_cast<uint32_t>(file_offset);
        file_offset += section.data.size();
    }

    const uint32_t shoff = static_cast<uint32_t>(align_to(file_offset, 4));
    elf[32] = static_cast<uint8_t>(shoff & 0xFF);
    elf[33] = static_cast<uint8_t>((shoff >> 8) & 0xFF);
    elf[34] = static_cast<uint8_t>((shoff >> 16) & 0xFF);
    elf[35] = static_cast<uint8_t>((shoff >> 24) & 0xFF);

    elf.resize(shoff, 0);
    for (const auto& section : sections) {
        if (section.type == SHT_NOBITS || section.data.empty()) {
            continue;
        }
        if (elf.size() < section.offset + section.data.size()) {
            elf.resize(section.offset + section.data.size(), 0);
        }
        std::copy(section.data.begin(), section.data.end(), elf.begin() + section.offset);
    }

    elf.resize(shoff);
    for (const auto& section : sections) {
        write32(elf, section.name_offset);
        write32(elf, section.type);
        write32(elf, section.flags);
        write32(elf, 0);
        write32(elf, section.offset);
        write32(elf, section.size);
        write32(elf, section.link);
        write32(elf, section.info);
        write32(elf, section.addralign);
        write32(elf, section.entsize);
    }

    return elf;
}

} // namespace Assembler
