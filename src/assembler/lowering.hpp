#pragma once

#include "ast.hpp"
#include "ir.hpp"

namespace Assembler {

class LoweringContext {
public:
    IRProgram lower_program(const Program& program);

private:
    IRSectionKind current_section_ = IRSectionKind::Text;
    uint64_t text_offset_ = 0;
    uint64_t data_offset_ = 0;
    uint64_t bss_offset_ = 0;
    uint64_t rodata_offset_ = 0;
    uint64_t custom_offset_ = 0;

    void lower_statement(const Statement& statement, IRProgram& ir_program);
    void lower_label(const Label& label, IRProgram& ir_program);
    void lower_instruction(const Instruction& instruction, IRProgram& ir_program);
    void lower_directive(const Directive& directive, IRProgram& ir_program);
    void lower_data_instruction(const Instruction& instruction, IRProgram& ir_program);
    IROperand lower_expression(const Expression& expression);
    void record_instruction_relocations(const IRInstruction& instruction, IRProgram& ir_program, uint64_t offset);
    void record_data_record_relocations(const IRDataRecord& record, IRProgram& ir_program, uint64_t offset);
    void maybe_record_operand_relocation(const IROperand& operand, IRSectionKind section, uint64_t offset, size_t operand_index, IRProgram& ir_program);
    uint64_t& current_offset();
    const uint64_t& current_offset() const;
    size_t estimate_data_record_size(const IRDataRecord& record) const;
    bool is_data_instruction(const std::string& mnemonic) const;
    IRSectionKind parse_section_name(const std::string& name) const;
    bool is_data_directive(const std::string& name) const;
};

IRProgram lower_program(const Program& program);

} // namespace Assembler