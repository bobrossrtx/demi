#include "ir_generator.hpp"
#include <sstream>
#include <iostream>

namespace DemiLanguage {

// Demi VM opcodes (match opcodes.hpp)
enum : uint8_t {
    OP_NOP = 0x00,
    OP_LOAD_IMM = 0x01,
    OP_ADD = 0x02,
    OP_SUB = 0x03,
    OP_MOV = 0x04,
    OP_JMP = 0x05,
    OP_LOAD = 0x06,
    OP_STORE = 0x07,
    OP_PUSH = 0x08,
    OP_POP = 0x09,
    OP_CMP = 0x0A,
    OP_JZ = 0x0B,
    OP_JNZ = 0x0C,
    OP_JS = 0x0D,
    OP_JNS = 0x0E,
    OP_JC = 0x0F,
    OP_JNC = 0x22,
    OP_JG = 0x25,
    OP_JL = 0x26,
    OP_JGE = 0x27,
    OP_JLE = 0x28,
    OP_CALL = 0x1A,
    OP_RET = 0x1B,
    OP_MUL = 0x10,
    OP_DIV = 0x11,
    OP_INC = 0x12,
    OP_DEC = 0x13,
    OP_AND = 0x14,
    OP_OR = 0x15,
    OP_XOR = 0x16,
    OP_NOT = 0x17,
    OP_SHL = 0x18,
    OP_SHR = 0x19,
    OP_HALT = 0xFF,
};

IRGenerator::IRGenerator() {}

void IRGenerator::emit_byte(uint8_t b) { code_.push_back(b); }
void IRGenerator::emit_opcode(uint8_t op) { emit_byte(op); }

void IRGenerator::emit_u32(uint32_t val) {
    emit_byte(val & 0xFF);
    emit_byte((val >> 8) & 0xFF);
    emit_byte((val >> 16) & 0xFF);
    emit_byte((val >> 24) & 0xFF);
}

void IRGenerator::emit_load_imm(int reg, uint32_t val) {
    emit_opcode(OP_LOAD_IMM);
    emit_byte(static_cast<uint8_t>(reg));
    emit_u32(val);
}

void IRGenerator::emit_label(const std::string& name) {
    symbols_[name] = static_cast<uint32_t>(code_.size());
}

void IRGenerator::patch_jump(uint32_t jump_offset, uint32_t target_offset) {
    code_[jump_offset + 1] = target_offset & 0xFF;
    code_[jump_offset + 2] = (target_offset >> 8) & 0xFF;
    code_[jump_offset + 3] = (target_offset >> 16) & 0xFF;
    code_[jump_offset + 4] = (target_offset >> 24) & 0xFF;
}

int IRGenerator::alloc_local(const std::string& name) {
    int reg = next_local_reg_++;
    var_regs_[name] = reg;
    return reg;
}

int IRGenerator::get_var_reg(const std::string& name) {
    auto it = var_regs_.find(name);
    return (it != var_regs_.end()) ? it->second : -1;
}

// === Main generation ===

CodegenResult IRGenerator::generate(const DemiModule& module) {
    code_.clear();
    symbols_.clear();
    var_regs_.clear();
    next_local_reg_ = 8;
    
    // Emit entry trampoline: CALL main, then HALT
    emit_opcode(OP_CALL);
    emit_u32(0); // placeholder — patched later
    emit_opcode(OP_HALT);
    
    // Generate all functions
    for (auto& func : module.functions) {
        generate_function(*func);
    }
    
    // Patch entry CALL to main
    uint32_t entry_call_offset = 0;  // CALL is at offset 0
    auto main_it = symbols_.find("main");
    if (main_it != symbols_.end()) {
        patch_jump(entry_call_offset, main_it->second);
    }
    
    CodegenResult result;
    result.bytecode = std::move(code_);
    result.entry_label = "main";
    result.symbol_offsets = std::move(symbols_);
    return result;
}

// === Function generation ===

void IRGenerator::generate_function(const DemiFunction& func) {
    current_func_ = func.name;
    func_start_offset_ = static_cast<uint32_t>(code_.size());
    emit_label(func.name);
    var_regs_.clear();
    next_local_reg_ = 8;
    
    // Map parameters to local registers (not R0-R7, which are used for returns/temps)
    // Prologue: PUSH R5, MOV R5, R4, then save params to locals
    emit_opcode(OP_PUSH); emit_byte(5);
    emit_opcode(OP_MOV); emit_byte(5); emit_byte(4);
    
    for (size_t i = 0; i < func.params.size() && i < 8; i++) {
        int local_reg = next_local_reg_++;
        var_regs_[func.params[i].first] = local_reg;
        // MOV local_reg, R(i) — save param from call reg to local
        emit_opcode(OP_MOV); emit_byte(static_cast<uint8_t>(local_reg)); emit_byte(static_cast<uint8_t>(i));
    }
    
    // Generate body
    for (auto& stmt : func.body) {
        generate_stmt(*stmt);
    }
    
    // Function epilogue: POP R5, RET
    emit_opcode(OP_POP); emit_byte(5);
    emit_opcode(OP_RET);
}

// === Statement generation ===

void IRGenerator::generate_stmt(const DemiStmt& stmt) {
    switch (stmt.kind) {
        case StmtKind::Let: {
            int reg = alloc_local(stmt.var_name);
            generate_expr(*stmt.var_init, reg);
            break;
        }
        case StmtKind::Return: {
            if (stmt.return_expr) {
                generate_expr(*stmt.return_expr, 0); // return value in R0
            }
            // Epilogue: POP R5, RET
            emit_opcode(OP_POP); emit_byte(5);
            emit_opcode(OP_RET);
            break;
        }
        case StmtKind::Expr: {
            if (stmt.return_expr) {
                // Evaluate expression into a temp register
                int tmp = next_local_reg_;
                generate_expr(*stmt.return_expr, tmp);
            }
            break;
        }
        case StmtKind::Block: {
            for (auto& s : stmt.body) generate_stmt(*s);
            break;
        }
        case StmtKind::Assign: {
            if (stmt.assign_target && stmt.assign_value) {
                int reg = get_var_reg(stmt.assign_target->literal);
                if (reg >= 0) {
                    generate_expr(*stmt.assign_value, reg);
                }
            }
            break;
        }
        case StmtKind::If: {
            // condition → CMP with 0, JZ to else/end
            if (stmt.for_cond) {
                int cond_reg = next_local_reg_++;
                generate_expr(*stmt.for_cond, cond_reg);
                int zero_reg = next_local_reg_++;
                emit_load_imm(zero_reg, 0);
                emit_opcode(OP_CMP); emit_byte(static_cast<uint8_t>(cond_reg)); emit_byte(static_cast<uint8_t>(zero_reg));
                // JZ to skip past then-block
                uint32_t jz_offset = static_cast<uint32_t>(code_.size());
                emit_opcode(OP_JZ);
                emit_u32(0); // placeholder
                
                // then-block
                if (!stmt.body.empty()) generate_stmt(*stmt.body[0]);
                
                // else-block if present
                uint32_t jmp_over_else = static_cast<uint32_t>(code_.size());
                if (stmt.body.size() > 1) {
                    emit_opcode(OP_JMP);
                    emit_u32(0); // placeholder JMP past else
                }
                
                // patch JZ to here (after then)
                uint32_t after_then = static_cast<uint32_t>(code_.size());
                patch_jump(jz_offset, after_then);
                
                // else/elif blocks
                for (size_t i = 1; i < stmt.body.size(); i++) {
                    generate_stmt(*stmt.body[i]);
                }
                
                if (stmt.body.size() > 1) {
                    patch_jump(jmp_over_else, static_cast<uint32_t>(code_.size()));
                }
            }
            break;
        }
        case StmtKind::While: {
            uint32_t loop_start = static_cast<uint32_t>(code_.size());
            if (stmt.for_cond) {
                int cond_reg = next_local_reg_++;
                generate_expr(*stmt.for_cond, cond_reg);
                int zero_reg = next_local_reg_++;
                emit_load_imm(zero_reg, 0);
                emit_opcode(OP_CMP); emit_byte(static_cast<uint8_t>(cond_reg)); emit_byte(static_cast<uint8_t>(zero_reg));
                uint32_t jz_exit = static_cast<uint32_t>(code_.size());
                emit_opcode(OP_JZ);
                emit_u32(0);
            
                // body
                for (auto& b : stmt.body) generate_stmt(*b);
            
                // loop back
                emit_opcode(OP_JMP);
                emit_u32(loop_start);
            
                patch_jump(jz_exit, static_cast<uint32_t>(code_.size()));
            }
            break;
        }
        case StmtKind::For: {
            // For now, skip complex for-loop codegen (needs scope handling)
            break;
        }
        default: break;
    }
}

// === Expression generation ===

void IRGenerator::generate_expr(const DemiExpr& expr, int dest_reg) {
    switch (expr.kind) {
        case DemiExpr::Kind::LiteralString: {
            // Write string to fixed memory using STORE+reg
            static int str_heap = 0xC0;
            int addr = str_heap;
            int len = static_cast<int>(expr.literal.size());
            
            // Store each char: LOAD_IMM temp, char; STORE temp, addr+i
            for (int i = 0; i < len; i++) {
                int char_reg = next_local_reg_++;
                emit_load_imm(char_reg, static_cast<uint8_t>(expr.literal[i]));
                uint32_t a = static_cast<uint32_t>(addr + i);
                emit_opcode(0x07); emit_byte(static_cast<uint8_t>(char_reg));
                emit_byte(a & 0xFF); emit_byte((a>>8)&0xFF); emit_byte((a>>16)&0xFF); emit_byte((a>>24)&0xFF);
            }
            // Null terminator
            int nul_reg = next_local_reg_++;
            emit_load_imm(nul_reg, 0);
            uint32_t null_addr = static_cast<uint32_t>(addr + len);
            emit_opcode(0x07); emit_byte(static_cast<uint8_t>(nul_reg));
            emit_byte(null_addr & 0xFF); emit_byte((null_addr>>8)&0xFF); emit_byte((null_addr>>16)&0xFF); emit_byte((null_addr>>24)&0xFF);
            
            emit_load_imm(dest_reg, static_cast<uint32_t>(addr));
            str_heap += len + 1;
            break;
        }
        case DemiExpr::Kind::LiteralInt:
            emit_load_imm(dest_reg, static_cast<uint32_t>(expr.int_value));
            break;
        
        case DemiExpr::Kind::LiteralFloat: {
            // Store float as 32-bit int pattern
            union { float f; uint32_t i; } u;
            u.f = static_cast<float>(expr.float_value);
            emit_load_imm(dest_reg, u.i);
            break;
        }
        
        case DemiExpr::Kind::LiteralBool:
            emit_load_imm(dest_reg, expr.bool_value ? 1 : 0);
            break;
        
        case DemiExpr::Kind::Identifier: {
            int src_reg = get_var_reg(expr.literal);
            if (src_reg >= 0) {
                emit_opcode(OP_MOV); emit_byte(static_cast<uint8_t>(dest_reg)); emit_byte(static_cast<uint8_t>(src_reg));
            }
            break;
        }
        
        case DemiExpr::Kind::Binary: {
            // Evaluate left into temp (not dest_reg — CALL clobbers R0)
            int left_reg = next_local_reg_++;
            generate_expr(*expr.left, left_reg);
            // Evaluate right into temp reg
            int right_reg = next_local_reg_++;
            generate_expr(*expr.right, right_reg);
            
            // Emit binary op into left_reg, then copy to dest_reg
            uint8_t op = OP_ADD;
            switch (expr.bin_op) {
                case BinaryOp::Add: op = OP_ADD; break;
                case BinaryOp::Sub: op = OP_SUB; break;
                case BinaryOp::Mul: op = OP_MUL; break;
                case BinaryOp::Div: op = OP_DIV; break;
                case BinaryOp::Eq: case BinaryOp::Ne:
                case BinaryOp::Lt: case BinaryOp::Gt:
                case BinaryOp::Le: case BinaryOp::Ge: {
                    // CMP sets flags, then conditional jump
                    emit_opcode(OP_CMP); emit_byte(static_cast<uint8_t>(left_reg)); emit_byte(static_cast<uint8_t>(right_reg));
                    
                    uint8_t jmp_op = OP_JZ;
                    switch (expr.bin_op) {
                        case BinaryOp::Eq: jmp_op = OP_JZ; break;
                        case BinaryOp::Ne: jmp_op = OP_JNZ; break;
                        case BinaryOp::Lt: jmp_op = 0x26; break; // JL
                        case BinaryOp::Gt: jmp_op = 0x25; break; // JG
                        case BinaryOp::Le: jmp_op = 0x28; break; // JLE
                        case BinaryOp::Ge: jmp_op = 0x27; break; // JGE
                        default: break;
                    }
                    
                    // Jump to true path if condition met
                    uint32_t jmp_offset = static_cast<uint32_t>(code_.size());
                    emit_opcode(jmp_op); emit_u32(0); // placeholder
                    
                    // False path: dest = 0
                    emit_load_imm(static_cast<int>(dest_reg), 0);
                    uint32_t skip_true = static_cast<uint32_t>(code_.size());
                    emit_opcode(OP_JMP); emit_u32(0);
                    
                    // True path: dest = 1
                    uint32_t true_target = static_cast<uint32_t>(code_.size());
                    emit_load_imm(static_cast<int>(dest_reg), 1);
                    
                    // Patch jumps
                    patch_jump(jmp_offset, true_target);
                    patch_jump(skip_true, static_cast<uint32_t>(code_.size()));
                    return;
                }
                case BinaryOp::And: case BinaryOp::Or: op = OP_AND; break; // simplified
                case BinaryOp::BitAnd: op = OP_AND; break;
                case BinaryOp::BitOr: op = OP_OR; break;
                case BinaryOp::BitXor: op = OP_XOR; break;
                case BinaryOp::Shl: op = OP_SHL; break;
                case BinaryOp::Shr: op = OP_SHR; break;
                case BinaryOp::Mod: op = OP_DIV; break; // simplified
            }
            emit_opcode(op); emit_byte(static_cast<uint8_t>(left_reg)); emit_byte(static_cast<uint8_t>(right_reg));
            // Copy result to destination
            if (left_reg != dest_reg) {
                emit_opcode(OP_MOV); emit_byte(static_cast<uint8_t>(dest_reg)); emit_byte(static_cast<uint8_t>(left_reg));
            }
            break;
        }
        
        case DemiExpr::Kind::Call: {
            // Handle direct function calls and method calls
            std::string func_name;
            if (expr.left) {
                if (expr.left->kind == DemiExpr::Kind::Identifier) {
                    func_name = expr.left->literal;
                } else if (expr.left->kind == DemiExpr::Kind::Member) {
                    // Method call like console.println — map to builtins
                    if (expr.left->left && expr.left->left->kind == DemiExpr::Kind::Identifier) {
                        std::string obj = expr.left->left->literal;
                        std::string method = expr.left->literal;
                        if (obj == "console" && method == "println") {
                            // print string arg + newline
                            if (!expr.args.empty()) {
                                generate_expr(*expr.args[0], 0); // arg into R0
                            }
                            emit_opcode(0x39); emit_byte(0); emit_byte(1); // OUTSTR R0, port 1
                            // newline
                            int nl_reg = next_local_reg_++;
                            emit_load_imm(nl_reg, '\n');
                            emit_opcode(0x31); emit_byte(static_cast<uint8_t>(nl_reg)); emit_u32(1); // OUT R_nl, 1
                            emit_load_imm(dest_reg, 0);
                            break;
                        }
                        if (obj == "console" && method == "print") {
                            if (!expr.args.empty()) {
                                // String arg: generate code to get string address in R0
                                // For string literals, store string data inline and return address
                                int str_reg = next_local_reg_++;
                                generate_expr(*expr.args[0], str_reg);
                                emit_opcode(OP_MOV); emit_byte(0); emit_byte(static_cast<uint8_t>(str_reg));
                            }
                            emit_opcode(0x39); emit_byte(0); emit_byte(1); // OUTSTR R0, port 1
                            emit_load_imm(dest_reg, 0);
                            break;
                        }
                        if (obj == "console" && method == "print_i32") {
                            if (!expr.args.empty()) {
                                if (expr.args[0]->kind == DemiExpr::Kind::Identifier) {
                                    int var_reg = get_var_reg(expr.args[0]->literal);
                                    if (var_reg >= 0) {
                                        emit_opcode(0x31); emit_byte(static_cast<uint8_t>(var_reg)); emit_byte(1);
                                    } else {
                                        generate_expr(*expr.args[0], 0);
                                        emit_opcode(0x31); emit_byte(0); emit_byte(1);
                                    }
                                } else {
                                    generate_expr(*expr.args[0], 0);
                                    emit_opcode(0x31); emit_byte(0); emit_byte(1);
                                }
                            } else {
                                emit_opcode(0x31); emit_byte(0); emit_byte(1);
                            }
                            emit_load_imm(dest_reg, 0);
                            break;
                        }
                        if (obj == "console" && method == "print_hex") {
                            // Print 32-bit integer as 8 hex digits
                            // SHR uses 8-bit immediate shift (not register), so hardcode each shift
                            if (!expr.args.empty()) {
                                int val_reg = next_local_reg_++;
                                generate_expr(*expr.args[0], val_reg);
                                int nibble_reg = next_local_reg_++;
                                int mask10_reg = next_local_reg_++;
                                int ascA_reg = next_local_reg_++;
                                int asc0_reg = next_local_reg_++;
                                int result_reg = next_local_reg_++;
                                emit_load_imm(mask10_reg, 10);
                                emit_load_imm(ascA_reg, 'A');
                                emit_load_imm(asc0_reg, '0');
                                int shifts[] = {28, 24, 20, 16, 12, 8, 4, 0};
                                for (int shift : shifts) {
                                    // Copy val to nibble_reg
                                    emit_opcode(OP_MOV); emit_byte(static_cast<uint8_t>(nibble_reg)); emit_byte(static_cast<uint8_t>(val_reg));
                                    // SHR nibble, shift (immediate 8-bit)
                                    emit_opcode(OP_SHR); emit_byte(static_cast<uint8_t>(nibble_reg)); emit_byte(static_cast<uint8_t>(shift));
                                    // AND nibble, 0xF
                                    emit_load_imm(result_reg, 0xF);
                                    emit_opcode(OP_AND); emit_byte(static_cast<uint8_t>(nibble_reg)); emit_byte(static_cast<uint8_t>(result_reg));
                                    // Convert to ASCII
                                    emit_opcode(OP_CMP); emit_byte(static_cast<uint8_t>(nibble_reg)); emit_byte(static_cast<uint8_t>(mask10_reg));
                                    uint32_t jl_off = static_cast<uint32_t>(code_.size());
                                    emit_opcode(OP_JL); emit_u32(0);
                                    // nibble >= 10
                                    emit_opcode(OP_MOV); emit_byte(static_cast<uint8_t>(result_reg)); emit_byte(static_cast<uint8_t>(nibble_reg));
                                    emit_opcode(OP_SUB); emit_byte(static_cast<uint8_t>(result_reg)); emit_byte(static_cast<uint8_t>(mask10_reg));
                                    emit_opcode(OP_ADD); emit_byte(static_cast<uint8_t>(result_reg)); emit_byte(static_cast<uint8_t>(ascA_reg));
                                    uint32_t jmp_off = static_cast<uint32_t>(code_.size());
                                    emit_opcode(OP_JMP); emit_u32(0);
                                    // nibble < 10
                                    uint32_t lt10_target = static_cast<uint32_t>(code_.size());
                                    patch_jump(jl_off, lt10_target);
                                    emit_opcode(OP_MOV); emit_byte(static_cast<uint8_t>(result_reg)); emit_byte(static_cast<uint8_t>(nibble_reg));
                                    emit_opcode(OP_ADD); emit_byte(static_cast<uint8_t>(result_reg)); emit_byte(static_cast<uint8_t>(asc0_reg));
                                    uint32_t done_target = static_cast<uint32_t>(code_.size());
                                    patch_jump(jmp_off, done_target);
                                    // OUT result, 1
                                    emit_opcode(0x31); emit_byte(static_cast<uint8_t>(result_reg)); emit_byte(1);
                                }
                            }
                            emit_load_imm(dest_reg, 0);
                            break;
                        }
                        if (obj == "math" && method == "abs") {
                            int val_reg = next_local_reg_++;
                            generate_expr(*expr.args[0], val_reg);
                            // if val >= 0, skip; else val = 0 - val (SUB from zero)
                            int zero_reg = next_local_reg_++;
                            emit_load_imm(zero_reg, 0);
                            emit_opcode(OP_CMP); emit_byte(static_cast<uint8_t>(val_reg)); emit_byte(static_cast<uint8_t>(zero_reg));
                            uint32_t jge_off = static_cast<uint32_t>(code_.size());
                            emit_opcode(OP_JGE); emit_u32(0);
                            // negate: result = 0 - val  (load 0 into temp, SUB val from it)
                            int tmp_reg = next_local_reg_++;
                            emit_load_imm(tmp_reg, 0);
                            emit_opcode(OP_SUB); emit_byte(static_cast<uint8_t>(tmp_reg)); emit_byte(static_cast<uint8_t>(val_reg));
                            emit_opcode(OP_MOV); emit_byte(static_cast<uint8_t>(val_reg)); emit_byte(static_cast<uint8_t>(tmp_reg));
                            uint32_t skip_off = static_cast<uint32_t>(code_.size());
                            patch_jump(jge_off, skip_off);
                            emit_opcode(OP_MOV); emit_byte(static_cast<uint8_t>(dest_reg)); emit_byte(static_cast<uint8_t>(val_reg));
                            break;
                        }
                        if (obj == "math" && method == "min") {
                            int a_reg = next_local_reg_++;
                            int b_reg = next_local_reg_++;
                            generate_expr(*expr.args[0], a_reg);
                            generate_expr(*expr.args[1], b_reg);
                            emit_opcode(OP_CMP); emit_byte(static_cast<uint8_t>(a_reg)); emit_byte(static_cast<uint8_t>(b_reg));
                            uint32_t jle_off = static_cast<uint32_t>(code_.size());
                            emit_opcode(OP_JLE); emit_u32(0);
                            emit_opcode(OP_MOV); emit_byte(static_cast<uint8_t>(a_reg)); emit_byte(static_cast<uint8_t>(b_reg));
                            uint32_t min_skip = static_cast<uint32_t>(code_.size());
                            patch_jump(jle_off, min_skip);
                            emit_opcode(OP_MOV); emit_byte(static_cast<uint8_t>(dest_reg)); emit_byte(static_cast<uint8_t>(a_reg));
                            break;
                        }
                        if (obj == "math" && method == "max") {
                            int a_reg = next_local_reg_++;
                            int b_reg = next_local_reg_++;
                            generate_expr(*expr.args[0], a_reg);
                            generate_expr(*expr.args[1], b_reg);
                            emit_opcode(OP_CMP); emit_byte(static_cast<uint8_t>(a_reg)); emit_byte(static_cast<uint8_t>(b_reg));
                            uint32_t jge_off = static_cast<uint32_t>(code_.size());
                            emit_opcode(OP_JGE); emit_u32(0);
                            emit_opcode(OP_MOV); emit_byte(static_cast<uint8_t>(a_reg)); emit_byte(static_cast<uint8_t>(b_reg));
                            uint32_t max_skip = static_cast<uint32_t>(code_.size());
                            patch_jump(jge_off, max_skip);
                            emit_opcode(OP_MOV); emit_byte(static_cast<uint8_t>(dest_reg)); emit_byte(static_cast<uint8_t>(a_reg));
                            break;
                        }
                    }
                    emit_load_imm(dest_reg, 0);
                    break;
                }
            }
            
            if (!func_name.empty()) {
                // Evaluate args into temp regs first (to avoid clobbering params)
                std::vector<int> arg_regs;
                for (size_t i = 0; i < expr.args.size() && i < 8; i++) {
                    int arg_reg = next_local_reg_++;
                    generate_expr(*expr.args[i], arg_reg);
                    arg_regs.push_back(arg_reg);
                }
                // Copy temps to parameter registers R0-R7
                for (size_t i = 0; i < arg_regs.size(); i++) {
                    if (arg_regs[i] != static_cast<int>(i)) {
                        emit_opcode(OP_MOV); emit_byte(static_cast<uint8_t>(i)); emit_byte(static_cast<uint8_t>(arg_regs[i]));
                    }
                }
                // CALL to function label
                func_name = expr.left->literal;
                auto it = symbols_.find(func_name);
                if (it != symbols_.end()) {
                    emit_opcode(OP_CALL);
                    emit_u32(it->second);
                    // Result is in R0 — move to dest_reg
                    if (static_cast<int>(dest_reg) != 0) {
                        emit_opcode(OP_MOV); emit_byte(static_cast<uint8_t>(dest_reg)); emit_byte(0);
                    }
                }
            }
            break;
        }
        
        case DemiExpr::Kind::Member: {
            // p.x: load field from struct
            // Compute address: base + offset
            // For now, use simple field name→offset mapping
            int base_reg = next_local_reg_++;
            generate_expr(*expr.left, base_reg);
            
            // Compute field offset (simplified: just use field name hash for demo)
            int offset = 0;
            if (expr.literal == "y") offset = 4;
            else if (expr.literal == "z") offset = 8;
            else if (expr.literal == "w") offset = 12;
            
            if (offset > 0) {
                int offset_reg = next_local_reg_++;
                emit_load_imm(offset_reg, static_cast<uint32_t>(offset));
                emit_opcode(OP_ADD); emit_byte(static_cast<uint8_t>(base_reg)); emit_byte(static_cast<uint8_t>(offset_reg));
            }
            // Load from computed address
            emit_opcode(0x41); emit_byte(static_cast<uint8_t>(dest_reg)); emit_byte(static_cast<uint8_t>(base_reg)); // LOADR
            break;
        }
        
        case DemiExpr::Kind::StructLiteral: {
            // Allocate struct in heap (bump pointer)
            int heap_ptr = 0x80;
            int base_addr = heap_ptr;
            int struct_size = static_cast<int>(expr.fields.size() * 4);
            heap_ptr += struct_size;
            
            // Store each field using direct STORE (opcode 0x07)
            for (size_t i = 0; i < expr.fields.size(); i++) {
                int field_off = static_cast<int>(i * 4);
                int val_reg = next_local_reg_++;
                generate_expr(*expr.fields[i].second, val_reg);
                
                // Store 4 bytes using direct addressing
                for (int b = 0; b < 4; b++) {
                    uint32_t addr = static_cast<uint32_t>(base_addr + field_off + b);
                    // Get byte b: shift right by b*8, then STORE LSB
                    int byte_reg = val_reg;
                    if (b > 0) {
                        byte_reg = next_local_reg_++;
                        emit_opcode(OP_MOV); emit_byte(static_cast<uint8_t>(byte_reg)); emit_byte(static_cast<uint8_t>(val_reg));
                        emit_opcode(0x19); emit_byte(static_cast<uint8_t>(byte_reg)); emit_byte(static_cast<uint8_t>(b * 8)); // SHR
                    }
                    // STORE: opcode(0x07) + reg + 4-byte addr
                    emit_opcode(0x07); emit_byte(static_cast<uint8_t>(byte_reg));
                    emit_byte(addr & 0xFF); emit_byte((addr>>8) & 0xFF);
                    emit_byte((addr>>16) & 0xFF); emit_byte((addr>>24) & 0xFF);
                }
            }
            
            // Return 0 in dest_reg (pointer is implicit in codegen for now)
            emit_load_imm(dest_reg, 0);
            break;
        }
        
        default:
            emit_load_imm(dest_reg, 0);
            break;
    }
}

} // namespace DemiLanguage
