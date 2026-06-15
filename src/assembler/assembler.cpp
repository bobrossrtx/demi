#include <iostream>
#include <ios>
#include <iomanip>
#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "assembler.hpp"
#include "lowering.hpp"
#include "opcodes.hpp"
#include "../engine/safe_memcpy.hpp"
#include "../debug/error_handler.hpp"
#include "../debug/debug_handler.hpp"
#include "../config.hpp"
#include "../../extern/fmt/include/fmt/format.h"

namespace Assembler {
uint32_t AssemblerEngine::db_next_addr = 0x100;

AssemblerEngine::AssemblerEngine() : current_address(0) {
    init_opcode_table();
    init_register_table();
    init_encoder_map();
}

AssemblerEngine::~AssemblerEngine() {
    DEBUG_INFO(Logging::DebugCategory::ASM_PARSING, "AssemblerEngine destructor called, clearing forward_refs with {} entries", forward_refs.size());
    forward_refs.clear();
}

void AssemblerEngine::init_opcode_table() {
    // Map mnemonics to opcodes based on DemiEngine CPU instruction set
    mnemonic_to_opcode["NOP"] = static_cast<uint8_t>(Opcode::NOP);
    mnemonic_to_opcode["LOAD_IMM"] = static_cast<uint8_t>(Opcode::LOAD_IMM);
    mnemonic_to_opcode["ADD"] = static_cast<uint8_t>(Opcode::ADD);
    mnemonic_to_opcode["SUB"] = static_cast<uint8_t>(Opcode::SUB);
    mnemonic_to_opcode["MOV"] = static_cast<uint8_t>(Opcode::MOV);
    mnemonic_to_opcode["JMP"] = static_cast<uint8_t>(Opcode::JMP);
    mnemonic_to_opcode["LOAD"] = static_cast<uint8_t>(Opcode::LOAD);
    mnemonic_to_opcode["LOADR"] = static_cast<uint8_t>(Opcode::LOADR);
    mnemonic_to_opcode["NOP"] = static_cast<uint8_t>(Opcode::NOP);
    mnemonic_to_opcode["SUB"] = static_cast<uint8_t>(Opcode::SUB);
    mnemonic_to_opcode["LOAD"] = static_cast<uint8_t>(Opcode::LOAD);
    mnemonic_to_opcode["STORER"] = static_cast<uint8_t>(Opcode::STORER);
    mnemonic_to_opcode["STORE"] = static_cast<uint8_t>(Opcode::STORE);
    mnemonic_to_opcode["PUSH"] = static_cast<uint8_t>(Opcode::PUSH);
    mnemonic_to_opcode["POP"] = static_cast<uint8_t>(Opcode::POP);
    mnemonic_to_opcode["CMP"] = static_cast<uint8_t>(Opcode::CMP);
    mnemonic_to_opcode["JZ"] = static_cast<uint8_t>(Opcode::JZ);
    mnemonic_to_opcode["JE"] = static_cast<uint8_t>(Opcode::JZ);
    mnemonic_to_opcode["JNZ"] = static_cast<uint8_t>(Opcode::JNZ);
    mnemonic_to_opcode["JNE"] = static_cast<uint8_t>(Opcode::JNZ);
    mnemonic_to_opcode["JS"] = static_cast<uint8_t>(Opcode::JS);
    mnemonic_to_opcode["JNS"] = static_cast<uint8_t>(Opcode::JNS);
    mnemonic_to_opcode["JC"] = static_cast<uint8_t>(Opcode::JC);
    mnemonic_to_opcode["JNC"] = static_cast<uint8_t>(Opcode::JNC);
    mnemonic_to_opcode["JO"] = static_cast<uint8_t>(Opcode::JO);
    mnemonic_to_opcode["JNO"] = static_cast<uint8_t>(Opcode::JNO);
    mnemonic_to_opcode["JG"] = static_cast<uint8_t>(Opcode::JG);
    mnemonic_to_opcode["JL"] = static_cast<uint8_t>(Opcode::JL);
    mnemonic_to_opcode["JGE"] = static_cast<uint8_t>(Opcode::JGE);
    mnemonic_to_opcode["JLE"] = static_cast<uint8_t>(Opcode::JLE);
    mnemonic_to_opcode["MUL"] = static_cast<uint8_t>(Opcode::MUL);
    mnemonic_to_opcode["DIV"] = static_cast<uint8_t>(Opcode::DIV);
    mnemonic_to_opcode["MOD"] = static_cast<uint8_t>(Opcode::MOD);
    mnemonic_to_opcode["INC"] = static_cast<uint8_t>(Opcode::INC);
    mnemonic_to_opcode["DEC"] = static_cast<uint8_t>(Opcode::DEC);
    mnemonic_to_opcode["AND"] = static_cast<uint8_t>(Opcode::AND);
    mnemonic_to_opcode["OR"] = static_cast<uint8_t>(Opcode::OR);
    mnemonic_to_opcode["XOR"] = static_cast<uint8_t>(Opcode::XOR);
    mnemonic_to_opcode["NOT"] = static_cast<uint8_t>(Opcode::NOT);
    mnemonic_to_opcode["SHL"] = static_cast<uint8_t>(Opcode::SHL);
    mnemonic_to_opcode["SHR"] = static_cast<uint8_t>(Opcode::SHR);
    mnemonic_to_opcode["CALL"] = static_cast<uint8_t>(Opcode::CALL);
    mnemonic_to_opcode["RET"] = static_cast<uint8_t>(Opcode::RET);
    mnemonic_to_opcode["PUSH_ARG"] = static_cast<uint8_t>(Opcode::PUSH_ARG);
    mnemonic_to_opcode["POP_ARG"] = static_cast<uint8_t>(Opcode::POP_ARG);
    mnemonic_to_opcode["PUSH_FLAG"] = static_cast<uint8_t>(Opcode::PUSH_FLAG);
    mnemonic_to_opcode["POP_FLAG"] = static_cast<uint8_t>(Opcode::POP_FLAG);
    mnemonic_to_opcode["LEA"] = static_cast<uint8_t>(Opcode::LEA);
    mnemonic_to_opcode["SWAP"] = static_cast<uint8_t>(Opcode::SWAP);
    mnemonic_to_opcode["IN"] = static_cast<uint8_t>(Opcode::IN);
    mnemonic_to_opcode["OUT"] = static_cast<uint8_t>(Opcode::OUT);
    mnemonic_to_opcode["INB"] = static_cast<uint8_t>(Opcode::INB);
    mnemonic_to_opcode["OUTB"] = static_cast<uint8_t>(Opcode::OUTB);
    mnemonic_to_opcode["INW"] = static_cast<uint8_t>(Opcode::INW);
    mnemonic_to_opcode["OUTW"] = static_cast<uint8_t>(Opcode::OUTW);
    mnemonic_to_opcode["INL"] = static_cast<uint8_t>(Opcode::INL);
    mnemonic_to_opcode["OUTL"] = static_cast<uint8_t>(Opcode::OUTL);
    mnemonic_to_opcode["INSTR"] = static_cast<uint8_t>(Opcode::INSTR);
    mnemonic_to_opcode["OUTSTR"] = static_cast<uint8_t>(Opcode::OUTSTR);
    mnemonic_to_opcode["DB"] = static_cast<uint8_t>(Opcode::DB);
    mnemonic_to_opcode["DEBUG"] = static_cast<uint8_t>(Opcode::DEBUG);
    
    // Interrupt operations
    mnemonic_to_opcode["INT"] = static_cast<uint8_t>(Opcode::INT);
    mnemonic_to_opcode["IRET"] = static_cast<uint8_t>(Opcode::IRET);
    mnemonic_to_opcode["CLI"] = static_cast<uint8_t>(Opcode::CLI);
    mnemonic_to_opcode["STI"] = static_cast<uint8_t>(Opcode::STI);
    
    mnemonic_to_opcode["HALT"] = static_cast<uint8_t>(Opcode::HALT);

    // Extended operations
    mnemonic_to_opcode["ADD64"] = static_cast<uint8_t>(Opcode::ADD64);
    mnemonic_to_opcode["SUB64"] = static_cast<uint8_t>(Opcode::SUB64);
    mnemonic_to_opcode["MOV64"] = static_cast<uint8_t>(Opcode::MOV64);
    mnemonic_to_opcode["LOAD_IMM64"] = static_cast<uint8_t>(Opcode::LOAD_IMM64);
    mnemonic_to_opcode["MUL64"] = static_cast<uint8_t>(Opcode::MUL64);
    mnemonic_to_opcode["DIV64"] = static_cast<uint8_t>(Opcode::DIV64);
    mnemonic_to_opcode["AND64"] = static_cast<uint8_t>(Opcode::AND64);
    mnemonic_to_opcode["OR64"] = static_cast<uint8_t>(Opcode::OR64);
    mnemonic_to_opcode["XOR64"] = static_cast<uint8_t>(Opcode::XOR64);
    mnemonic_to_opcode["SHL64"] = static_cast<uint8_t>(Opcode::SHL64);
    mnemonic_to_opcode["SHR64"] = static_cast<uint8_t>(Opcode::SHR64);
    mnemonic_to_opcode["CMP64"] = static_cast<uint8_t>(Opcode::CMP64);
    mnemonic_to_opcode["NOT64"] = static_cast<uint8_t>(Opcode::NOT64);
    mnemonic_to_opcode["INC64"] = static_cast<uint8_t>(Opcode::INC64);
    mnemonic_to_opcode["DEC64"] = static_cast<uint8_t>(Opcode::DEC64);
    mnemonic_to_opcode["MOD64"] = static_cast<uint8_t>(Opcode::MOD64);
    mnemonic_to_opcode["MOVEX"] = static_cast<uint8_t>(Opcode::MOVEX);
    mnemonic_to_opcode["ADDEX"] = static_cast<uint8_t>(Opcode::ADDEX);
    mnemonic_to_opcode["SUBEX"] = static_cast<uint8_t>(Opcode::SUBEX);
    mnemonic_to_opcode["MULEX"] = static_cast<uint8_t>(Opcode::MULEX);
    mnemonic_to_opcode["DIVEX"] = static_cast<uint8_t>(Opcode::DIVEX);
    mnemonic_to_opcode["CMPEX"] = static_cast<uint8_t>(Opcode::CMPEX);
    mnemonic_to_opcode["LOADEX"] = static_cast<uint8_t>(Opcode::LOADEX);
    mnemonic_to_opcode["STOREX"] = static_cast<uint8_t>(Opcode::STOREX);
    mnemonic_to_opcode["PUSHEX"] = static_cast<uint8_t>(Opcode::PUSHEX);
    mnemonic_to_opcode["POPEX"] = static_cast<uint8_t>(Opcode::POPEX);
    mnemonic_to_opcode["MODE32"] = static_cast<uint8_t>(Opcode::MODE32);
    mnemonic_to_opcode["MODE64"] = static_cast<uint8_t>(Opcode::MODE64);
    mnemonic_to_opcode["MODECMP"] = static_cast<uint8_t>(Opcode::MODECMP);

    // SSE-style SIMD Operations (0x80-0x99 range)
    mnemonic_to_opcode["MOVAPS"] = static_cast<uint8_t>(Opcode::MOVAPS);
    mnemonic_to_opcode["MOVUPS"] = static_cast<uint8_t>(Opcode::MOVUPS);
    mnemonic_to_opcode["ADDPS"] = static_cast<uint8_t>(Opcode::ADDPS);
    mnemonic_to_opcode["SUBPS"] = static_cast<uint8_t>(Opcode::SUBPS);
    mnemonic_to_opcode["MULPS"] = static_cast<uint8_t>(Opcode::MULPS);
    mnemonic_to_opcode["DIVPS"] = static_cast<uint8_t>(Opcode::DIVPS);
    mnemonic_to_opcode["SQRTPS"] = static_cast<uint8_t>(Opcode::SQRTPS);
    mnemonic_to_opcode["MAXPS"] = static_cast<uint8_t>(Opcode::MAXPS);
    mnemonic_to_opcode["MINPS"] = static_cast<uint8_t>(Opcode::MINPS);
    mnemonic_to_opcode["ANDPS"] = static_cast<uint8_t>(Opcode::ANDPS);
    mnemonic_to_opcode["ORPS"] = static_cast<uint8_t>(Opcode::ORPS);
    mnemonic_to_opcode["XORPS"] = static_cast<uint8_t>(Opcode::XORPS);
    mnemonic_to_opcode["CMPPS"] = static_cast<uint8_t>(Opcode::CMPPS);

    // Documentation alias (not a distinct opcode in engine)
    mnemonic_to_opcode["MOVPS"] = static_cast<uint8_t>(Opcode::MOVAPS);

    // Packed Double Operations (0x8D-0x99 range)
    mnemonic_to_opcode["MOVAPD"] = static_cast<uint8_t>(Opcode::MOVAPD);
    mnemonic_to_opcode["MOVUPD"] = static_cast<uint8_t>(Opcode::MOVUPD);
    mnemonic_to_opcode["ADDPD"] = static_cast<uint8_t>(Opcode::ADDPD);
    mnemonic_to_opcode["SUBPD"] = static_cast<uint8_t>(Opcode::SUBPD);
    mnemonic_to_opcode["MULPD"] = static_cast<uint8_t>(Opcode::MULPD);
    mnemonic_to_opcode["DIVPD"] = static_cast<uint8_t>(Opcode::DIVPD);
    mnemonic_to_opcode["SQRTPD"] = static_cast<uint8_t>(Opcode::SQRTPD);
    mnemonic_to_opcode["MAXPD"] = static_cast<uint8_t>(Opcode::MAXPD);
    mnemonic_to_opcode["MINPD"] = static_cast<uint8_t>(Opcode::MINPD);
    mnemonic_to_opcode["ANDPD"] = static_cast<uint8_t>(Opcode::ANDPD);
    mnemonic_to_opcode["ORPD"] = static_cast<uint8_t>(Opcode::ORPD);
    mnemonic_to_opcode["XORPD"] = static_cast<uint8_t>(Opcode::XORPD);
    mnemonic_to_opcode["CMPPD"] = static_cast<uint8_t>(Opcode::CMPPD);

    // FPU Operations
    mnemonic_to_opcode["FINIT"] = static_cast<uint8_t>(Opcode::FINIT);
    mnemonic_to_opcode["FLD"] = static_cast<uint8_t>(Opcode::FLD);
    mnemonic_to_opcode["FST"] = static_cast<uint8_t>(Opcode::FST);
    mnemonic_to_opcode["FSTP"] = static_cast<uint8_t>(Opcode::FSTP);
    mnemonic_to_opcode["FADD"] = static_cast<uint8_t>(Opcode::FADD);
    mnemonic_to_opcode["FSUB"] = static_cast<uint8_t>(Opcode::FSUB);
    mnemonic_to_opcode["FMUL"] = static_cast<uint8_t>(Opcode::FMUL);
    mnemonic_to_opcode["FDIV"] = static_cast<uint8_t>(Opcode::FDIV);
    mnemonic_to_opcode["FILD"] = static_cast<uint8_t>(Opcode::FILD);
    mnemonic_to_opcode["FIST"] = static_cast<uint8_t>(Opcode::FIST);
    mnemonic_to_opcode["FISTP"] = static_cast<uint8_t>(Opcode::FISTP);
    mnemonic_to_opcode["FABS"] = static_cast<uint8_t>(Opcode::FABS);
    mnemonic_to_opcode["FCHS"] = static_cast<uint8_t>(Opcode::FCHS);
    mnemonic_to_opcode["FSQRT"] = static_cast<uint8_t>(Opcode::FSQRT);
    mnemonic_to_opcode["FSIN"] = static_cast<uint8_t>(Opcode::FSIN);
    mnemonic_to_opcode["FCOS"] = static_cast<uint8_t>(Opcode::FCOS);
    mnemonic_to_opcode["FTAN"] = static_cast<uint8_t>(Opcode::FTAN);
    mnemonic_to_opcode["FCOMPP"] = static_cast<uint8_t>(Opcode::FCOMPP);
    mnemonic_to_opcode["FUCOMPP"] = static_cast<uint8_t>(Opcode::FUCOMPP);
    mnemonic_to_opcode["FCLEX"] = static_cast<uint8_t>(Opcode::FCLEX);
    mnemonic_to_opcode["FSTCW"] = static_cast<uint8_t>(Opcode::FSTCW);
    mnemonic_to_opcode["FLDCW"] = static_cast<uint8_t>(Opcode::FLDCW);
    mnemonic_to_opcode["FSTSW"] = static_cast<uint8_t>(Opcode::FSTSW);
    
    // Simple SIMD Operations (custom opcodes 0xD4-0xDF)
    mnemonic_to_opcode["VADD"] = 0xD4;
    mnemonic_to_opcode["VMUL"] = 0xD5;
    mnemonic_to_opcode["VDOT"] = 0xD6;
    mnemonic_to_opcode["VMAX"] = 0xD7;
    mnemonic_to_opcode["VBROADCAST"] = 0xD8;
    mnemonic_to_opcode["VCMPGT"] = 0xD9;
    mnemonic_to_opcode["PACKB"] = 0xDA;
    mnemonic_to_opcode["UNPACKB"] = 0xDB;

    // SSE/SIMD ops already mapped above via Opcode enum (lines 130-161)
}

void AssemblerEngine::init_register_table() {
    // Legacy register names (R0-R7)
    for (int i = 0; i < 8; ++i) {
        register_to_number["R" + std::to_string(i)] = i;
        register_sizes["R" + std::to_string(i)] = 32; // Default to 32-bit for legacy
    }

    // x64-style register names
    register_to_number["RAX"] = 0; register_sizes["RAX"] = 64;
    register_to_number["RCX"] = 1; register_sizes["RCX"] = 64;
    register_to_number["RDX"] = 2; register_sizes["RDX"] = 64;
    register_to_number["RBX"] = 3; register_sizes["RBX"] = 64;
    register_to_number["RSP"] = 4; register_sizes["RSP"] = 64;
    register_to_number["RBP"] = 5; register_sizes["RBP"] = 64;
    register_to_number["RSI"] = 6; register_sizes["RSI"] = 64;
    register_to_number["RDI"] = 7; register_sizes["RDI"] = 64;

    // Extended registers R8-R15
    for (int i = 8; i < 16; ++i) {
        register_to_number["R" + std::to_string(i)] = i;
        register_sizes["R" + std::to_string(i)] = 64; // R8-R15 are 64-bit

        register_to_number["R" + std::to_string(i) + "D"] = i; // 32-bit alias
        register_sizes["R" + std::to_string(i) + "D"] = 32;
    }

    // Special registers (mapped to extended register numbers)
    register_to_number["RIP"] = 16; register_sizes["RIP"] = 64;
    register_to_number["RFLAGS"] = 17; register_sizes["RFLAGS"] = 64;

    // 32-bit register aliases (for x86 compatibility)
    register_to_number["EAX"] = 0; register_sizes["EAX"] = 32;
    register_to_number["ECX"] = 1; register_sizes["ECX"] = 32;
    register_to_number["EDX"] = 2; register_sizes["EDX"] = 32;
    register_to_number["EBX"] = 3; register_sizes["EBX"] = 32;
    register_to_number["ESP"] = 4; register_sizes["ESP"] = 32;
    register_to_number["EBP"] = 5; register_sizes["EBP"] = 32;
    register_to_number["ESI"] = 6; register_sizes["ESI"] = 32;
    register_to_number["EDI"] = 7; register_sizes["EDI"] = 32;

    // XMM registers (stored as 2x64-bit parts in the VM)
    for (int i = 0; i < 16; ++i) {
        const int base = 50 + (i * 2); // Register::XMM0 is 50
        const std::string xmm = "XMM" + std::to_string(i);
        const std::string xmm_high = xmm + "_HIGH";
        register_to_number[xmm] = static_cast<uint8_t>(base);
        register_sizes[xmm] = 128;
        register_to_number[xmm_high] = static_cast<uint8_t>(base + 1);
        register_sizes[xmm_high] = 64;
    }

    // YMM register aliases (refer to the lower 128-bit XMM part)
    for (int i = 0; i < 16; ++i) {
        const int base = 50 + (i * 2); // Lower 128 bits
        const std::string ymm = "YMM" + std::to_string(i);
        register_to_number[ymm] = static_cast<uint8_t>(base);
        // register_sizes is a byte-sized field; represent YMM aliases as 128-bit (lower XMM part).
        register_sizes[ymm] = 128;
    }
}

void AssemblerEngine::init_encoder_map() {
    encoder_map_["DEBUG"] = &AssemblerEngine::enc_debug;
    encoder_map_["LOAD_IMM"] = &AssemblerEngine::enc_load_imm;
    encoder_map_["LOAD_IMM64"] = &AssemblerEngine::enc_load_imm64;

    encoder_map_["MOV"] = &AssemblerEngine::enc_mov;
    encoder_map_["MOV64"] = &AssemblerEngine::enc_mov;
    encoder_map_["MOVEX"] = &AssemblerEngine::enc_mov;

    encoder_map_["ADD"] = &AssemblerEngine::enc_alu_2reg;
    encoder_map_["SUB"] = &AssemblerEngine::enc_alu_2reg;
    encoder_map_["CMP"] = &AssemblerEngine::enc_alu_2reg;
    encoder_map_["MUL"] = &AssemblerEngine::enc_alu_2reg;
    encoder_map_["DIV"] = &AssemblerEngine::enc_alu_2reg;
    encoder_map_["MOD"] = &AssemblerEngine::enc_alu_2reg;
    encoder_map_["AND"] = &AssemblerEngine::enc_alu_2reg;
    encoder_map_["OR"] = &AssemblerEngine::enc_alu_2reg;
    encoder_map_["XOR"] = &AssemblerEngine::enc_alu_2reg;
    encoder_map_["ADD64"] = &AssemblerEngine::enc_alu_2reg;
    encoder_map_["SUB64"] = &AssemblerEngine::enc_alu_2reg;
    encoder_map_["CMP64"] = &AssemblerEngine::enc_alu_2reg;
    encoder_map_["MODECMP"] = &AssemblerEngine::enc_alu_2reg;
    encoder_map_["ADDEX"] = &AssemblerEngine::enc_alu_2reg;
    encoder_map_["SUBEX"] = &AssemblerEngine::enc_alu_2reg;
    encoder_map_["CMPEX"] = &AssemblerEngine::enc_alu_2reg;

    encoder_map_["MUL64"] = &AssemblerEngine::enc_alu_3reg;
    encoder_map_["DIV64"] = &AssemblerEngine::enc_alu_3reg;
    encoder_map_["AND64"] = &AssemblerEngine::enc_alu_3reg;
    encoder_map_["OR64"] = &AssemblerEngine::enc_alu_3reg;
    encoder_map_["XOR64"] = &AssemblerEngine::enc_alu_3reg;
    encoder_map_["MOD64"] = &AssemblerEngine::enc_alu_3reg;
    encoder_map_["MULEX"] = &AssemblerEngine::enc_alu_3reg;
    encoder_map_["DIVEX"] = &AssemblerEngine::enc_alu_3reg;

    encoder_map_["MODE32"] = &AssemblerEngine::enc_mode;
    encoder_map_["MODE64"] = &AssemblerEngine::enc_mode;

    encoder_map_["JMP"] = &AssemblerEngine::enc_jump;
    encoder_map_["JZ"] = &AssemblerEngine::enc_jump;
    encoder_map_["JE"] = &AssemblerEngine::enc_jump;
    encoder_map_["JNZ"] = &AssemblerEngine::enc_jump;
    encoder_map_["JNE"] = &AssemblerEngine::enc_jump;
    encoder_map_["JS"] = &AssemblerEngine::enc_jump;
    encoder_map_["JNS"] = &AssemblerEngine::enc_jump;
    encoder_map_["JC"] = &AssemblerEngine::enc_jump;
    encoder_map_["JNC"] = &AssemblerEngine::enc_jump;
    encoder_map_["JO"] = &AssemblerEngine::enc_jump;
    encoder_map_["JNO"] = &AssemblerEngine::enc_jump;
    encoder_map_["JG"] = &AssemblerEngine::enc_jump;
    encoder_map_["JL"] = &AssemblerEngine::enc_jump;
    encoder_map_["JGE"] = &AssemblerEngine::enc_jump;
    encoder_map_["JLE"] = &AssemblerEngine::enc_jump;
    encoder_map_["CALL"] = &AssemblerEngine::enc_jump;

    encoder_map_["PUSH"] = &AssemblerEngine::enc_single_reg;
    encoder_map_["POP"] = &AssemblerEngine::enc_single_reg;
    encoder_map_["INC"] = &AssemblerEngine::enc_single_reg;
    encoder_map_["DEC"] = &AssemblerEngine::enc_single_reg;
    encoder_map_["NOT"] = &AssemblerEngine::enc_single_reg;
    encoder_map_["INC64"] = &AssemblerEngine::enc_single_reg;
    encoder_map_["DEC64"] = &AssemblerEngine::enc_single_reg;
    encoder_map_["NOT64"] = &AssemblerEngine::enc_single_reg;

    encoder_map_["OUT"] = &AssemblerEngine::enc_io;
    encoder_map_["IN"] = &AssemblerEngine::enc_io;
    encoder_map_["OUTB"] = &AssemblerEngine::enc_io;
    encoder_map_["INB"] = &AssemblerEngine::enc_io;
    encoder_map_["OUTW"] = &AssemblerEngine::enc_io;
    encoder_map_["INW"] = &AssemblerEngine::enc_io;
    encoder_map_["OUTL"] = &AssemblerEngine::enc_io;
    encoder_map_["INL"] = &AssemblerEngine::enc_io;
    encoder_map_["OUTSTR"] = &AssemblerEngine::enc_io;
    encoder_map_["INSTR"] = &AssemblerEngine::enc_io;

    encoder_map_["LOAD"] = &AssemblerEngine::enc_load_store;
    encoder_map_["LOADR"] = &AssemblerEngine::enc_load_store;
    encoder_map_["STORE"] = &AssemblerEngine::enc_load_store;
    encoder_map_["STORER"] = &AssemblerEngine::enc_load_store;
    encoder_map_["LEA"] = &AssemblerEngine::enc_load_store;
    encoder_map_["SWAP"] = &AssemblerEngine::enc_load_store;

    encoder_map_["LOADEX"] = &AssemblerEngine::enc_loadex_storex;
    encoder_map_["STOREX"] = &AssemblerEngine::enc_loadex_storex;

    encoder_map_["SHL"] = &AssemblerEngine::enc_shift;
    encoder_map_["SHR"] = &AssemblerEngine::enc_shift;
    encoder_map_["SHL64"] = &AssemblerEngine::enc_shift;
    encoder_map_["SHR64"] = &AssemblerEngine::enc_shift;

    encoder_map_["FLD"] = &AssemblerEngine::enc_fpu_ldst;
    encoder_map_["FST"] = &AssemblerEngine::enc_fpu_ldst;
    encoder_map_["FSTP"] = &AssemblerEngine::enc_fpu_ldst;

    encoder_map_["FADD"] = &AssemblerEngine::enc_fpu_arith;
    encoder_map_["FSUB"] = &AssemblerEngine::enc_fpu_arith;
    encoder_map_["FMUL"] = &AssemblerEngine::enc_fpu_arith;
    encoder_map_["FDIV"] = &AssemblerEngine::enc_fpu_arith;

    encoder_map_["FILD"] = &AssemblerEngine::enc_fild;
    encoder_map_["FIST"] = &AssemblerEngine::enc_fist;
    encoder_map_["FISTP"] = &AssemblerEngine::enc_fist;

    encoder_map_["FSTCW"] = &AssemblerEngine::enc_fpu_ctrl;
    encoder_map_["FLDCW"] = &AssemblerEngine::enc_fpu_ctrl;

    encoder_map_["FSTSW"] = &AssemblerEngine::enc_fstsw;

    encoder_map_["CMPPS"] = &AssemblerEngine::enc_sse_cmp;
    encoder_map_["CMPPD"] = &AssemblerEngine::enc_sse_cmp;

    encoder_map_["MOVAPS"] = &AssemblerEngine::enc_sse;
    encoder_map_["MOVUPS"] = &AssemblerEngine::enc_sse;
    encoder_map_["MOVPS"] = &AssemblerEngine::enc_sse;
    encoder_map_["ADDPS"] = &AssemblerEngine::enc_sse;
    encoder_map_["SUBPS"] = &AssemblerEngine::enc_sse;
    encoder_map_["MULPS"] = &AssemblerEngine::enc_sse;
    encoder_map_["DIVPS"] = &AssemblerEngine::enc_sse;
    encoder_map_["SQRTPS"] = &AssemblerEngine::enc_sse;
    encoder_map_["MAXPS"] = &AssemblerEngine::enc_sse;
    encoder_map_["MINPS"] = &AssemblerEngine::enc_sse;
    encoder_map_["ANDPS"] = &AssemblerEngine::enc_sse;
    encoder_map_["ORPS"] = &AssemblerEngine::enc_sse;
    encoder_map_["XORPS"] = &AssemblerEngine::enc_sse;
    encoder_map_["MOVAPD"] = &AssemblerEngine::enc_sse;
    encoder_map_["MOVUPD"] = &AssemblerEngine::enc_sse;
    encoder_map_["ADDPD"] = &AssemblerEngine::enc_sse;
    encoder_map_["SUBPD"] = &AssemblerEngine::enc_sse;
    encoder_map_["MULPD"] = &AssemblerEngine::enc_sse;
    encoder_map_["DIVPD"] = &AssemblerEngine::enc_sse;
    encoder_map_["SQRTPD"] = &AssemblerEngine::enc_sse;
    encoder_map_["MAXPD"] = &AssemblerEngine::enc_sse;
    encoder_map_["MINPD"] = &AssemblerEngine::enc_sse;
    encoder_map_["ANDPD"] = &AssemblerEngine::enc_sse;
    encoder_map_["ORPD"] = &AssemblerEngine::enc_sse;
    encoder_map_["XORPD"] = &AssemblerEngine::enc_sse;

    encoder_map_["NOP"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["HALT"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["RET"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["PUSH_FLAG"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["POP_FLAG"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["FINIT"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["FCLEX"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["FABS"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["FCHS"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["FSQRT"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["FSIN"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["FCOS"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["FTAN"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["FCOMPP"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["FUCOMPP"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["FEXP"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["FLN"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["FLG2"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["FLDPI"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["FLDZ"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["FLD1"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["CLI"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["STI"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["IRET"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["VADD"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["VMUL"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["VDOT"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["VMAX"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["VBROADCAST"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["VCMPGT"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["PACKB"] = &AssemblerEngine::enc_no_operands;
    encoder_map_["UNPACKB"] = &AssemblerEngine::enc_no_operands;

    encoder_map_["INT"] = &AssemblerEngine::enc_int;
}

std::vector<uint8_t> AssemblerEngine::assemble(const Program& program) {
    // Clear previous state
    errors.clear();
    symbol_table.clear();
    forward_refs.clear();
    bytecode.clear();

    try {
        [[maybe_unused]] IRProgram lowered_program = lower_program(program);
    } catch (const std::exception& ex) {
        add_error(std::string("Failed to lower assembler program: ") + ex.what());
        return {};
    }

    // Do not reset current_address; let directives set it as needed

    // Two-pass assembly
    db_next_addr = 0x100; // Only reset once here
    first_pass(program);
    if (has_errors()) return {};

    // Do NOT reset db_next_addr in second_pass or first_pass
    second_pass(program);
    if (has_errors()) return {};

    resolve_forward_references();
    if (has_errors()) return {};

    return bytecode;
}

void AssemblerEngine::first_pass(const Program& program) {
    current_address = 0;
    use_simple_db_format = false; // Reset flag
    
    // Detect DB format: count DB instructions and check for .org
    size_t db_count = 0;
    bool has_org = false;
    
    for (const auto& stmt : program.statements) {
        if (stmt->type == ASTNodeType::INSTRUCTION) {
            const auto& instruction = static_cast<const Instruction&>(*stmt);
            if (instruction.mnemonic == "DB" || instruction.mnemonic == "RESB") {
                db_count++;
            }
        } else if (stmt->type == ASTNodeType::DIRECTIVE) {
            const auto& directive = static_cast<const Directive&>(*stmt);
            if (directive.name == ".org") {
                has_org = true;
            }
        }
    }
    
    // Use simple format for multiple DBs or with .org
    use_simple_db_format = (db_count > 1) || has_org;

    // First, process all instructions/directives to update current_address and track highest address
    uint32_t highest_address = 0;
    for (size_t i = 0; i < program.statements.size(); ++i) {
        const auto& stmt = program.statements[i];
        
        // Handle DB and RESB size calculation
        if (stmt->type == ASTNodeType::INSTRUCTION) {
            const auto& instruction = static_cast<const Instruction&>(*stmt);
            if (instruction.mnemonic == "DB") {
                // Calculate DB size from operands
                for (const auto& operand : instruction.operands) {
                    if (auto str_lit = dynamic_cast<const StringLiteralExpression*>(operand.get())) {
                        current_address += str_lit->value.length();
                    } else {
                        current_address += 1; // Single byte
                    }
                }
                if (current_address > highest_address) highest_address = current_address;
                continue;
            } else if (instruction.mnemonic == "RESB") {
                // RESB reserves bytes
                if (instruction.operands.size() == 1) {
                    bool is_symbol;
                    std::string symbol_name;
                    int64_t bytes_to_reserve = evaluate_expression(*instruction.operands[0], is_symbol, symbol_name);
                    if (!is_symbol && bytes_to_reserve >= 0) {
                        current_address += static_cast<uint32_t>(bytes_to_reserve);
                        if (current_address > highest_address) highest_address = current_address;
                    }
                }
                continue;
            }
        }
        // Process labels in first pass to assign correct addresses
        if (stmt->type == ASTNodeType::LABEL) {
            const auto& label = static_cast<const Label&>(*stmt);
            // Add label to symbol table with current address
            auto it = symbol_table.find(label.name);
            if (it != symbol_table.end()) {
                // Duplicate definition of an already-defined label is an error.
                // (Allow a previously-declared global/extern symbol to be defined once.)
                if (it->second.defined) {
                    const std::string message = "Duplicate label: " + label.name;
                    errors.push_back("Line " + std::to_string(label.line) + ", Column " + std::to_string(label.column) + ": " + message);
                    Logging::ErrorHandler::instance().report_parse(
                        Logging::ErrorCode::PARSE_DUPLICATE_LABEL,
                        message,
                        "",
                        label.line,
                        label.column);
                    last_label_name = label.name;
                    continue; // Keep scanning to accumulate errors, but don't redefine the symbol.
                }

                // It was declared (e.g. global/extern), now we define it.
                it->second.address = current_address;
                it->second.defined = true;
                it->second.section = current_section;
            } else {
                symbol_table[label.name] = Symbol(label.name, current_address, true);
                symbol_table[label.name].section = current_section;
            }
            last_label_name = label.name;
            continue; // Labels don't change current_address
        }
        // Normal instruction/directive size logic
        if (stmt->type == ASTNodeType::INSTRUCTION) {
            const auto& instruction = static_cast<const Instruction&>(*stmt);
            size_t size = get_instruction_size(instruction.mnemonic, instruction.operands);
            current_address += size;
            if (current_address > highest_address) highest_address = current_address;
        } else if (stmt->type == ASTNodeType::DIRECTIVE) {
            const auto& directive = static_cast<const Directive&>(*stmt);
            if (directive.name == ".org") {
                handle_org_directive(directive.arguments);
                if (current_address > highest_address) highest_address = current_address;
            } else if (directive.name == ".equ") {
                handle_equ_directive(directive.arguments);
                // .equ doesn't change current_address
            } else if (directive.name == ".dw") {
                current_address += directive.arguments.size() * 2;
                if (current_address > highest_address) highest_address = current_address;
            } else if (directive.name == ".dd") {
                current_address += directive.arguments.size() * 4;
                if (current_address > highest_address) highest_address = current_address;
            } else if (directive.name == ".string") {
                for (const auto& arg : directive.arguments) {
                    if (auto str_lit = dynamic_cast<const StringLiteralExpression*>(arg.get())) {
                        current_address += str_lit->value.length() + 1;
                    }
                }
                if (current_address > highest_address) highest_address = current_address;
            } else if (directive.name == ".align") {
                if (directive.arguments.size() == 1) {
                    bool is_symbol;
                    std::string symbol_name;
                    int64_t alignment = evaluate_expression(*directive.arguments[0], is_symbol, symbol_name);
                    if (!is_symbol && alignment > 0 && (alignment & (alignment - 1)) == 0) {
                        uint32_t align_mask = static_cast<uint32_t>(alignment - 1);
                        current_address = (current_address + align_mask) & ~align_mask;
                        if (current_address > highest_address) highest_address = current_address;
                    }
                }
            } else if (directive.name == ".bss" || directive.name == ".resb" || directive.name == "RESB") {
                if (directive.arguments.size() == 1) {
                    bool is_symbol;
                    std::string symbol_name;
                    int64_t bytes_to_reserve = evaluate_expression(*directive.arguments[0], is_symbol, symbol_name);
                    if (!is_symbol && bytes_to_reserve >= 0) {
                        current_address += static_cast<uint32_t>(bytes_to_reserve);
                        if (current_address > highest_address) highest_address = current_address;
                    }
                }
            } else if (directive.name == ".data") {
                handle_data_section();
            } else if (directive.name == ".text") {
                // Calculate .data section size before switching to .text
                if (current_section == ".data" && has_explicit_sections) {
                    data_section_size = current_address - data_section_base;
                }
                handle_text_section();
            }
        }
    }
    
    // Calculate final section sizes
    if (has_explicit_sections) {
        if (current_section == ".data") {
            data_section_size = current_address - data_section_base;
        } else if (current_section == ".text") {
            text_section_size = current_address - text_section_base;
        }
    }
    
    // Now set db_next_addr to highest_address, so DB data is placed after code and buffer zones
    db_next_addr = highest_address;
    // DB labels will be fixed during second pass when actual data locations are known
}

void Assembler::AssemblerEngine::second_pass(const Assembler::Program& program) {
    current_address = 0;
    bytecode.clear();
    entry_address = 0;
    std::string last_label_name; // Track the most recent label for DB assignment

    bool first_instruction_address_set = false;
    uint32_t first_instruction_address = 0;
    
    for (const auto& stmt : program.statements) {
        if (stmt->type == Assembler::ASTNodeType::DIRECTIVE) {
            const auto& directive = static_cast<const Assembler::Directive&>(*stmt);
            process_directive(directive);
        } else if (stmt->type == Assembler::ASTNodeType::LABEL) {
            const auto& label = static_cast<const Assembler::Label&>(*stmt);
            // Labels are already processed in first pass, just track for DB assignment
            last_label_name = label.name; // Remember this label for potential DB assignment
            // Update current_address to match the label's address from first pass
            auto it = symbol_table.find(label.name);
            if (it != symbol_table.end() && it->second.defined) {
                current_address = it->second.address;
            }
        } else if (stmt->type == Assembler::ASTNodeType::INSTRUCTION) {
            const auto& instruction = static_cast<const Instruction&>(*stmt);
            DEBUG_DETAIL(Logging::DebugCategory::ASM_ENCODING,
                "[PASS2_ADDR] before {} at current_address=0x{:04X}, bytecode.size()=0x{:04X}",
                instruction.mnemonic, current_address, bytecode.size());
            if (instruction.mnemonic != "DB") {
                // Pad bytecode to current_address before emitting
                while (bytecode.size() < current_address) {
                    bytecode.push_back(0);
                }
                // Track first instruction address for fallback
                if (!first_instruction_address_set) {
                    first_instruction_address = bytecode.size();
                    first_instruction_address_set = true;
                }
            }
            process_instruction(instruction);
            DEBUG_DETAIL(Logging::DebugCategory::ASM_ENCODING,
                "[PASS2_ADDR] after {} at current_address=0x{:04X}, bytecode.size()=0x{:04X}",
                instruction.mnemonic, current_address, bytecode.size());
        }
    }
    
    // After processing all statements, resolve entry point
    DEBUG_DETAIL(Logging::DebugCategory::ASM_ENCODING,
        "[ENTRY_POINT] Looking for entry point symbol: '{}'", entry_point_symbol);
    DEBUG_DETAIL(Logging::DebugCategory::ASM_ENCODING,
        "[ENTRY_POINT] first_instruction_address_set={}, first_instruction_address=0x{:04X}", 
        first_instruction_address_set, first_instruction_address);
    
    auto entry_it = symbol_table.find(entry_point_symbol);
    if (entry_it != symbol_table.end() && entry_it->second.defined) {
        // Use the specified entry point symbol if it exists and is defined
        entry_address = entry_it->second.address;
        DEBUG_DETAIL(Logging::DebugCategory::ASM_ENCODING,
            "[ENTRY_POINT] Found symbol '{}' at address 0x{:04X}", entry_point_symbol, entry_address);
    } else if (first_instruction_address_set) {
        // Fall back to first instruction
        entry_address = first_instruction_address;
        DEBUG_DETAIL(Logging::DebugCategory::ASM_ENCODING,
            "[ENTRY_POINT] Symbol '{}' not found, using first instruction at 0x{:04X}", 
            entry_point_symbol, entry_address);
    } else {
        // No instructions at all - default to 0
        entry_address = 0;
        DEBUG_DETAIL(Logging::DebugCategory::ASM_ENCODING,
            "[ENTRY_POINT] {}", "No entry symbol or instructions found, using address 0");
    }
}

void Assembler::AssemblerEngine::process_label(const Assembler::Label& label) {
    if (symbol_table.find(label.name) != symbol_table.end()) {
        // If it exists, check if it was declared global/extern but not defined
        if (symbol_table[label.name].defined) {
            add_error("Label '" + label.name + "' already defined", label.line, label.column);
            return;
        }
        // It was declared (e.g. global), now we define it
        symbol_table[label.name].address = current_address;
        symbol_table[label.name].defined = true;
        symbol_table[label.name].section = current_section;
    } else {
        symbol_table[label.name] = Symbol(label.name, current_address, true);
        symbol_table[label.name].section = current_section;
    }
    last_label_name = label.name; // Remember this label for potential DB assignment
}

void Assembler::AssemblerEngine::process_directive(const Assembler::Directive& directive) {
    if (directive.name == ".memory") {
        handle_memory_directive(directive.arguments);
    } else if (directive.name == ".equ") {
        // EQU is already processed in first_pass, skip it here
        return;
    } else if (directive.name == ".dw") {
        handle_dw_directive(directive.arguments);
    } else if (directive.name == ".dd") {
        handle_dd_directive(directive.arguments);
    } else if (directive.name == ".string") {
        handle_string_directive(directive.arguments);
    } else if (directive.name == ".align") {
        handle_align_directive(directive.arguments);
    } else if (directive.name == ".bss") {
        handle_bss_directive(directive.arguments);
    } else if (directive.name == ".resb" || directive.name == "RESB") {
        handle_bss_directive(directive.arguments); // RESB works the same as .bss
    } else if (directive.name == "section" || directive.name == ".section") {
        handle_section_directive(directive.arguments);
    } else if (directive.name == "global" || directive.name == ".global") {
        handle_global_directive(directive.arguments);
    } else if (directive.name == "extern" || directive.name == ".extern") {
        handle_extern_directive(directive.arguments);
    } else if (directive.name == ".data") {
        handle_data_section();
    } else if (directive.name == ".text") {
        handle_text_section();
    } else if (directive.name == ".org") {
    Logging::DebugHandler::instance().report(Logging::DebugCategory::ASM_DIRECTIVE, "Processing .org directive", Logging::DebugLevel::DETAIL);
        handle_org_directive(directive.arguments);
    DEBUG_INFO(Logging::DebugCategory::ASM_DIRECTIVE, "After .org directive, current_address: 0x{:04X}", current_address);
    } else {
        add_error("Unknown directive: " + directive.name, directive.line, directive.column);
    }
}

void Assembler::AssemblerEngine::process_instruction(const Assembler::Instruction& instruction) {
    // Record source line for DWARF debug info
    if (instruction.line > 0) {
        bytecode_line_map.push_back({bytecode.size(), static_cast<uint32_t>(instruction.line)});
    }
    encode_instruction(instruction);
}

void Assembler::AssemblerEngine::encode_instruction(const Assembler::Instruction& instruction) {
    std::string mnemonic = instruction.mnemonic;

    // Auto-detect 64-bit operations based on register sizes
    bool use_64bit = false;
    for (const auto& operand : instruction.operands) {
        if (auto reg_expr = dynamic_cast<const Assembler::RegisterExpression*>(operand.get())) {
            if (register_sizes[reg_expr->name] == 64) {
                use_64bit = true;
                break;
            }
        }
    }

    if (use_64bit) {
        if (mnemonic == "MOV") mnemonic = "MOV64";
        else if (mnemonic == "ADD") mnemonic = "ADD64";
        else if (mnemonic == "SUB") mnemonic = "SUB64";
        else if (mnemonic == "MUL") mnemonic = "MUL64";
        else if (mnemonic == "DIV") mnemonic = "DIV64";
        else if (mnemonic == "AND") mnemonic = "AND64";
        else if (mnemonic == "OR") mnemonic = "OR64";
        else if (mnemonic == "XOR") mnemonic = "XOR64";
        else if (mnemonic == "SHL") mnemonic = "SHL64";
        else if (mnemonic == "SHR") mnemonic = "SHR64";
        else if (mnemonic == "CMP") mnemonic = "CMP64";
        else if (mnemonic == "NOT") mnemonic = "NOT64";
        else if (mnemonic == "INC") mnemonic = "INC64";
        else if (mnemonic == "DEC") mnemonic = "DEC64";
        else if (mnemonic == "MOD") mnemonic = "MOD64";
        else if (mnemonic == "LOAD_IMM") mnemonic = "LOAD_IMM64";
        else if (mnemonic == "LOAD") mnemonic = "LOADEX";
        else if (mnemonic == "STORE") mnemonic = "STOREX";
    }

    // Pad bytecode to current_address before emitting instruction
    while (bytecode.size() < current_address) {
        bytecode.push_back(0);
    }
    Logging::DebugHandler::instance().report(Logging::DebugCategory::ASM_ENCODING, fmt::format("Emitting instruction '{}' at address: 0x{:04X}", mnemonic, current_address), Logging::DebugLevel::DETAIL);
    
    // Handle DB pseudo-instruction (no opcode emission, just raw bytes)
    if (mnemonic == "DB") {
        if (instruction.operands.empty()) {
            add_error("DB requires at least one operand", instruction.line, instruction.column);
            return;
        }

        // Check if first operand is a string literal
        if (auto str_expr = dynamic_cast<const Assembler::StringLiteralExpression*>(instruction.operands[0].get())) {
            // String-based DB: emit string bytes, then optional additional operands
            std::string str_value = str_expr->value;
            
            // Emit string characters
            for (char c : str_value) {
                emit_byte(static_cast<uint8_t>(c));
            }
            
            // Emit any additional operands (length, terminator, etc.) - all optional
            for (size_t i = 1; i < instruction.operands.size(); ++i) {
                bool is_symbol;
                std::string symbol_name;
                int64_t value = evaluate_expression(*instruction.operands[i], is_symbol, symbol_name);
                
                if (is_symbol) {
                    emit_forward_ref(symbol_name, 1);
                } else {
                    emit_byte(static_cast<uint8_t>(value));
                }
            }
        } else {
            // Simple byte list - emit each operand as a byte
            for (const auto& operand : instruction.operands) {
                bool is_symbol;
                std::string symbol_name;
                int64_t value = evaluate_expression(*operand, is_symbol, symbol_name);
                
                if (is_symbol) {
                    emit_forward_ref(symbol_name, 1);
                } else {
                    if (value < 0 || value > 255) {
                        add_error("DB byte value " + std::to_string(value) + 
                                  " out of range (must be 0-255)", instruction.line, instruction.column);
                        return;
                    }
                    emit_byte(static_cast<uint8_t>(value));
                }
            }
        }
        return; // DB doesn't emit an opcode, so return early
    }
    
    // Handle RESB pseudo-instruction (reserve bytes - emit zeros)
    if (mnemonic == "RESB") {
        if (instruction.operands.size() != 1) {
            add_error("RESB requires exactly one operand (number of bytes)", instruction.line, instruction.column);
            return;
        }
        
        bool is_symbol;
        std::string symbol_name;
        int64_t count = evaluate_expression(*instruction.operands[0], is_symbol, symbol_name);
        
        if (is_symbol) {
            add_error("RESB cannot use forward references", instruction.line, instruction.column);
            return;
        }
        
        if (count < 0) {
            add_error("RESB count must be non-negative", instruction.line, instruction.column);
            return;
        }
        
        // Emit 'count' zero bytes
        for (int64_t i = 0; i < count; i++) {
            emit_byte(0);
        }
        return; // RESB doesn't emit an opcode, so return early
    }
    
    uint8_t opcode = get_opcode(mnemonic);
    if (opcode == 0xFF && mnemonic != "HALT") {
        add_error("Unknown instruction: " + mnemonic, instruction.line, instruction.column);
        return;
    }

    // Special handling for CMP with immediate operand BEFORE emitting opcode
    if ((mnemonic == "CMP" || mnemonic == "CMP64" || mnemonic == "CMPEX") &&
        instruction.operands.size() == 2 &&
        !dynamic_cast<const Assembler::RegisterExpression*>(instruction.operands[1].get())) {
        
        // Handle CMP reg, immediate by loading immediate into a temporary register first
        auto reg_expr = dynamic_cast<const Assembler::RegisterExpression*>(instruction.operands[0].get());
        if (!reg_expr) {
            add_error(mnemonic + " first operand must be a register", instruction.line, instruction.column);
            return;
        }
        
        uint8_t dest_reg = get_register_number(reg_expr->name);
        
        // Choose a temporary register intelligently based on architecture and usage
        uint8_t temp_reg;
        if (Config::architecture == Architecture::X64) {
            // In x64 mode, prefer R15 (extended register, rarely used)
            // If R15 is already the dest_reg, find the closest unused extended register
            if (dest_reg == 15) {
                // Try R14, R13, R12, R11, R10, R9, R8, avoiding RSP(4) and RBP(5)
                for (uint8_t reg = 14; reg >= 8; reg--) {
                    if (reg != dest_reg) {
                        temp_reg = reg;
                        break;
                    }
                }
                // If somehow all extended registers are used (unlikely), fallback to RDX
                if (dest_reg == 15 && temp_reg == 15) {
                    temp_reg = 2;  // RDX
                }
            } else {
                temp_reg = 15;  // R15 - default choice for x64
            }
        } else {
            // In x86 mode, only registers 0-7 are available (RAX-RDI)
            // Must avoid RSP(4) and RBP(5), and prefer not clobbering dest_reg
            // Strategy: prefer RDX(2), then RAX(0), then RCX(1), then RBX(3)
            const uint8_t rsp_reg = 4;
            const uint8_t rbp_reg = 5;
            const uint8_t preferred_temps[] = {2, 0, 1, 3, 6, 7};  // RDX, RAX, RCX, RBX, RSI, RDI
            
            temp_reg = 2;  // Default to RDX
            for (uint8_t candidate : preferred_temps) {
                if (candidate != dest_reg && candidate != rsp_reg && candidate != rbp_reg) {
                    temp_reg = candidate;
                    break;
                }
            }
        }
        
        // Evaluate the immediate value
        bool is_symbol;
        std::string symbol_name;
        int64_t imm_value = evaluate_expression(*instruction.operands[1], is_symbol, symbol_name);
        
        // Emit LOAD_IMM temp_reg, immediate
        emit_byte(static_cast<uint8_t>(Opcode::LOAD_IMM));
        emit_byte(temp_reg);
        if (is_symbol) {
            emit_forward_ref(symbol_name, 4);
        } else {
            emit_dword(static_cast<uint32_t>(imm_value));
        }
        
        // Emit CMP dest_reg, temp_reg
        emit_byte(opcode); // CMP opcode
        emit_byte(dest_reg);
        emit_byte(temp_reg);
        
        return; // Early return, we've handled the entire instruction
    }

    // Dispatch to encoder via map lookup
    auto it = encoder_map_.find(mnemonic);
    if (it == encoder_map_.end()) {
        add_error("Instruction encoding not implemented: " + instruction.mnemonic, instruction.line, instruction.column);
        return;
    }

    (this->*(it->second))(instruction, opcode);
}


void Assembler::AssemblerEngine::enc_debug(const Instruction& instr, uint8_t opcode) {
    emit_byte(opcode);
    if (instr.operands.empty()) {
        add_error("DEBUG requires at least 1 operand (sub-opcode)", instr.line, instr.column);
        return;
    }
    bool is_symbol;
    std::string symbol_name;
    int64_t sub_opcode = evaluate_expression(*instr.operands[0], is_symbol, symbol_name);
    emit_byte(static_cast<uint8_t>(sub_opcode));
    if (sub_opcode == 0) {
         if (instr.operands.size() < 2) { add_error("DEBUG PRINT requires a second operand", instr.line, instr.column); return; }
         if (auto str_expr = dynamic_cast<const StringLiteralExpression*>(instr.operands[1].get())) {
             emit_byte(1); std::string s = str_expr->value;
             if (s.length() > 255) s = s.substr(0, 255);
             emit_byte(static_cast<uint8_t>(s.length()));
             for (char c : s) emit_byte(c);
         } else if (auto reg_expr = dynamic_cast<const RegisterExpression*>(instr.operands[1].get())) {
             emit_byte(0); emit_byte(get_register_number(reg_expr->name));
         } else { add_error("DEBUG PRINT operand must be string or register", instr.line, instr.column); }
    } else if (sub_opcode == 3) {
         if (instr.operands.size() < 3) { add_error("DEBUG MEMDUMP requires start and length", instr.line, instr.column); return; }
         int64_t start = evaluate_expression(*instr.operands[1], is_symbol, symbol_name);
         if (is_symbol) emit_forward_ref(symbol_name, 4); else emit_dword(static_cast<uint32_t>(start));
         int64_t len = evaluate_expression(*instr.operands[2], is_symbol, symbol_name);
         emit_dword(static_cast<uint32_t>(len));
    } else if (sub_opcode == 4) {
         if (instr.operands.size() > 1) { int64_t val = evaluate_expression(*instr.operands[1], is_symbol, symbol_name); emit_byte(static_cast<uint8_t>(val)); }
         else { emit_byte(2); }
    } else if (sub_opcode == 5) {
         if (instr.operands.size() < 3) { add_error("DEBUG ASSERT requires register/memory and value", instr.line, instr.column); return; }
         if (auto reg_expr = dynamic_cast<const RegisterExpression*>(instr.operands[1].get())) { emit_byte(0); emit_byte(get_register_number(reg_expr->name)); }
         else { emit_byte(1); int64_t addr = evaluate_expression(*instr.operands[1], is_symbol, symbol_name); emit_dword(static_cast<uint32_t>(addr)); }
         int64_t value = evaluate_expression(*instr.operands[2], is_symbol, symbol_name);
         emit_qword(static_cast<uint64_t>(value));
    } else if (sub_opcode == 6) {
         if (instr.operands.size() > 1) { int64_t depth = evaluate_expression(*instr.operands[1], is_symbol, symbol_name); emit_dword(static_cast<uint32_t>(depth)); }
         else { emit_dword(16); }
    } else if (sub_opcode == 7) {
         if (instr.operands.size() < 3) { add_error("DEBUG WATCH requires address and length", instr.line, instr.column); return; }
         int64_t addr = evaluate_expression(*instr.operands[1], is_symbol, symbol_name); emit_dword(static_cast<uint32_t>(addr));
         int64_t len = evaluate_expression(*instr.operands[2], is_symbol, symbol_name); emit_dword(static_cast<uint32_t>(len));
    } else if (sub_opcode == 8) {
         if (instr.operands.size() < 2) { add_error("DEBUG UNWATCH requires address", instr.line, instr.column); return; }
         int64_t addr = evaluate_expression(*instr.operands[1], is_symbol, symbol_name); emit_dword(static_cast<uint32_t>(addr));
    } else if (sub_opcode == 9) {
         if (instr.operands.size() < 2) { add_error("DEBUG CHECKPOINT requires label", instr.line, instr.column); return; }
         if (auto str_expr = dynamic_cast<const StringLiteralExpression*>(instr.operands[1].get())) {
             std::string s = str_expr->value; if (s.length() > 255) s = s.substr(0, 255);
             emit_byte(static_cast<uint8_t>(s.length())); for (char c : s) emit_byte(c);
         } else { add_error("DEBUG CHECKPOINT label must be string", instr.line, instr.column); }
    } else if (sub_opcode == 10) {
         if (instr.operands.size() < 3) { add_error("DEBUG LOG requires level and message", instr.line, instr.column); return; }
         int64_t level = evaluate_expression(*instr.operands[1], is_symbol, symbol_name); emit_byte(static_cast<uint8_t>(level));
         if (auto str_expr = dynamic_cast<const StringLiteralExpression*>(instr.operands[2].get())) {
             std::string s = str_expr->value; if (s.length() > 255) s = s.substr(0, 255);
             emit_byte(static_cast<uint8_t>(s.length())); for (char c : s) emit_byte(c);
         } else { add_error("DEBUG LOG message must be string", instr.line, instr.column); }
    } else if (sub_opcode == 11) {
         if (instr.operands.size() < 2) { add_error("DEBUG DUMPREG requires register", instr.line, instr.column); return; }
         if (auto reg_expr = dynamic_cast<const RegisterExpression*>(instr.operands[1].get())) { emit_byte(get_register_number(reg_expr->name)); }
         else { add_error("DEBUG DUMPREG operand must be register", instr.line, instr.column); }
    } else if (sub_opcode == 12) {
         if (instr.operands.size() < 4) { add_error("DEBUG MEMSET requires address, length, and value", instr.line, instr.column); return; }
         int64_t addr = evaluate_expression(*instr.operands[1], is_symbol, symbol_name); emit_dword(static_cast<uint32_t>(addr));
         int64_t len = evaluate_expression(*instr.operands[2], is_symbol, symbol_name); emit_dword(static_cast<uint32_t>(len));
         int64_t value = evaluate_expression(*instr.operands[3], is_symbol, symbol_name); emit_byte(static_cast<uint8_t>(value));
    } else if (sub_opcode == 13) {
         if (instr.operands.size() > 1) { int64_t count = evaluate_expression(*instr.operands[1], is_symbol, symbol_name); emit_dword(static_cast<uint32_t>(count)); }
         else { emit_dword(1); }
    }
}

void Assembler::AssemblerEngine::enc_load_imm(const Instruction& instr, uint8_t opcode) {
    emit_byte(opcode);
    if (instr.operands.size() != 2) { add_error("LOAD_IMM requires 2 operands", instr.line, instr.column); return; }
    std::string reg_name;
    if (auto reg_expr = dynamic_cast<const RegisterExpression*>(instr.operands[0].get())) { reg_name = reg_expr->name; emit_byte(get_register_number(reg_name)); }
    else { add_error("First operand must be a register", instr.line, instr.column); return; }
    bool is_symbol; std::string symbol_name;
    int64_t value = evaluate_expression(*instr.operands[1], is_symbol, symbol_name);
    bool is_32bit_reg = (register_sizes.find(reg_name) != register_sizes.end() && register_sizes[reg_name] == 32);
    size_t imm_size = is_32bit_reg ? 4 : 1;
    if (is_symbol) { emit_forward_ref(symbol_name, imm_size); }
    else {
        if (imm_size == 1) {
            if (value < 0 || value > 255) { add_error("LOAD_IMM immediate value " + std::to_string(value) + " out of range for 8-bit register (must be 0-255)", instr.line, instr.column); return; }
            emit_byte(static_cast<uint8_t>(value));
        } else {
            if (value < 0 || value > 0xFFFFFFFF) { add_error("LOAD_IMM immediate value " + std::to_string(value) + " out of range for 32-bit register (must be 0-4294967295)", instr.line, instr.column); return; }
            emit_dword(static_cast<uint32_t>(value));
        }
    }
}

void Assembler::AssemblerEngine::enc_load_imm64(const Instruction& instr, uint8_t opcode) {
    emit_byte(opcode);
    if (instr.operands.size() != 2) { add_error("LOAD_IMM64 requires 2 operands", instr.line, instr.column); return; }
    if (auto reg_expr = dynamic_cast<const RegisterExpression*>(instr.operands[0].get())) { emit_byte(get_register_number(reg_expr->name)); }
    else { add_error("First operand must be a register", instr.line, instr.column); return; }
    bool is_symbol; std::string symbol_name;
    int64_t value = evaluate_expression(*instr.operands[1], is_symbol, symbol_name);
    if (is_symbol) { emit_forward_ref(symbol_name, 8); }
    else { for (int i = 0; i < 8; ++i) { emit_byte(static_cast<uint8_t>((value >> (8 * i)) & 0xFF)); } }
}

void Assembler::AssemblerEngine::enc_mov(const Instruction& instr, [[maybe_unused]] uint8_t opcode) {
    if (instr.operands.size() != 2) { add_error(instr.mnemonic + " requires 2 operands", instr.line, instr.column); return; }
    auto dst = instr.operands[0].get();
    auto src = instr.operands[1].get();
    if (auto dst_reg = dynamic_cast<const RegisterExpression*>(dst)) {
        uint8_t dst_reg_num = get_register_number(dst_reg->name);
        bool is_extended = (dst_reg_num >= 8 && dst_reg_num <= 15);
        if (auto src_reg = dynamic_cast<const RegisterExpression*>(src)) {
            uint8_t src_reg_num = get_register_number(src_reg->name);
            bool src_extended = (src_reg_num >= 8 && src_reg_num <= 15);
            if (instr.mnemonic == "MOVEX") {
                if (!(is_extended && src_extended)) { add_error("MOVEX requires both operands to be extended integer registers R8-R15", instr.line, instr.column); return; }
                emit_byte(static_cast<uint8_t>(Opcode::MOVEX)); emit_byte(dst_reg_num); emit_byte(src_reg_num); return;
            }
            if (is_extended || src_extended) { emit_byte(static_cast<uint8_t>(Opcode::MOVEX)); }
            else if (register_sizes[dst_reg->name] == 64 || register_sizes[src_reg->name] == 64) { emit_byte(static_cast<uint8_t>(Opcode::MOV64)); }
            else { emit_byte(static_cast<uint8_t>(Opcode::MOV)); }
            emit_byte(dst_reg_num); emit_byte(src_reg_num);
        } else if (dynamic_cast<const ImmediateExpression*>(src) || dynamic_cast<const IdentifierExpression*>(src)) {
            int reg_size = register_sizes[dst_reg->name];
            if (reg_size >= 32) {
                emit_byte(static_cast<uint8_t>(Opcode::LOAD_IMM64)); emit_byte(dst_reg_num);
                bool is_symbol; std::string symbol_name;
                int64_t val = evaluate_expression(*src, is_symbol, symbol_name);
                if (is_symbol) emit_forward_ref(symbol_name, 8);
                else for(int i=0; i<8; ++i) emit_byte((uint8_t)((val >> (i*8)) & 0xFF));
            } else {
                emit_byte(static_cast<uint8_t>(Opcode::LOAD_IMM)); emit_byte(dst_reg_num);
                bool is_symbol; std::string symbol_name;
                int64_t val = evaluate_expression(*src, is_symbol, symbol_name);
                if (is_symbol) emit_forward_ref(symbol_name, 1);
                else emit_byte((uint8_t)val);
            }
        } else if (auto mem_expr = dynamic_cast<const MemoryReferenceExpression*>(src)) {
            if (is_simple_register_indirect(*mem_expr)) {
                emit_byte(static_cast<uint8_t>(Opcode::LOADR)); emit_byte(dst_reg_num);
                auto base_reg = static_cast<const RegisterExpression*>(mem_expr->base.get());
                emit_byte(get_register_number(base_reg->name));
            } else {
                if (is_extended) emit_byte(static_cast<uint8_t>(Opcode::LOADEX));
                else emit_byte(static_cast<uint8_t>(Opcode::LOAD));
                emit_byte(dst_reg_num);
                bool is_symbol = false; std::string symbol_name;
                int64_t val = 0;
                if (!resolve_memory_operand_address(*mem_expr, val, is_symbol, symbol_name)) {
                    add_error("Unsupported memory operand for MOV source", instr.line, instr.column);
                    return;
                }
                if (is_extended) {
                    if (is_symbol) emit_forward_ref(symbol_name, 8);
                    else for(int i=0; i<8; ++i) emit_byte((uint8_t)((val >> (i*8)) & 0xFF));
                } else {
                    if (is_symbol) emit_forward_ref(symbol_name, 4);
                    else emit_dword((uint32_t)val);
                }
            }
        } else { add_error("Invalid source operand for MOV", instr.line, instr.column); return; }
    } else if (auto dst_mem = dynamic_cast<const MemoryReferenceExpression*>(dst)) {
         if (auto src_reg = dynamic_cast<const RegisterExpression*>(src)) {
             uint8_t src_reg_num = get_register_number(src_reg->name);
             bool is_extended = (src_reg_num >= 8 && src_reg_num <= 15);
             if (is_simple_register_indirect(*dst_mem)) {
                 emit_byte(static_cast<uint8_t>(Opcode::STORER)); emit_byte(src_reg_num);
                 auto base_reg = static_cast<const RegisterExpression*>(dst_mem->base.get());
                 emit_byte(get_register_number(base_reg->name));
             } else {
                 if (is_extended) emit_byte(static_cast<uint8_t>(Opcode::STOREX));
                 else emit_byte(static_cast<uint8_t>(Opcode::STORE));
                 emit_byte(src_reg_num);
                 bool is_symbol = false; std::string symbol_name;
                 int64_t val = 0;
                 if (!resolve_memory_operand_address(*dst_mem, val, is_symbol, symbol_name)) {
                     add_error("Unsupported memory operand for MOV destination", instr.line, instr.column);
                     return;
                 }
                 if (is_extended) {
                     if (is_symbol) emit_forward_ref(symbol_name, 8);
                     else for(int i=0; i<8; ++i) emit_byte((uint8_t)((val >> (i*8)) & 0xFF));
                 } else {
                     if (is_symbol) emit_forward_ref(symbol_name, 4);
                     else emit_dword((uint32_t)val);
                 }
             }
         } else { add_error("MOV to memory requires register source", instr.line, instr.column); return; }
    } else { add_error("Invalid destination operand for MOV", instr.line, instr.column); return; }
}

void Assembler::AssemblerEngine::enc_alu_2reg(const Instruction& instr, uint8_t opcode) {
    const std::string& mnemonic = instr.mnemonic;
    const bool supports_three_operand = (mnemonic == "MOD" ||
                         mnemonic == "ADD64" || mnemonic == "SUB64" ||
                         mnemonic == "MUL64" || mnemonic == "DIV64" ||
                         mnemonic == "MOD64" || mnemonic == "AND64" ||
                         mnemonic == "OR64" || mnemonic == "XOR64");
    if (instr.operands.size() != 2 && instr.operands.size() != 3) { add_error(mnemonic + " requires 2 or 3 operands", instr.line, instr.column); return; }
    if (instr.operands.size() == 3 && !supports_three_operand) { add_error(mnemonic + " 3-operand form is only supported for MOD and select 64-bit ALU ops", instr.line, instr.column); return; }
    for (size_t i = 0; i < instr.operands.size(); ++i) {
        const auto& operand = instr.operands[i];
        if (!dynamic_cast<const RegisterExpression*>(operand.get())) {
            std::string operand_type = "unknown";
            if (dynamic_cast<const ImmediateExpression*>(operand.get())) operand_type = "immediate";
            else if (dynamic_cast<const IdentifierExpression*>(operand.get())) operand_type = "identifier";
            else if (dynamic_cast<const StringLiteralExpression*>(operand.get())) operand_type = "string";
            add_error(mnemonic + " operand " + std::to_string(i+1) + " must be a register (got " + operand_type + ")", instr.line, instr.column);
            return;
        }
    }
    if (instr.operands.size() == 2) {
        emit_byte(opcode);
        auto dest_reg = static_cast<const RegisterExpression*>(instr.operands[0].get());
        auto src_reg = static_cast<const RegisterExpression*>(instr.operands[1].get());
        emit_byte(get_register_number(dest_reg->name));
        emit_byte(get_register_number(src_reg->name));
    } else {
        auto dest_reg = static_cast<const RegisterExpression*>(instr.operands[0].get());
        auto src1_reg = static_cast<const RegisterExpression*>(instr.operands[1].get());
        auto src2_reg = static_cast<const RegisterExpression*>(instr.operands[2].get());
        uint8_t dest_num = get_register_number(dest_reg->name);
        uint8_t src1_num = get_register_number(src1_reg->name);
        uint8_t src2_num = get_register_number(src2_reg->name);
        if (dest_num != src1_num) {
            const bool is_extended = (dest_num >= 8 || src1_num >= 8);
            int dest_bits = 0; int src1_bits = 0;
            auto dest_it = register_sizes.find(dest_reg->name);
            if (dest_it != register_sizes.end()) dest_bits = dest_it->second;
            auto src1_it = register_sizes.find(src1_reg->name);
            if (src1_it != register_sizes.end()) src1_bits = src1_it->second;
            const bool is_64bit_move = (dest_bits == 64 || src1_bits == 64);
            emit_byte(static_cast<uint8_t>(is_extended ? Opcode::MOVEX : (is_64bit_move ? Opcode::MOV64 : Opcode::MOV)));
            emit_byte(dest_num); emit_byte(src1_num);
        }
        emit_byte(opcode); emit_byte(dest_num); emit_byte(src2_num);
    }
}

void Assembler::AssemblerEngine::enc_alu_3reg(const Instruction& instr, uint8_t opcode) {
    const std::string& mnemonic = instr.mnemonic;
    emit_byte(opcode);
    if (instr.operands.size() == 2) {
        auto dest_expr = dynamic_cast<const RegisterExpression*>(instr.operands[0].get());
        auto src_expr = dynamic_cast<const RegisterExpression*>(instr.operands[1].get());
        if (!dest_expr || !src_expr) { add_error(mnemonic + " requires register operands", instr.line, instr.column); return; }
        uint8_t dest = get_register_number(dest_expr->name);
        uint8_t src = get_register_number(src_expr->name);
        emit_byte(dest); emit_byte(dest); emit_byte(src);
    } else if (instr.operands.size() == 3) {
        if (auto reg_expr = dynamic_cast<const RegisterExpression*>(instr.operands[0].get())) emit_byte(get_register_number(reg_expr->name));
        else { add_error("First operand must be a destination register", instr.line, instr.column); return; }
        if (auto reg_expr = dynamic_cast<const RegisterExpression*>(instr.operands[1].get())) emit_byte(get_register_number(reg_expr->name));
        else { add_error("Second operand must be a source register", instr.line, instr.column); return; }
        if (auto reg_expr = dynamic_cast<const RegisterExpression*>(instr.operands[2].get())) emit_byte(get_register_number(reg_expr->name));
        else { add_error("Third operand must be a source register", instr.line, instr.column); return; }
    } else { add_error(mnemonic + " requires 2 or 3 operands", instr.line, instr.column); return; }
}

void Assembler::AssemblerEngine::enc_mode(const Instruction& instr, uint8_t opcode) {
    emit_byte(opcode);
    if (instr.operands.size() != 0) { add_error(instr.mnemonic + " requires no operands", instr.line, instr.column); return; }
}

void Assembler::AssemblerEngine::enc_jump(const Instruction& instr, uint8_t opcode) {
    emit_byte(opcode);
    if (instr.operands.size() != 1) { add_error(instr.mnemonic + " requires 1 operand", instr.line, instr.column); return; }
    bool is_symbol; std::string symbol_name;
    int64_t value = evaluate_expression(*instr.operands[0], is_symbol, symbol_name);
    if (is_symbol) emit_forward_ref(symbol_name, 4);
    else emit_dword(static_cast<uint32_t>(value));
}

void Assembler::AssemblerEngine::enc_single_reg(const Instruction& instr, uint8_t opcode) {
    emit_byte(opcode);
    if (instr.operands.size() != 1) { add_error(instr.mnemonic + " requires 1 operand", instr.line, instr.column); return; }
    if (auto reg_expr = dynamic_cast<const RegisterExpression*>(instr.operands[0].get())) emit_byte(get_register_number(reg_expr->name));
    else { add_error("Operand must be a register", instr.line, instr.column); return; }
}

void Assembler::AssemblerEngine::enc_io(const Instruction& instr, uint8_t opcode) {
    emit_byte(opcode);
    if (instr.operands.size() != 2) { add_error(instr.mnemonic + " requires 2 operands", instr.line, instr.column); return; }
    if (auto reg_expr = dynamic_cast<const RegisterExpression*>(instr.operands[0].get())) emit_byte(get_register_number(reg_expr->name));
    else { add_error("First operand must be a register", instr.line, instr.column); return; }
    bool is_symbol; std::string symbol_name;
    int64_t port_value = evaluate_expression(*instr.operands[1], is_symbol, symbol_name);
    if (is_symbol) { add_error("Port number must be an immediate value", instr.line, instr.column); return; }
    else emit_byte(static_cast<uint8_t>(port_value));
}

void Assembler::AssemblerEngine::enc_load_store(const Instruction& instr, uint8_t opcode) {
    if (instr.operands.size() != 2) { add_error(instr.mnemonic + " requires 2 operands", instr.line, instr.column); return; }
    uint8_t dst_reg_num;
    if (auto reg_expr = dynamic_cast<const RegisterExpression*>(instr.operands[0].get())) dst_reg_num = get_register_number(reg_expr->name);
    else { add_error("First operand must be a register", instr.line, instr.column); return; }
    auto& second_operand = instr.operands[1];
    if (is_bracket_register_syntax(second_operand.get())) {
        auto mem_expr = static_cast<const MemoryReferenceExpression*>(second_operand.get());
        auto base_reg = static_cast<const RegisterExpression*>(mem_expr->base.get());
        uint8_t addr_reg_num = get_register_number(base_reg->name);
        if (instr.mnemonic == "LOAD") {
            emit_byte(static_cast<uint8_t>(Opcode::LOADR)); emit_byte(dst_reg_num); emit_byte(addr_reg_num);
        } else if (instr.mnemonic == "STORE") {
            emit_byte(static_cast<uint8_t>(Opcode::STORER)); emit_byte(addr_reg_num); emit_byte(dst_reg_num);
        } else { add_error(instr.mnemonic + " with register indirect addressing not supported", instr.line, instr.column); }
        return;
    }
    emit_byte(opcode); emit_byte(dst_reg_num);
    if (instr.mnemonic == "LOADR" || instr.mnemonic == "STORER") {
        if (auto reg_expr = dynamic_cast<const RegisterExpression*>(second_operand.get())) emit_byte(get_register_number(reg_expr->name));
        else { add_error("Second operand must be a register for " + instr.mnemonic, instr.line, instr.column); return; }
    } else {
        bool is_symbol = false; std::string symbol_name;
        int64_t addr_value = 0;
        if (auto mem_expr = dynamic_cast<const MemoryReferenceExpression*>(second_operand.get())) {
            if (!resolve_memory_operand_address(*mem_expr, addr_value, is_symbol, symbol_name)) {
                add_error("Unsupported memory operand for " + instr.mnemonic, instr.line, instr.column);
                return;
            }
        } else {
            addr_value = evaluate_expression(*second_operand, is_symbol, symbol_name);
        }
        if (is_symbol) emit_forward_ref(symbol_name, 4);
        else emit_dword(static_cast<uint32_t>(addr_value));
    }
}

void Assembler::AssemblerEngine::enc_loadex_storex(const Instruction& instr, uint8_t opcode) {
    emit_byte(opcode);
    if (instr.operands.size() != 2) { add_error(instr.mnemonic + " requires 2 operands", instr.line, instr.column); return; }
    if (auto reg_expr = dynamic_cast<const RegisterExpression*>(instr.operands[0].get())) emit_byte(get_register_number(reg_expr->name));
    else { add_error("First operand must be a register", instr.line, instr.column); return; }
    bool is_symbol; std::string symbol_name;
    int64_t addr_value = evaluate_expression(*instr.operands[1], is_symbol, symbol_name);
    if (is_symbol) emit_forward_ref(symbol_name, 8);
    else for (int i = 0; i < 8; i++) emit_byte(static_cast<uint8_t>((addr_value >> (i * 8)) & 0xFF));
}

void Assembler::AssemblerEngine::enc_shift(const Instruction& instr, uint8_t opcode) {
    emit_byte(opcode);
    if (instr.operands.size() != 2) { add_error(instr.mnemonic + " requires 2 operands", instr.line, instr.column); return; }
    if (auto reg_expr = dynamic_cast<const RegisterExpression*>(instr.operands[0].get())) emit_byte(get_register_number(reg_expr->name));
    else { add_error("First operand must be a register", instr.line, instr.column); return; }
    bool is_symbol; std::string symbol_name;
    int64_t value = evaluate_expression(*instr.operands[1], is_symbol, symbol_name);
    if (is_symbol) { add_error("Shift amount must be an immediate value", instr.line, instr.column); return; }
    else emit_byte(static_cast<uint8_t>(value));
}

void Assembler::AssemblerEngine::enc_fpu_ldst(const Instruction& instr, uint8_t opcode) {
    emit_byte(opcode);
    if (instr.operands.size() != 1) { add_error(instr.mnemonic + " requires 1 operand", instr.line, instr.column); return; }
    bool is_symbol; std::string symbol_name;
    if (instr.mnemonic == "FLD") {
        if (auto imm_expr = dynamic_cast<const ImmediateExpression*>(instr.operands[0].get())) {
            emit_byte(0x02);
            double double_val = static_cast<double>(imm_expr->value);
            uint64_t raw_double = safe_bitcast<uint64_t>(double_val);
            for (int i = 0; i < 8; ++i) emit_byte(static_cast<uint8_t>((raw_double >> (8 * i)) & 0xFF));
            return;
        }
    }
    emit_byte(0x01);
    int64_t addr_value = evaluate_expression(*instr.operands[0], is_symbol, symbol_name);
    if (is_symbol) emit_forward_ref(symbol_name, 4);
    else { emit_byte(static_cast<uint8_t>(addr_value & 0xFF)); emit_byte(static_cast<uint8_t>((addr_value >> 8) & 0xFF)); emit_byte(static_cast<uint8_t>((addr_value >> 16) & 0xFF)); emit_byte(static_cast<uint8_t>((addr_value >> 24) & 0xFF)); }
}

void Assembler::AssemblerEngine::enc_fpu_arith(const Instruction& instr, uint8_t opcode) {
    emit_byte(opcode);
    if (instr.operands.size() != 1) { add_error(instr.mnemonic + " requires 1 operand", instr.line, instr.column); return; }
    bool is_symbol; std::string symbol_name;
    if (auto imm_expr = dynamic_cast<const ImmediateExpression*>(instr.operands[0].get())) {
        emit_byte(0x02);
        double double_val = static_cast<double>(imm_expr->value);
        uint64_t raw_double = safe_bitcast<uint64_t>(double_val);
        for (int i = 0; i < 8; ++i) emit_byte(static_cast<uint8_t>((raw_double >> (8 * i)) & 0xFF));
        return;
    }
    emit_byte(0x01);
    int64_t addr_value = evaluate_expression(*instr.operands[0], is_symbol, symbol_name);
    if (is_symbol) emit_forward_ref(symbol_name, 4);
    else { emit_byte(static_cast<uint8_t>(addr_value & 0xFF)); emit_byte(static_cast<uint8_t>((addr_value >> 8) & 0xFF)); emit_byte(static_cast<uint8_t>((addr_value >> 16) & 0xFF)); emit_byte(static_cast<uint8_t>((addr_value >> 24) & 0xFF)); }
}

void Assembler::AssemblerEngine::enc_fild(const Instruction& instr, uint8_t opcode) {
    emit_byte(opcode);
    if (instr.operands.size() != 1) { add_error("FILD requires 1 operand", instr.line, instr.column); return; }
    bool is_symbol; std::string symbol_name;
    if (auto imm_expr = dynamic_cast<const ImmediateExpression*>(instr.operands[0].get())) {
        emit_byte(0x00); int32_t int_val = static_cast<int32_t>(imm_expr->value);
        uint32_t raw_int = static_cast<uint32_t>(int_val);
        emit_byte(static_cast<uint8_t>(raw_int & 0xFF)); emit_byte(static_cast<uint8_t>((raw_int >> 8) & 0xFF));
        emit_byte(static_cast<uint8_t>((raw_int >> 16) & 0xFF)); emit_byte(static_cast<uint8_t>((raw_int >> 24) & 0xFF));
        return;
    }
    emit_byte(0x01);
    int64_t addr_value = evaluate_expression(*instr.operands[0], is_symbol, symbol_name);
    if (is_symbol) emit_forward_ref(symbol_name, 4);
    else { emit_byte(static_cast<uint8_t>(addr_value & 0xFF)); emit_byte(static_cast<uint8_t>((addr_value >> 8) & 0xFF)); emit_byte(static_cast<uint8_t>((addr_value >> 16) & 0xFF)); emit_byte(static_cast<uint8_t>((addr_value >> 24) & 0xFF)); }
}

void Assembler::AssemblerEngine::enc_fist(const Instruction& instr, uint8_t opcode) {
    emit_byte(opcode);
    if (instr.operands.size() != 1) { add_error(instr.mnemonic + " requires 1 operand", instr.line, instr.column); return; }
    emit_byte(0x01);
    bool is_symbol; std::string symbol_name;
    int64_t addr_value = evaluate_expression(*instr.operands[0], is_symbol, symbol_name);
    if (is_symbol) emit_forward_ref(symbol_name, 4);
    else { emit_byte(static_cast<uint8_t>(addr_value & 0xFF)); emit_byte(static_cast<uint8_t>((addr_value >> 8) & 0xFF)); emit_byte(static_cast<uint8_t>((addr_value >> 16) & 0xFF)); emit_byte(static_cast<uint8_t>((addr_value >> 24) & 0xFF)); }
}

void Assembler::AssemblerEngine::enc_fpu_ctrl(const Instruction& instr, uint8_t opcode) {
    emit_byte(opcode);
    if (instr.operands.size() != 1) { add_error(instr.mnemonic + " requires 1 operand", instr.line, instr.column); return; }
    bool is_symbol; std::string symbol_name;
    int64_t addr_value = evaluate_expression(*instr.operands[0], is_symbol, symbol_name);
    if (is_symbol) emit_forward_ref(symbol_name, 4);
    else { emit_byte(static_cast<uint8_t>(addr_value & 0xFF)); emit_byte(static_cast<uint8_t>((addr_value >> 8) & 0xFF)); emit_byte(static_cast<uint8_t>((addr_value >> 16) & 0xFF)); emit_byte(static_cast<uint8_t>((addr_value >> 24) & 0xFF)); }
}

void Assembler::AssemblerEngine::enc_fstsw(const Instruction& instr, uint8_t opcode) {
    emit_byte(opcode);
    if (instr.operands.size() != 1) { add_error("FSTSW requires 1 operand", instr.line, instr.column); return; }
    if (auto reg_expr = dynamic_cast<const RegisterExpression*>(instr.operands[0].get())) {
        if (reg_expr->name == "R0" || reg_expr->name == "AX") { emit_byte(0x01); return; }
    }
    emit_byte(0x00);
    bool is_symbol; std::string symbol_name;
    int64_t addr_value = evaluate_expression(*instr.operands[0], is_symbol, symbol_name);
    if (is_symbol) emit_forward_ref(symbol_name, 4);
    else { emit_byte(static_cast<uint8_t>(addr_value & 0xFF)); emit_byte(static_cast<uint8_t>((addr_value >> 8) & 0xFF)); emit_byte(static_cast<uint8_t>((addr_value >> 16) & 0xFF)); emit_byte(static_cast<uint8_t>((addr_value >> 24) & 0xFF)); }
}

void Assembler::AssemblerEngine::enc_sse_cmp(const Instruction& instr, uint8_t opcode) {
    emit_byte(opcode);
    if (instr.operands.size() != 3) { add_error(instr.mnemonic + " requires 3 operands (dst, src, predicate)", instr.line, instr.column); return; }
    auto dst = dynamic_cast<const RegisterExpression*>(instr.operands[0].get());
    auto src = dynamic_cast<const RegisterExpression*>(instr.operands[1].get());
    auto pred = dynamic_cast<const ImmediateExpression*>(instr.operands[2].get());
    if (!dst || !src || !pred) { add_error(instr.mnemonic + " operands must be: register, register, immediate", instr.line, instr.column); return; }
    emit_byte(get_register_number(dst->name)); emit_byte(get_register_number(src->name)); emit_byte(static_cast<uint8_t>(pred->value & 0xFF));
}

void Assembler::AssemblerEngine::enc_sse(const Instruction& instr, uint8_t opcode) {
    emit_byte(opcode);
    if (instr.operands.size() != 2) { add_error(instr.mnemonic + " requires 2 operands", instr.line, instr.column); return; }
    auto dst = dynamic_cast<const RegisterExpression*>(instr.operands[0].get());
    auto src = dynamic_cast<const RegisterExpression*>(instr.operands[1].get());
    if (!dst || !src) { add_error(instr.mnemonic + " operands must be registers", instr.line, instr.column); return; }
    emit_byte(get_register_number(dst->name)); emit_byte(get_register_number(src->name));
}

void Assembler::AssemblerEngine::enc_no_operands(const Instruction& instr, uint8_t opcode) {
    emit_byte(opcode);
    if (instr.operands.size() != 0) { add_error(instr.mnemonic + " requires no operands", instr.line, instr.column); return; }
}

void Assembler::AssemblerEngine::enc_int(const Instruction& instr, uint8_t opcode) {
    emit_byte(opcode);
    if (instr.operands.size() != 1) { add_error("INT requires exactly 1 operand (interrupt vector)", instr.line, instr.column); return; }
    bool is_symbol; std::string symbol;
    int64_t vector = evaluate_expression(*instr.operands[0], is_symbol, symbol);
    if (is_symbol) { add_error("INT vector must be an immediate value, not a symbol", instr.line, instr.column); return; }
    if (vector < 0 || vector > 255) { add_error("INT vector must be between 0 and 255", instr.line, instr.column); return; }
    emit_byte(static_cast<uint8_t>(vector));
}

int64_t Assembler::AssemblerEngine::evaluate_expression(const Assembler::Expression& expr, bool& is_symbol_ref, std::string& symbol_name) {
    is_symbol_ref = false;
    symbol_name.clear();

    switch (expr.type) {
        case Assembler::ASTNodeType::IMMEDIATE: {
            const auto& imm = static_cast<const Assembler::ImmediateExpression&>(expr);
            return imm.value;
        }

        case Assembler::ASTNodeType::IDENTIFIER: {
            const auto& id = static_cast<const Assembler::IdentifierExpression&>(expr);
            auto it = symbol_table.find(id.name);
            if (it != symbol_table.end() && it->second.defined) {
                return it->second.address;
            } else {
                is_symbol_ref = true;
                symbol_name = id.name;
                return 0; // Will be resolved later
            }
        }
        
        case Assembler::ASTNodeType::MEMORY_REF: {
            const auto& mem = static_cast<const Assembler::MemoryReferenceExpression&>(expr);
            int64_t address = 0;
            if (!resolve_memory_operand_address(mem, address, is_symbol_ref, symbol_name)) {
                add_error("Unsupported memory reference form", expr.line, expr.column);
                return 0;
            }
            return address;
        }

        case Assembler::ASTNodeType::REGISTER: {
            const auto& reg = static_cast<const Assembler::RegisterExpression&>(expr);
            return get_register_number(reg.name);
        }

        default:
            add_error("Cannot evaluate expression");
            return 0;
    }
}

uint8_t Assembler::AssemblerEngine::get_register_number(const std::string& reg_name) {
    auto it = register_to_number.find(reg_name);
    if (it != register_to_number.end()) {
        return it->second;
    }
    add_error("Unknown register: " + reg_name);
    return 0;
}

uint8_t Assembler::AssemblerEngine::get_opcode(const std::string& mnemonic) {
    auto it = mnemonic_to_opcode.find(mnemonic);
    if (it != mnemonic_to_opcode.end()) {
        return it->second;
    }
    return 0xFF; // Invalid opcode
}

void Assembler::AssemblerEngine::emit_byte(uint8_t byte) {
    bytecode.push_back(byte);
    current_address++;
}

void Assembler::AssemblerEngine::emit_word(uint16_t word) {
    emit_byte(word & 0xFF);
    emit_byte((word >> 8) & 0xFF);
}

void Assembler::AssemblerEngine::emit_dword(uint32_t dword) {
    emit_byte(dword & 0xFF);
    emit_byte((dword >> 8) & 0xFF);
    emit_byte((dword >> 16) & 0xFF);
    emit_byte((dword >> 24) & 0xFF);
}

void Assembler::AssemblerEngine::emit_qword(uint64_t qword) {
    emit_byte(qword & 0xFF);
    emit_byte((qword >> 8) & 0xFF);
    emit_byte((qword >> 16) & 0xFF);
    emit_byte((qword >> 24) & 0xFF);
    emit_byte((qword >> 32) & 0xFF);
    emit_byte((qword >> 40) & 0xFF);
    emit_byte((qword >> 48) & 0xFF);
    emit_byte((qword >> 56) & 0xFF);
}

void Assembler::AssemblerEngine::emit_forward_ref(const std::string& symbol, size_t size, bool relative) {
    forward_refs.push_back({current_address, symbol, size, relative});
        DEBUG_DETAIL(Logging::DebugCategory::ASM_FORWARD_REF, "emit_forward_ref: symbol='{}', address=0x{:04X}, size={}, relative={}", symbol, current_address, size, relative);

    // Emit placeholder bytes
    for (size_t i = 0; i < size; ++i) {
        emit_byte(0);
    }
}

bool Assembler::AssemblerEngine::is_bracket_register_syntax(const Assembler::Expression* operand) {
    if (auto mem_expr = dynamic_cast<const Assembler::MemoryReferenceExpression*>(operand)) {
        if (dynamic_cast<const Assembler::RegisterExpression*>(mem_expr->base.get()) &&
            !mem_expr->offset && !mem_expr->index && !mem_expr->displacement && !mem_expr->symbol) {
            return true;
        }
    }
    return false;
}

bool Assembler::AssemblerEngine::is_simple_register_indirect(const MemoryReferenceExpression& mem) const {
    return dynamic_cast<const RegisterExpression*>(mem.base.get()) != nullptr &&
           !mem.offset && !mem.index && !mem.displacement && !mem.symbol;
}

bool Assembler::AssemblerEngine::resolve_memory_operand_address(const MemoryReferenceExpression& mem, int64_t& address, bool& is_symbol_ref, std::string& symbol_name) {
    address = 0;
    is_symbol_ref = false;
    symbol_name.clear();

    if (is_simple_register_indirect(mem)) {
        return false;
    }

    if (mem.index) {
        add_error("Indexed memory operands are not supported by the current backend", mem.line, mem.column);
        return false;
    }

    if (dynamic_cast<const RegisterExpression*>(mem.base.get()) != nullptr) {
        add_error("Register-relative memory operands with displacement are not supported by the current backend", mem.line, mem.column);
        return false;
    }

    auto accumulate_term = [&](const Expression* term) -> bool {
        if (!term) {
            return true;
        }

        if (auto imm = dynamic_cast<const ImmediateExpression*>(term)) {
            address += imm->value;
            return true;
        }

        if (auto ident = dynamic_cast<const IdentifierExpression*>(term)) {
            if (is_symbol_ref && symbol_name != ident->name) {
                add_error("Multiple symbolic terms in one absolute memory operand are not supported", mem.line, mem.column);
                return false;
            }

            auto it = symbol_table.find(ident->name);
            if (it != symbol_table.end() && it->second.defined) {
                address += it->second.address;
            } else {
                is_symbol_ref = true;
                symbol_name = ident->name;
            }
            return true;
        }

        add_error("Unsupported term in memory operand", mem.line, mem.column);
        return false;
    };

    if (!accumulate_term(mem.base.get())) {
        return false;
    }
    if (!accumulate_term(mem.offset.get())) {
        return false;
    }
    if (!accumulate_term(mem.displacement.get())) {
        return false;
    }
    if (mem.symbol && !accumulate_term(mem.symbol.get())) {
        return false;
    }

    return true;
}

size_t Assembler::AssemblerEngine::get_instruction_size(const std::string& mnemonic, const std::vector<std::unique_ptr<Assembler::Expression>>& operands) {
    // Mirror encode_instruction()'s x64 auto-upgrade logic so label addresses
    // computed during the first pass match the bytes emitted in the second pass.
    std::string effective_mnemonic = mnemonic;
    bool use_64bit = false;
    for (const auto& operand : operands) {
        if (auto reg_expr = dynamic_cast<const Assembler::RegisterExpression*>(operand.get())) {
            auto it = register_sizes.find(reg_expr->name);
            if (it != register_sizes.end() && it->second == 64) {
                use_64bit = true;
                break;
            }
        }
    }

    if (use_64bit) {
        if (effective_mnemonic == "MOV") effective_mnemonic = "MOV64";
        else if (effective_mnemonic == "ADD") effective_mnemonic = "ADD64";
        else if (effective_mnemonic == "SUB") effective_mnemonic = "SUB64";
        else if (effective_mnemonic == "MUL") effective_mnemonic = "MUL64";
        else if (effective_mnemonic == "DIV") effective_mnemonic = "DIV64";
        else if (effective_mnemonic == "AND") effective_mnemonic = "AND64";
        else if (effective_mnemonic == "OR") effective_mnemonic = "OR64";
        else if (effective_mnemonic == "XOR") effective_mnemonic = "XOR64";
        else if (effective_mnemonic == "SHL") effective_mnemonic = "SHL64";
        else if (effective_mnemonic == "SHR") effective_mnemonic = "SHR64";
        else if (effective_mnemonic == "CMP") effective_mnemonic = "CMP64";
        else if (effective_mnemonic == "NOT") effective_mnemonic = "NOT64";
        else if (effective_mnemonic == "INC") effective_mnemonic = "INC64";
        else if (effective_mnemonic == "DEC") effective_mnemonic = "DEC64";
        else if (effective_mnemonic == "MOD") effective_mnemonic = "MOD64";
        else if (effective_mnemonic == "LOAD_IMM") effective_mnemonic = "LOAD_IMM64";
        else if (effective_mnemonic == "LOAD") effective_mnemonic = "LOADEX";
        else if (effective_mnemonic == "STORE") effective_mnemonic = "STOREX";
    }

    // Basic instruction size calculations for Demi Engine
    if (effective_mnemonic == "NOP" || effective_mnemonic == "HALT" || effective_mnemonic == "RET" ||
        effective_mnemonic == "PUSH_FLAG" || effective_mnemonic == "POP_FLAG" ||
        // Interrupt operations (no operands)
        effective_mnemonic == "CLI" || effective_mnemonic == "STI" || effective_mnemonic == "IRET" ||
        // Scalar x87 no-operand instructions
        effective_mnemonic == "FINIT" || effective_mnemonic == "FCLEX" ||
        effective_mnemonic == "FSIN" || effective_mnemonic == "FCOS" ||
        effective_mnemonic == "FTAN" || effective_mnemonic == "FSQRT" ||
        effective_mnemonic == "FABS" || effective_mnemonic == "FCHS" ||
        effective_mnemonic == "FCOMPP" || effective_mnemonic == "FUCOMPP" ||
        // SIMD instructions with no operands
        effective_mnemonic == "VADD" || effective_mnemonic == "VMUL" || effective_mnemonic == "VDOT" ||
        effective_mnemonic == "VMAX" || effective_mnemonic == "VBROADCAST" || effective_mnemonic == "VCMPGT" ||
        effective_mnemonic == "PACKB" || effective_mnemonic == "UNPACKB") {
        return 1;
    } else if (effective_mnemonic == "INT") {
        return 2; // opcode + vector byte
    } else if (effective_mnemonic == "LOAD_IMM") {
        // LOAD_IMM always uses 4-byte immediate (6 bytes total) for GPRs
        // This matches what encode_instruction emits (emit_dword for CMP rewrite)
        return 6; // opcode + register + 4-byte immediate
    } else if (effective_mnemonic == "LOAD_IMM64") {
        return 10; // opcode + register + 8-byte immediate
    } else if (effective_mnemonic == "CMP" || effective_mnemonic == "CMP64" || effective_mnemonic == "CMPEX") {
        // CMP with immediate requires LOAD_IMM + CMP
        // CMP64 with imm: LOAD_IMM64 (10 bytes) + CMP64 (3 bytes) = 13 bytes
        // CMP with imm: LOAD_IMM (6 bytes) + CMP (3 bytes) = 9 bytes
        if (operands.size() >= 2) {
            bool second_is_reg = dynamic_cast<const RegisterExpression*>(operands[1].get()) != nullptr;
            if (!second_is_reg) {
                // Check if using 64-bit variant
                if (effective_mnemonic == "CMP64" || effective_mnemonic == "CMPEX") {
                    return 13; // LOAD_IMM64 (10) + CMP64 (3)
                }
                return 9; // LOAD_IMM (6 bytes) + CMP (3 bytes)
            }
        }
        return 3; // opcode + reg1 + reg2
    } else if (effective_mnemonic == "MUL64" || effective_mnemonic == "DIV64" ||
               effective_mnemonic == "AND64" || effective_mnemonic == "OR64" ||
               effective_mnemonic == "XOR64" || effective_mnemonic == "MOD64" ||
               effective_mnemonic == "MULEX" || effective_mnemonic == "DIVEX") {
        return 4; // opcode + dest_reg + src_reg1 + src_reg2
    } else if (effective_mnemonic == "ADD" || effective_mnemonic == "SUB" || effective_mnemonic == "MOV" ||
               effective_mnemonic == "MUL" || effective_mnemonic == "DIV" ||
               effective_mnemonic == "MOD" || effective_mnemonic == "AND" || effective_mnemonic == "OR" ||
               effective_mnemonic == "XOR" || effective_mnemonic == "ADD64" || effective_mnemonic == "SUB64" ||
               effective_mnemonic == "MOV64" ||
               // SSE-style reg-reg SIMD (opcode + dst + src)
               effective_mnemonic == "MOVAPS" || effective_mnemonic == "MOVUPS" || effective_mnemonic == "MOVPS" ||
               effective_mnemonic == "ADDPS" || effective_mnemonic == "SUBPS" || effective_mnemonic == "MULPS" ||
               effective_mnemonic == "DIVPS" || effective_mnemonic == "SQRTPS" || effective_mnemonic == "MAXPS" ||
               effective_mnemonic == "MINPS" || effective_mnemonic == "ANDPS" || effective_mnemonic == "ORPS" ||
               effective_mnemonic == "XORPS" || effective_mnemonic == "CMPPS" ||
               effective_mnemonic == "MOVAPD" || effective_mnemonic == "MOVUPD" || effective_mnemonic == "ADDPD" ||
               effective_mnemonic == "SUBPD" || effective_mnemonic == "MULPD" || effective_mnemonic == "DIVPD" ||
               effective_mnemonic == "SQRTPD" || effective_mnemonic == "MAXPD" || effective_mnemonic == "MINPD" ||
               effective_mnemonic == "ANDPD" || effective_mnemonic == "ORPD" || effective_mnemonic == "XORPD" ||
               effective_mnemonic == "CMPPD") {
        return 3; // opcode + reg1 + reg2 (2-operand instructions)
    } else if (effective_mnemonic == "JMP" || effective_mnemonic == "JZ" || effective_mnemonic == "JE" ||
               effective_mnemonic == "JNZ" || effective_mnemonic == "JNE" ||
               effective_mnemonic == "JS" || effective_mnemonic == "JNS" || effective_mnemonic == "JC" ||
               effective_mnemonic == "JNC" || effective_mnemonic == "JO" || effective_mnemonic == "JNO" ||
               effective_mnemonic == "JG" || effective_mnemonic == "JL" || effective_mnemonic == "JGE" ||
               effective_mnemonic == "JLE" || effective_mnemonic == "CALL") {
        return 5; // opcode + 4-byte address
    } else if (effective_mnemonic == "PUSH" || effective_mnemonic == "POP" || effective_mnemonic == "INC" ||
               effective_mnemonic == "DEC" || effective_mnemonic == "NOT" || effective_mnemonic == "INC64" ||
               effective_mnemonic == "DEC64" || effective_mnemonic == "NOT64" ||
               effective_mnemonic == "PUSHEX" || effective_mnemonic == "POPEX") {
        return 2; // opcode + register
    } else if (effective_mnemonic == "LOADR" || effective_mnemonic == "STORER") {
        DEBUG_DETAIL(Logging::DebugCategory::ASM_ENCODING, "[PASS1_SIZE] {} -> 3 bytes (explicit LOADR/STORER)", effective_mnemonic);
        return 3; // opcode + register + register
    } else if (effective_mnemonic == "FLD" || effective_mnemonic == "FADD" ||
               effective_mnemonic == "FSUB" || effective_mnemonic == "FMUL" ||
               effective_mnemonic == "FDIV") {
        if (!operands.empty() && dynamic_cast<const ImmediateExpression*>(operands[0].get()) != nullptr) {
            return 10; // opcode + operand kind + 8-byte immediate double
        }
        return 6; // opcode + operand kind + 4-byte VM address
    } else if (effective_mnemonic == "FST" || effective_mnemonic == "FSTP" ||
               effective_mnemonic == "FILD" || effective_mnemonic == "FIST" ||
               effective_mnemonic == "FISTP") {
        return 6;
    } else if (effective_mnemonic == "FSTCW" || effective_mnemonic == "FLDCW") {
        return 5;
    } else if (effective_mnemonic == "FSTSW") {
        if (!operands.empty() && dynamic_cast<const RegisterExpression*>(operands[0].get()) != nullptr) {
            return 2;
        }
        return 6;
    } else if (effective_mnemonic == "LOAD" || effective_mnemonic == "STORE" || effective_mnemonic == "LEA") {
        // Check if EITHER operand is [register] syntax -> will become LOADR/STORER (3 bytes)
        if (operands.size() >= 2) {
            // Check operand 0 (for STORE [reg], value syntax if it exists)
            if (is_bracket_register_syntax(operands[0].get())) {
                DEBUG_DETAIL(Logging::DebugCategory::ASM_ENCODING, "[PASS1_SIZE] {} with operand[0] bracket syntax -> 3 bytes (will become LOADR/STORER)", effective_mnemonic);
                return 3; // opcode + dest_reg + addr_reg
            }
            // Check operand 1 (for LOAD reg, [reg] or STORE reg, [reg] syntax)
            if (is_bracket_register_syntax(operands[1].get())) {
                DEBUG_DETAIL(Logging::DebugCategory::ASM_ENCODING, "[PASS1_SIZE] {} with operand[1] bracket syntax -> 3 bytes (will become LOADR/STORER)", effective_mnemonic);
                return 3; // opcode + dest_reg + addr_reg
            }
        }
        DEBUG_DETAIL(Logging::DebugCategory::ASM_ENCODING, "[PASS1_SIZE] {} without bracket syntax -> 6 bytes (normal LOAD/STORE)", effective_mnemonic);
        return 6; // opcode + register + 4-byte address
    } else if (effective_mnemonic == "OUT" || effective_mnemonic == "IN" || effective_mnemonic == "OUTB" ||
               effective_mnemonic == "INB" || effective_mnemonic == "OUTW" || effective_mnemonic == "INW" ||
               effective_mnemonic == "OUTL" || effective_mnemonic == "INL" || effective_mnemonic == "OUTSTR" ||
               effective_mnemonic == "INSTR" ||
               effective_mnemonic == "SWAP" || effective_mnemonic == "SHL" ||
               effective_mnemonic == "SHR" || effective_mnemonic == "SHL64" ||
               effective_mnemonic == "SHR64") {
        return 3; // opcode + register + port/immediate/register
    } else if (effective_mnemonic == "LOADEX" || effective_mnemonic == "STOREX") {
        return 10; // opcode + register + 8-byte address
    }

    return 1; // Default size
}

void Assembler::AssemblerEngine::handle_db_directive(const std::vector<std::unique_ptr<Assembler::Expression>>& args) {
    if (args.empty()) {
        add_error(".db directive requires at least one argument");
        return;
    }

    // Check if this is a simple byte list (like standard .db directive)
    bool all_immediates = true;
    for (const auto& arg : args) {
        if (!dynamic_cast<const Assembler::ImmediateExpression*>(arg.get())) {
            all_immediates = false;
            break;
        }
    }

    if (all_immediates) {
        // Standard .db directive: emit bytes directly
        for (const auto& arg : args) {
            bool is_symbol;
            std::string symbol_name;
            int64_t value = evaluate_expression(*arg, is_symbol, symbol_name);
            
            if (is_symbol) {
                emit_forward_ref(symbol_name, 1);
            } else {
                if (value < 0 || value > 255) {
                    add_error(".db byte value " + std::to_string(value) + " out of range (must be 0-255)");
                    return;
                }
                emit_byte(static_cast<uint8_t>(value));
            }
        }
        return;
    }

    // Original string-based DB handling (for compatibility with existing DB instruction tests)
    if (args.size() < 2) {
        add_error(".db directive requires at least a string and length, or immediate value(s)");
        return;
    }
    
    size_t str_idx = 0;
    size_t len_idx = 1;
    // Use static member db_next_addr
    uint32_t target_addr = 0;
    if (args.size() == 3) {
        // Explicit address
        size_t addr_idx = 2;
        if (auto addr_imm = dynamic_cast<const Assembler::ImmediateExpression*>(args[addr_idx].get())) {
            target_addr = static_cast<uint32_t>(addr_imm->value);
        } else {
            add_error(".db address must be an immediate value");
            return;
        }
    } else {
        // Auto address
        target_addr = AssemblerEngine::db_next_addr;
    }
    current_address = target_addr;
    while (bytecode.size() < current_address) {
        bytecode.push_back(0);
    }

    // Emit string (with escapes)
    size_t db_start = current_address;
    size_t db_len = 0;
    size_t actual_string_len = 0;
    if (auto str_lit = dynamic_cast<const Assembler::StringLiteralExpression*>(args[str_idx].get())) {
        actual_string_len = str_lit->value.length();
        for (char c : str_lit->value) {
            emit_byte(static_cast<uint8_t>(c));
            db_len++;
        }
        emit_byte(0); // Null terminator for OUTSTR
        db_len++;
    } else {
        add_error(".db first argument must be a string literal or all arguments must be byte values");
        return;
    }

    // Emit length
    bool is_symbol;
    std::string symbol_name;
    int64_t value = evaluate_expression(*args[len_idx], is_symbol, symbol_name);
    
    // Validate that specified length doesn't exceed actual string length (including null terminator)
    if (!is_symbol && value > static_cast<int64_t>(actual_string_len + 1)) {
        add_error(".db length (" + std::to_string(value) + 
                  ") exceeds actual string length (" + std::to_string(actual_string_len) + ")");
        return;
    }
    
    if (is_symbol) {
        emit_forward_ref(symbol_name, 1);
        db_len++;
    } else {
        emit_byte(static_cast<uint8_t>(value));
        db_len++;
    }
    // Set current_address to end of DB data
    current_address = db_start + db_len;
    // If auto address, increment db_next_addr
    if (args.size() == 2) {
        AssemblerEngine::db_next_addr += db_len;
    }
    
    // Fix any label that should point to this DB data
    // Look for the most recent label that might reference this DB
    if (!last_label_name.empty()) {
        // Update the symbol to point to where the actual string data starts (db_start)
        auto it = symbol_table.find(last_label_name);
        if (it != symbol_table.end()) {
            symbol_table[last_label_name] = Symbol(last_label_name, static_cast<uint32_t>(db_start), true);
        }
        last_label_name.clear(); // Clear after use
    }
}

void Assembler::AssemblerEngine::handle_dw_directive(const std::vector<std::unique_ptr<Assembler::Expression>>& args) {
    for (const auto& arg : args) {
        bool is_symbol;
        std::string symbol_name;
        int64_t value = evaluate_expression(*arg, is_symbol, symbol_name);

        if (is_symbol) {
            emit_forward_ref(symbol_name, 2);
        } else {
            emit_word(static_cast<uint16_t>(value));
        }
    }
}

void Assembler::AssemblerEngine::handle_dd_directive(const std::vector<std::unique_ptr<Assembler::Expression>>& args) {
    for (const auto& arg : args) {
        bool is_symbol;
        std::string symbol_name;
        int64_t value = evaluate_expression(*arg, is_symbol, symbol_name);

        if (is_symbol) {
            emit_forward_ref(symbol_name, 4);
        } else {
            emit_dword(static_cast<uint32_t>(value));
        }
    }
}

void Assembler::AssemblerEngine::handle_string_directive(const std::vector<std::unique_ptr<Assembler::Expression>>& args) {
    for (const auto& arg : args) {
        if (auto str_lit = dynamic_cast<const Assembler::StringLiteralExpression*>(arg.get())) {
            for (char c : str_lit->value) {
                emit_byte(static_cast<uint8_t>(c));
            }
            emit_byte(0); // Null terminator
        } else {
            add_error("String directive requires string literal");
        }
    }
}

void Assembler::AssemblerEngine::handle_equ_directive(const std::vector<std::unique_ptr<Assembler::Expression>>& args) {
    if (args.size() != 2) {
        add_error(".equ directive requires exactly two arguments: name and value");
        return;
    }

    // First argument should be an identifier (the constant name)
    auto ident_expr = dynamic_cast<const Assembler::IdentifierExpression*>(args[0].get());
    if (!ident_expr) {
        add_error(".equ directive: first argument must be an identifier");
        return;
    }

    std::string const_name = ident_expr->name;

    // Second argument is the value
    bool is_symbol;
    std::string symbol_name;
    int64_t value = evaluate_expression(*args[1], is_symbol, symbol_name);

    if (is_symbol) {
        add_error(".equ directive: cannot use forward references for constant value");
        return;
    }

    // Check if constant already exists
    if (symbol_table.find(const_name) != symbol_table.end() && symbol_table[const_name].defined) {
        add_error("Constant '" + const_name + "' already defined");
        return;
    }

    // Add constant to symbol table with the evaluated value
    symbol_table[const_name] = Symbol(const_name, static_cast<uint32_t>(value), true);
    symbol_table[const_name].section = current_section;
}

void Assembler::AssemblerEngine::handle_org_directive(const std::vector<std::unique_ptr<Assembler::Expression>>& args) {
    if (args.size() != 1) {
        add_error(".org directive requires exactly one argument");
        return;
    }

    bool is_symbol;
    std::string symbol_name;
    int64_t value = evaluate_expression(*args[0], is_symbol, symbol_name);

    if (is_symbol) {
        add_error(".org directive cannot use forward references");
        return;
    }

    uint32_t new_address = static_cast<uint32_t>(value);
    
    // Check if .org is moving backwards
    if (new_address < current_address) {
        bool is_section_base_override = false;
        if (current_section == ".data" && current_address == data_section_base) {
            is_section_base_override = true;
            data_section_base = new_address;
        } else if (current_section == ".text" && current_address == text_section_base) {
            is_section_base_override = true;
            // Don't overlap with .data section data
            uint32_t data_end = data_section_base + data_section_size;
            if (new_address < data_end) {
                text_section_base = data_end;
            } else {
                text_section_base = new_address;
            }
        }

        if (!is_section_base_override) {
            std::stringstream ss;
            ss << ".org directive cannot move address backwards from 0x" << std::hex << std::uppercase << current_address 
               << " to 0x" << std::hex << std::uppercase << new_address;
            add_error(ss.str());
            return;
        }
        // Use the adjusted section base (may differ from new_address
        // if it was pushed forward to avoid .data overlap)
        current_address = (current_section == ".data") ? data_section_base : text_section_base;
    } else {
        current_address = new_address;
    }

    // Pad bytecode to the new address
    while (bytecode.size() < current_address) {
        bytecode.push_back(0);
    }
}

void Assembler::AssemblerEngine::handle_align_directive(const std::vector<std::unique_ptr<Assembler::Expression>>& args) {
    if (args.size() != 1) {
        add_error(".align directive requires exactly one argument (alignment boundary)");
        return;
    }

    bool is_symbol;
    std::string symbol_name;
    int64_t alignment = evaluate_expression(*args[0], is_symbol, symbol_name);

    if (is_symbol) {
        add_error(".align directive cannot use forward references");
        return;
    }

    if (alignment <= 0 || (alignment & (alignment - 1)) != 0) {
        add_error(".align directive requires a power of 2 alignment value (1, 2, 4, 8, 16, etc.)");
        return;
    }

    // Calculate aligned address
    uint32_t align_mask = static_cast<uint32_t>(alignment - 1);
    uint32_t aligned_address = (current_address + align_mask) & ~align_mask;

    // Pad with zeros to reach aligned address
    while (current_address < aligned_address) {
        emit_byte(0);
    }
}

void Assembler::AssemblerEngine::handle_bss_directive(const std::vector<std::unique_ptr<Assembler::Expression>>& args) {
    if (args.size() != 1) {
        add_error(".bss directive requires exactly one argument (number of bytes to reserve)");
        return;
    }

    bool is_symbol;
    std::string symbol_name;
    int64_t bytes_to_reserve = evaluate_expression(*args[0], is_symbol, symbol_name);

    if (is_symbol) {
        add_error(".bss directive cannot use forward references");
        return;
    }

    if (bytes_to_reserve < 0) {
        add_error(".bss directive requires a non-negative number of bytes");
        return;
    }

    // Reserve uninitialized space by advancing current_address and padding with zeros
    for (int64_t i = 0; i < bytes_to_reserve; i++) {
        emit_byte(0);
    }
}

void Assembler::AssemblerEngine::handle_memory_directive(const std::vector<std::unique_ptr<Assembler::Expression>>& args) {
    if (args.size() != 1) {
        add_error(".memory directive requires exactly one argument (memory size in bytes)");
        return;
    }

    bool is_symbol;
    std::string symbol_name;
    int64_t size = evaluate_expression(*args[0], is_symbol, symbol_name);

    if (is_symbol) {
        add_error(".memory directive cannot use forward references");
        return;
    }

    if (size < 256) {
        add_error(".memory directive requires at least 256 bytes");
        return;
    }

    if (size > 64 * 1024 * 1024) { // 64MB max
        add_error(".memory directive cannot exceed 64MB (67108864 bytes)");
        return;
    }

    memory_size = static_cast<size_t>(size);
    
    if (!Config::test_mode) {
        Logging::DebugHandler::instance().report(Logging::DebugCategory::ASM_DIRECTIVE, fmt::format("Memory size set to {} bytes ({:.1f} KB)", 
                                                 memory_size, memory_size / 1024.0), Logging::DebugLevel::INFO);
    }
}

void Assembler::AssemblerEngine::resolve_forward_references() {
    for (const auto& ref : forward_refs) {
            DEBUG_DETAIL(Logging::DebugCategory::ASM_FORWARD_REF, "resolve_forward_references: symbol='{}', address=0x{:04X}, size={}, relative={}", ref.symbol, ref.address, ref.size, ref.relative);
        auto it = symbol_table.find(ref.symbol);
        if (it == symbol_table.end() || !it->second.defined) {
            add_error("Undefined symbol: " + ref.symbol);
            continue;
        }

        uint32_t address = it->second.address;
        if (ref.relative) {
            address = address - (ref.address + ref.size);
        }

        // Patch the bytecode
        if (ref.address + ref.size > bytecode.size()) {
            add_error("Forward reference out of bounds");
            continue;
        }

        for (size_t i = 0; i < ref.size; ++i) {
            bytecode[ref.address + i] = (address >> (8 * i)) & 0xFF;
        }
    }
}

void Assembler::AssemblerEngine::add_error(const std::string& message) {
    errors.push_back("Assembly error: " + message);
    Logging::ErrorHandler::instance().report_parse(Logging::ErrorCode::ASM_GENERIC, message);
}

void Assembler::AssemblerEngine::add_error(const std::string& message, size_t line, size_t column) {
    errors.push_back("Line " + std::to_string(line) + ", Column " + std::to_string(column) + ": " + message);
    Logging::ErrorHandler::instance().report_parse(Logging::ErrorCode::ASM_DIRECTIVE_ERROR, message, "", line, column);
}

void Assembler::AssemblerEngine::handle_section_directive(const std::vector<std::unique_ptr<Assembler::Expression>>& args) {
    if (args.empty()) {
        add_error("section directive requires a section name");
        return;
    }
    
    // Get section name
    std::string section_name;
    if (auto id = dynamic_cast<const IdentifierExpression*>(args[0].get())) {
        section_name = id->name;
    } else if (auto str = dynamic_cast<const StringLiteralExpression*>(args[0].get())) {
        section_name = str->value;
    } else {
        add_error("section directive requires an identifier or string");
        return;
    }
    
    current_section = section_name;
}

void Assembler::AssemblerEngine::handle_global_directive(const std::vector<std::unique_ptr<Assembler::Expression>>& args) {
    for (const auto& arg : args) {
        std::string symbol_name;
        if (auto id = dynamic_cast<const IdentifierExpression*>(arg.get())) {
            symbol_name = id->name;
        } else {
            add_error("global directive requires symbol identifiers");
            continue;
        }
        
        // Create or update symbol
        if (symbol_table.find(symbol_name) == symbol_table.end()) {
            symbol_table[symbol_name] = Symbol(symbol_name, 0, false);
        }
        symbol_table[symbol_name].is_global = true;
    }
}

void Assembler::AssemblerEngine::handle_extern_directive(const std::vector<std::unique_ptr<Assembler::Expression>>& args) {
    for (const auto& arg : args) {
        std::string symbol_name;
        if (auto id = dynamic_cast<const IdentifierExpression*>(arg.get())) {
            symbol_name = id->name;
        } else {
            add_error("extern directive requires symbol identifiers");
            continue;
        }
        
        // Create symbol marked as defined (externally) so we don't error on undefined reference
        if (symbol_table.find(symbol_name) == symbol_table.end()) {
            symbol_table[symbol_name] = Symbol(symbol_name, 0, true); // Mark defined to pass checks
            symbol_table[symbol_name].is_global = true;
        }
    }
}

void Assembler::AssemblerEngine::handle_data_section() {
    current_section = ".data";
    has_explicit_sections = true;
    // Set current_address to data section base (0x100)
    if (current_address < data_section_base) {
        current_address = data_section_base;
    }
}

void Assembler::AssemblerEngine::handle_text_section() {
    current_section = ".text";
    has_explicit_sections = true;
    
    // Calculate .text section base dynamically
    // It starts after .data section, aligned to section boundary
    if (data_section_size > 0) {
        // .text starts after .data, with alignment
        uint32_t data_end = data_section_base + data_section_size;
        // Align to section_alignment boundary (e.g., 0x100 = 256 bytes)
        text_section_base = ((data_end + section_alignment - 1) / section_alignment) * section_alignment;
    } else {
        // No .data section yet, use default minimum base
        // Start at least at 0x100 to leave room for potential .data
        text_section_base = data_section_base;
    }
    
    // Set current_address to text section base
    if (current_address < text_section_base) {
        current_address = text_section_base;
    }
}

Architecture Assembler::AssemblerEngine::detect_architecture(const Program& program) {
    // Scan program for architecture indicators
    for (const auto& stmt : program.statements) {
        if (stmt->type == ASTNodeType::DIRECTIVE) {
            const auto& directive = static_cast<const Directive&>(*stmt);
            if (directive.name == ".bits") {
                if (!directive.arguments.empty()) {
                    if (auto imm = dynamic_cast<const ImmediateExpression*>(directive.arguments[0].get())) {
                        if (imm->value == 64) return Architecture::X64;
                        if (imm->value == 32) return Architecture::X86;
                    }
                }
            }
        } else if (stmt->type == ASTNodeType::INSTRUCTION) {
            const auto& instruction = static_cast<const Instruction&>(*stmt);
            
            // Check for 64-bit specific instructions
            if (instruction.mnemonic == "MODE64") return Architecture::X64;
            
            // Check for 64-bit instruction suffixes
            if (instruction.mnemonic.length() > 2) {
                std::string suffix2 = instruction.mnemonic.substr(instruction.mnemonic.length() - 2);
                if (suffix2 == "64" || suffix2 == "EX") return Architecture::X64;
            }
            
            // Check operands for 64-bit registers
            for (const auto& operand : instruction.operands) {
                if (auto reg = dynamic_cast<const RegisterExpression*>(operand.get())) {
                    std::string name = reg->name;
                    // Check for R8-R15
                    if (name.length() >= 2 && name[0] == 'R') {
                        if (isdigit(name[1])) {
                            int num = std::stoi(name.substr(1));
                            if (num >= 8 && num <= 15) return Architecture::X64;
                        }
                    }
                    // Check for standard 64-bit registers
                    if (name == "RAX" || name == "RBX" || name == "RCX" || name == "RDX" ||
                        name == "RSI" || name == "RDI" || name == "RBP" || name == "RSP" ||
                        name == "RIP" || name == "RFLAGS") {
                        return Architecture::X64;
                    }
                }
            }
        }
    }
    
    // Default to X86 if no 64-bit features detected
    return Architecture::X86;
}

} // namespace Assembler
