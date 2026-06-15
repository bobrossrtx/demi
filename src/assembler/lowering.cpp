#include "lowering.hpp"

#include <stdexcept>

namespace Assembler {

namespace {

std::string normalize_section_name(const std::string& name) {
    if (!name.empty() && name[0] == '.') {
        return name.substr(1);
    }
    return name;
}

} // namespace

IRProgram LoweringContext::lower_program(const Program& program) {
    IRProgram ir_program;
    current_section_ = IRSectionKind::Text;
    text_offset_ = 0;
    data_offset_ = 0;
    bss_offset_ = 0;
    rodata_offset_ = 0;
    custom_offset_ = 0;

    for (const auto& statement : program.statements) {
        lower_statement(*statement, ir_program);
    }

    return ir_program;
}

void LoweringContext::lower_statement(const Statement& statement, IRProgram& ir_program) {
    switch (statement.type) {
        case ASTNodeType::LABEL:
            lower_label(static_cast<const Label&>(statement), ir_program);
            break;
        case ASTNodeType::INSTRUCTION:
            lower_instruction(static_cast<const Instruction&>(statement), ir_program);
            break;
        case ASTNodeType::DIRECTIVE:
            lower_directive(static_cast<const Directive&>(statement), ir_program);
            break;
        default:
            break;
    }
}

void LoweringContext::lower_label(const Label& label, IRProgram& ir_program) {
    // If a global/external symbol with this name already exists (from .global/.extern),
    // update it in-place instead of creating a duplicate local symbol.
    for (auto& symbol : ir_program.symbols) {
        if (symbol.name == label.name && !symbol.defined &&
            (symbol.binding == IRSymbolBinding::Global || symbol.binding == IRSymbolBinding::External)) {
            symbol.section = current_section_;
            symbol.offset = current_offset();
            symbol.defined = true;
            return;
        }
    }

    IRSymbol symbol;
    symbol.name = label.name;
    symbol.section = current_section_;
    symbol.offset = current_offset();
    symbol.defined = true;
    ir_program.symbols.push_back(std::move(symbol));
}

void LoweringContext::lower_instruction(const Instruction& instruction, IRProgram& ir_program) {
    if (is_data_instruction(instruction.mnemonic)) {
        lower_data_instruction(instruction, ir_program);
        return;
    }

    IRInstruction ir_instruction;
    ir_instruction.mnemonic = instruction.mnemonic;
    ir_instruction.line = instruction.line;
    ir_instruction.column = instruction.column;
    ir_instruction.section = current_section_;

    for (const auto& operand : instruction.operands) {
        ir_instruction.operands.push_back(lower_expression(*operand));
    }

    const uint64_t logical_offset = current_offset();
    ir_program.instructions.push_back(std::move(ir_instruction));
    record_instruction_relocations(ir_program.instructions.back(), ir_program, logical_offset);
    current_offset() += 1;
}

void LoweringContext::lower_directive(const Directive& directive, IRProgram& ir_program) {
    if (directive.name == ".text" || directive.name == ".data" || directive.name == ".bss" ||
        directive.name == "section" || directive.name == ".section") {
        if (directive.name == ".text" || directive.name == ".data" || directive.name == ".bss") {
            current_section_ = parse_section_name(directive.name);
            return;
        }

        if (!directive.arguments.empty()) {
            if (auto ident = dynamic_cast<const IdentifierExpression*>(directive.arguments.front().get())) {
                current_section_ = parse_section_name(ident->name);
            }
        }
        return;
    }

    if (directive.name == ".comm") {
        // .comm name, size — common symbol in BSS
        if (directive.arguments.size() >= 2) {
            auto name_ident = dynamic_cast<const IdentifierExpression*>(directive.arguments[0].get());
            auto size_imm = dynamic_cast<const ImmediateExpression*>(directive.arguments[1].get());
            if (name_ident && size_imm) {
                IRSymbol symbol;
                symbol.name = name_ident->name;
                symbol.section = IRSectionKind::Bss;
                symbol.offset = bss_offset_;
                symbol.binding = IRSymbolBinding::Global;
                symbol.defined = true;
                symbol.size = static_cast<uint64_t>(std::max<int64_t>(0, size_imm->value));
                ir_program.symbols.push_back(std::move(symbol));

                IRDataRecord record;
                record.section = IRSectionKind::Bss;
                record.directive = ".resb";
                record.values.push_back({IROperandKind::Immediate, IRImmediateOperand{size_imm->value}});
                ir_program.data_records.push_back(std::move(record));
                bss_offset_ += static_cast<size_t>(std::max<int64_t>(0, size_imm->value));
            }
        }
        return;
    }

    if (directive.name == ".type") {
        // .type name, @function  or  .type name, @object
        if (directive.arguments.size() >= 2) {
            auto name_ident = dynamic_cast<const IdentifierExpression*>(directive.arguments[0].get());
            auto type_ident = dynamic_cast<const IdentifierExpression*>(directive.arguments[1].get());
            if (name_ident && type_ident) {
                for (auto& sym : ir_program.symbols) {
                    if (sym.name == name_ident->name) {
                        if (type_ident->name == "@function" || type_ident->name == "function")
                            sym.is_function = true;
                        // @object is the default (already STT_OBJECT via section check)
                        break;
                    }
                }
            }
        }
        return;
    }

    if (directive.name == ".size") {
        // .size name, expression
        if (directive.arguments.size() >= 2) {
            auto name_ident = dynamic_cast<const IdentifierExpression*>(directive.arguments[0].get());
            auto size_imm = dynamic_cast<const ImmediateExpression*>(directive.arguments[1].get());
            if (name_ident && size_imm) {
                for (auto& sym : ir_program.symbols) {
                    if (sym.name == name_ident->name) {
                        sym.size = static_cast<uint64_t>(size_imm->value);
                        break;
                    }
                }
            }
        }
        return;
    }

    if (directive.name == "global" || directive.name == ".global" || directive.name == ".function") {
        for (const auto& arg : directive.arguments) {
            if (auto ident = dynamic_cast<const IdentifierExpression*>(arg.get())) {
                IRSymbol symbol;
                symbol.name = ident->name;
                symbol.section = current_section_;
                symbol.offset = current_offset();
                symbol.binding = IRSymbolBinding::Global;
                symbol.is_function = (directive.name == ".function") || (current_section_ == IRSectionKind::Text);
                ir_program.symbols.push_back(std::move(symbol));
            }
        }
        return;
    }

    if (directive.name == "extern" || directive.name == ".extern") {
        for (const auto& arg : directive.arguments) {
            if (auto ident = dynamic_cast<const IdentifierExpression*>(arg.get())) {
                IRSymbol symbol;
                symbol.name = ident->name;
                symbol.section = current_section_;
                symbol.offset = 0;
                symbol.binding = IRSymbolBinding::External;
                symbol.defined = false;
                ir_program.symbols.push_back(std::move(symbol));
            }
        }
        return;
    }

    if (directive.name == ".equ") {
        if (directive.arguments.size() >= 2) {
            if (auto ident = dynamic_cast<const IdentifierExpression*>(directive.arguments[0].get())) {
                IRSymbol symbol;
                symbol.name = ident->name;
                symbol.section = IRSectionKind::Custom;
                symbol.offset = 0;
                symbol.defined = true;
                // Extract value from second argument
                if (auto imm = dynamic_cast<const ImmediateExpression*>(directive.arguments[1].get())) {
                    symbol.equ_value = imm->value;
                    ir_program.equ_constants[ident->name] = imm->value;
                }
                ir_program.symbols.push_back(std::move(symbol));
            }
        }
        return;
    }

    if (is_data_directive(directive.name)) {
        // Normalize GNU-style aliases
        std::string canonical = directive.name;
        if (canonical == ".byte") canonical = "DB";
        else if (canonical == ".word") canonical = ".dw";
        else if (canonical == ".long") canonical = ".dd";
        else if (canonical == ".quad") canonical = ".dq";

        IRDataRecord record;
        record.section = current_section_;
        record.directive = canonical;
        record.line = directive.line;
        record.column = directive.column;

        for (const auto& arg : directive.arguments) {
            record.values.push_back(lower_expression(*arg));
        }

        const uint64_t logical_offset = current_offset();
        ir_program.data_records.push_back(std::move(record));
        record_data_record_relocations(ir_program.data_records.back(), ir_program, logical_offset);
        current_offset() += estimate_data_record_size(ir_program.data_records.back());
    }
}

void LoweringContext::lower_data_instruction(const Instruction& instruction, IRProgram& ir_program) {
    IRDataRecord record;
    record.section = current_section_;
    record.directive = instruction.mnemonic;
    record.line = instruction.line;
    record.column = instruction.column;

    for (const auto& operand : instruction.operands) {
        record.values.push_back(lower_expression(*operand));
    }

    const uint64_t logical_offset = current_offset();
    ir_program.data_records.push_back(std::move(record));
    record_data_record_relocations(ir_program.data_records.back(), ir_program, logical_offset);
    current_offset() += estimate_data_record_size(ir_program.data_records.back());
}

IROperand LoweringContext::lower_expression(const Expression& expression) {
    switch (expression.type) {
        case ASTNodeType::REGISTER: {
            const auto& reg = static_cast<const RegisterExpression&>(expression);
            return {IROperandKind::Register, IRRegisterOperand{reg.name}};
        }
        case ASTNodeType::IMMEDIATE: {
            const auto& imm = static_cast<const ImmediateExpression&>(expression);
            return {IROperandKind::Immediate, IRImmediateOperand{imm.value}};
        }
        case ASTNodeType::FLOAT: {
            const auto& imm = static_cast<const FloatExpression&>(expression);
            return {IROperandKind::FloatImmediate, IRFloatImmediateOperand{imm.value}};
        }
        case ASTNodeType::IDENTIFIER: {
            const auto& ident = static_cast<const IdentifierExpression&>(expression);
            return {IROperandKind::Symbol, IRSymbolOperand{ident.name}};
        }
        case ASTNodeType::STRING_LITERAL: {
            const auto& literal = static_cast<const StringLiteralExpression&>(expression);
            return {IROperandKind::Symbol, IRSymbolOperand{literal.value}};
        }
        case ASTNodeType::ST_REGISTER: {
            const auto& st = static_cast<const STRegisterExpression&>(expression);
            return {IROperandKind::STRegister, IRSTRegisterOperand{st.index}};
        }
        case ASTNodeType::MEMORY_REF: {
            const auto& mem = static_cast<const MemoryReferenceExpression&>(expression);
            IRMemoryOperand lowered_mem;
            lowered_mem.scale = mem.scale;
            if (mem.size_hint_bits != 0) {
                lowered_mem.width_bits = mem.size_hint_bits;
            }

            if (mem.base) {
                if (auto reg = dynamic_cast<const RegisterExpression*>(mem.base.get())) {
                    lowered_mem.base = reg->name;
                } else if (auto imm = dynamic_cast<const ImmediateExpression*>(mem.base.get())) {
                    lowered_mem.displacement += imm->value;
                } else if (auto ident = dynamic_cast<const IdentifierExpression*>(mem.base.get())) {
                    lowered_mem.symbol = ident->name;
                }
            }

            if (mem.index) {
                if (auto reg = dynamic_cast<const RegisterExpression*>(mem.index.get())) {
                    lowered_mem.index = reg->name;
                }
            }

            const Expression* displacement = mem.displacement ? mem.displacement.get() : mem.offset.get();
            if (displacement) {
                if (auto imm = dynamic_cast<const ImmediateExpression*>(displacement)) {
                    lowered_mem.displacement += imm->value;
                } else if (auto ident = dynamic_cast<const IdentifierExpression*>(displacement)) {
                    lowered_mem.symbol = ident->name;
                }
            }

            if (mem.symbol) {
                lowered_mem.symbol = mem.symbol->name;
            }

            return {IROperandKind::Memory, lowered_mem};
        }
        case ASTNodeType::EXPRESSION: {
            const auto& binary = static_cast<const BinaryExpression&>(expression);
            IROperand left = lower_expression(*binary.left);
            IROperand right = lower_expression(*binary.right);
            bool ls = left.kind == IROperandKind::Symbol, rs = right.kind == IROperandKind::Symbol;
            bool li = left.kind == IROperandKind::Immediate, ri = right.kind == IROperandKind::Immediate;
            
            if ((ls && ri) || (rs && li)) {
                IROperand result; result.kind = IROperandKind::Symbol;
                result.value = ls ? std::get<IRSymbolOperand>(left.value) : std::get<IRSymbolOperand>(right.value);
                int64_t av = li ? std::get<IRImmediateOperand>(left.value).value : std::get<IRImmediateOperand>(right.value).value;
                result.reloc_addend = (binary.op == '+' && rs) ? -static_cast<int32_t>(av) : (binary.op == '-') ? -static_cast<int32_t>(av) : static_cast<int32_t>(av);
                return result;
            }
            if (li && ri) {
                int64_t lv = std::get<IRImmediateOperand>(left.value).value;
                int64_t rv = std::get<IRImmediateOperand>(right.value).value;
                switch (binary.op) { case '+': return {IROperandKind::Immediate, IRImmediateOperand{lv+rv}}; case '-': return {IROperandKind::Immediate, IRImmediateOperand{lv-rv}}; default: break; }
            }
            throw std::runtime_error("Unsupported expression in lowering");
        }
        default:
            throw std::runtime_error("Unsupported expression type in lowering");
    }
}

void LoweringContext::record_instruction_relocations(const IRInstruction& instruction, IRProgram& ir_program, uint64_t offset) {
    for (size_t operand_index = 0; operand_index < instruction.operands.size(); ++operand_index) {
        maybe_record_operand_relocation(instruction.operands[operand_index], instruction.section, offset, operand_index, ir_program);
    }
}

void LoweringContext::record_data_record_relocations(const IRDataRecord& record, IRProgram& ir_program, uint64_t offset) {
    // .string, .asciz, and DB directives use Symbol operands for inline string data,
    // not actual symbol references — skip relocation recording for them.
    if (record.directive == ".string" || record.directive == ".asciz" || record.directive == "DB") {
        return;
    }
    for (size_t operand_index = 0; operand_index < record.values.size(); ++operand_index) {
        maybe_record_operand_relocation(record.values[operand_index], record.section, offset, operand_index, ir_program);
    }
}

void LoweringContext::maybe_record_operand_relocation(const IROperand& operand, IRSectionKind section, uint64_t offset, size_t operand_index, IRProgram& ir_program) {
    IRRelocation relocation;
    relocation.section = section;
    relocation.offset = offset;
    relocation.operand_index = operand_index;

    if (operand.kind == IROperandKind::Symbol) {
        const auto& symbol = std::get<IRSymbolOperand>(operand.value);
        relocation.symbol = symbol.name;
        ir_program.relocations.push_back(std::move(relocation));
        return;
    }

    if (operand.kind != IROperandKind::Memory) {
        return;
    }

    const auto& memory = std::get<IRMemoryOperand>(operand.value);
    if (!memory.symbol) {
        return;
    }

    relocation.symbol = *memory.symbol;
    relocation.addend = memory.displacement;
    if (memory.width_bits && *memory.width_bits == 64) {
        relocation.kind = IRRelocationKind::Absolute64;
    }
    ir_program.relocations.push_back(std::move(relocation));
}

IRSectionKind LoweringContext::parse_section_name(const std::string& name) const {
    const std::string normalized = normalize_section_name(name);
    if (normalized == "text") {
        return IRSectionKind::Text;
    }
    if (normalized == "data") {
        return IRSectionKind::Data;
    }
    if (normalized == "bss") {
        return IRSectionKind::Bss;
    }
    if (normalized == "rodata") {
        return IRSectionKind::Rodata;
    }
    return IRSectionKind::Custom;
}

bool LoweringContext::is_data_directive(const std::string& name) const {
    return name == ".dw" || name == ".dd" || name == ".dq" ||
           name == ".string" || name == ".asciz" ||
           name == ".resb" || name == "RESB" || name == ".bss" ||
           name == ".zero" ||
           name == ".byte" || name == ".word" || name == ".long" || name == ".quad";
}

bool LoweringContext::is_data_instruction(const std::string& mnemonic) const {
    return mnemonic == "DB" || mnemonic == "RESB";
}

uint64_t& LoweringContext::current_offset() {
    switch (current_section_) {
        case IRSectionKind::Text: return text_offset_;
        case IRSectionKind::Data: return data_offset_;
        case IRSectionKind::Bss: return bss_offset_;
        case IRSectionKind::Rodata: return rodata_offset_;
        case IRSectionKind::Custom: return custom_offset_;
    }
    return text_offset_;
}

const uint64_t& LoweringContext::current_offset() const {
    switch (current_section_) {
        case IRSectionKind::Text: return text_offset_;
        case IRSectionKind::Data: return data_offset_;
        case IRSectionKind::Bss: return bss_offset_;
        case IRSectionKind::Rodata: return rodata_offset_;
        case IRSectionKind::Custom: return custom_offset_;
    }
    return text_offset_;
}

size_t LoweringContext::estimate_data_record_size(const IRDataRecord& record) const {
    if (record.directive == ".dw") {
        return record.values.size() * 2;
    }
    if (record.directive == ".dd") {
        return record.values.size() * 4;
    }
    if (record.directive == ".dq") {
        return record.values.size() * 8;
    }
    if (record.directive == ".string" || record.directive == ".asciz") {
        size_t total = 0;
        for (const auto& value : record.values) {
            if (value.kind == IROperandKind::Symbol) {
                total += std::get<IRSymbolOperand>(value.value).name.size() + 1;
            } else {
                total += 1;
            }
        }
        return total;
    }
    if (record.directive == ".resb" || record.directive == "RESB" || record.directive == ".bss" ||
        record.directive == ".zero") {
        if (record.values.empty() || record.values.front().kind != IROperandKind::Immediate) {
            return 0;
        }
        return static_cast<size_t>(std::max<int64_t>(0, std::get<IRImmediateOperand>(record.values.front().value).value));
    }
    if (record.directive == "DB") {
        size_t total = 0;
        for (const auto& value : record.values) {
            if (value.kind == IROperandKind::Symbol) {
                total += std::get<IRSymbolOperand>(value.value).name.size();
            } else {
                total += 1;
            }
        }
        return total;
    }
    return record.values.size();
}

IRProgram lower_program(const Program& program) {
    LoweringContext context;
    return context.lower_program(program);
}

} // namespace Assembler