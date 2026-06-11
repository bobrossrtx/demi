#include "lexer.hpp"
#include "../debug/error_handler.hpp"
#include <algorithm>

namespace Assembler {

// Static member initialization
std::unordered_map<std::string, TokenType> Lexer::keywords;
std::unordered_map<std::string, TokenType> Lexer::mnemonics;
std::unordered_map<std::string, TokenType> Lexer::registers;
bool Lexer::tables_initialized = false;

void Lexer::init_tables() {
    if (tables_initialized) return;
    
    // Clear existing data
    keywords.clear();
    mnemonics.clear();
    
    // Assembly directives
    keywords[".data"] = TokenType::DIRECTIVE;
    keywords[".text"] = TokenType::DIRECTIVE;
    keywords[".org"] = TokenType::DIRECTIVE;
    keywords[".equ"] = TokenType::DIRECTIVE;
    keywords[".include"] = TokenType::DIRECTIVE;
    // .db directive removed - use DB instruction instead
    keywords[".dw"] = TokenType::DIRECTIVE;
    keywords[".dd"] = TokenType::DIRECTIVE;
    keywords[".dq"] = TokenType::DIRECTIVE;
    keywords[".string"] = TokenType::DIRECTIVE;
    keywords[".asciz"] = TokenType::DIRECTIVE;
    keywords[".align"] = TokenType::DIRECTIVE;
    keywords[".bss"] = TokenType::DIRECTIVE;
    keywords[".resb"] = TokenType::DIRECTIVE;
    keywords[".zero"] = TokenType::DIRECTIVE;
    keywords[".comm"] = TokenType::DIRECTIVE;
    keywords[".type"] = TokenType::DIRECTIVE;
    keywords[".size"] = TokenType::DIRECTIVE;
    // GNU-style aliases
    keywords[".byte"] = TokenType::DIRECTIVE;
    keywords[".word"] = TokenType::DIRECTIVE;
    keywords[".long"] = TokenType::DIRECTIVE;
    keywords[".quad"] = TokenType::DIRECTIVE;
    keywords["RESB"] = TokenType::DIRECTIVE;
    keywords[".end"] = TokenType::DIRECTIVE;
    keywords[".memory"] = TokenType::DIRECTIVE;
    
    // Structure keywords
    keywords["section"] = TokenType::SECTION;
    keywords[".section"] = TokenType::SECTION;
    keywords["global"] = TokenType::GLOBAL;
    keywords[".global"] = TokenType::GLOBAL;
    keywords[".function"] = TokenType::GLOBAL;  // reuse GLOBAL token type
    keywords["extern"] = TokenType::EXTERN;
    keywords[".extern"] = TokenType::EXTERN;
    
    // Test directives (using dot syntax for consistency)
    keywords[".test"] = TokenType::TEST_DIRECTIVE;
    keywords[".assert_mem"] = TokenType::ASSERT_MEM;
    keywords[".assert_reg"] = TokenType::ASSERT_REG;
    keywords[".assert_output"] = TokenType::ASSERT_OUTPUT;
    keywords[".expect_error"] = TokenType::EXPECT_ERROR;
    keywords[".entry_point"] = TokenType::ENTRY_POINT;
    keywords[".description"] = TokenType::DESCRIPTION;
    keywords[".author"] = TokenType::AUTHOR;
    keywords[".category"] = TokenType::CATEGORY;
    keywords[".tag"] = TokenType::TAG;
    keywords[".maxsteps"] = TokenType::MAXSTEPS;
    keywords[".maxcalldepth"] = TokenType::MAXCALLDEPTH;
    keywords[".timeout"] = TokenType::TIMEOUT;
    keywords[".skip"] = TokenType::SKIP;
    
    // Benchmark directives
    keywords[".benchmark"] = TokenType::BENCHMARK;
    keywords[".warmup"] = TokenType::WARMUP;
    keywords[".iterations"] = TokenType::ITERATIONS;
    keywords[".measure"] = TokenType::MEASURE;
    
    // Preprocessor directives
    keywords[".define"] = TokenType::DEFINE;
    keywords[".ifdef"] = TokenType::IFDEF;
    keywords[".ifndef"] = TokenType::IFNDEF;
    keywords[".elif"] = TokenType::ELIF;
    keywords[".else"] = TokenType::ELSE;
    keywords[".endif"] = TokenType::ENDIF;
    keywords[".undef"] = TokenType::UNDEF;
    
    // Debug directives
    keywords[".print"] = TokenType::PRINT;
    keywords[".break"] = TokenType::BREAK;
    keywords[".dump"] = TokenType::DUMP;
    keywords[".memdump"] = TokenType::MEMDUMP;
    keywords[".trace"] = TokenType::TRACE;
    keywords[".assert"] = TokenType::ASSERT;
    keywords[".dumpstack"] = TokenType::DUMPSTACK;
    keywords[".watch"] = TokenType::WATCH;
    keywords[".unwatch"] = TokenType::UNWATCH;
    keywords[".checkpoint"] = TokenType::CHECKPOINT;
    keywords[".log"] = TokenType::LOG;
    keywords[".dumpreg"] = TokenType::DUMPREG;
    keywords[".memset"] = TokenType::MEMSET;
    keywords[".step"] = TokenType::STEP;
    
    // DemiEngine instruction mnemonics (matching the CPU opcodes)
    mnemonics["NOP"] = TokenType::MNEMONIC;
    mnemonics["LOAD_IMM"] = TokenType::MNEMONIC;
    mnemonics["ADD"] = TokenType::MNEMONIC;
    mnemonics["SUB"] = TokenType::MNEMONIC;
    mnemonics["MOV"] = TokenType::MNEMONIC;
    mnemonics["JMP"] = TokenType::MNEMONIC;
    mnemonics["LOAD"] = TokenType::MNEMONIC;
    mnemonics["LOADR"] = TokenType::MNEMONIC;
    mnemonics["STORER"] = TokenType::MNEMONIC;
    mnemonics["STORE"] = TokenType::MNEMONIC;
    mnemonics["PUSH"] = TokenType::MNEMONIC;
    mnemonics["POP"] = TokenType::MNEMONIC;
    mnemonics["CMP"] = TokenType::MNEMONIC;
    mnemonics["JZ"] = TokenType::MNEMONIC;
    mnemonics["JE"] = TokenType::MNEMONIC;   // Alias for JZ (Jump if Equal)
    mnemonics["JNZ"] = TokenType::MNEMONIC;
    mnemonics["JNE"] = TokenType::MNEMONIC;  // Alias for JNZ (Jump if Not Equal)
    mnemonics["JS"] = TokenType::MNEMONIC;
    mnemonics["JNS"] = TokenType::MNEMONIC;
    mnemonics["JC"] = TokenType::MNEMONIC;
    mnemonics["JNC"] = TokenType::MNEMONIC;
    mnemonics["JO"] = TokenType::MNEMONIC;
    mnemonics["JNO"] = TokenType::MNEMONIC;
    mnemonics["JG"] = TokenType::MNEMONIC;
    mnemonics["JL"] = TokenType::MNEMONIC;
    mnemonics["JGE"] = TokenType::MNEMONIC;
    mnemonics["JLE"] = TokenType::MNEMONIC;
    mnemonics["MUL"] = TokenType::MNEMONIC;
    mnemonics["IMUL"] = TokenType::MNEMONIC;
    mnemonics["DIV"] = TokenType::MNEMONIC;
    mnemonics["MOD"] = TokenType::MNEMONIC;
    mnemonics["ADC"] = TokenType::MNEMONIC;
    mnemonics["SBB"] = TokenType::MNEMONIC;
    mnemonics["MOVSX"] = TokenType::MNEMONIC;
    mnemonics["MOVZX"] = TokenType::MNEMONIC;
    mnemonics["SETZ"] = TokenType::MNEMONIC;
    mnemonics["SETNZ"] = TokenType::MNEMONIC;
    mnemonics["SETG"] = TokenType::MNEMONIC;
    mnemonics["SETGE"] = TokenType::MNEMONIC;
    mnemonics["SETL"] = TokenType::MNEMONIC;
    mnemonics["SETLE"] = TokenType::MNEMONIC;
    mnemonics["SETC"] = TokenType::MNEMONIC;
    mnemonics["SETNC"] = TokenType::MNEMONIC;
    mnemonics["SETO"] = TokenType::MNEMONIC;
    mnemonics["SETNO"] = TokenType::MNEMONIC;
    mnemonics["SETS"] = TokenType::MNEMONIC;
    mnemonics["SETNS"] = TokenType::MNEMONIC;
    mnemonics["SAL"] = TokenType::MNEMONIC;
    mnemonics["SAR"] = TokenType::MNEMONIC;
    mnemonics["LOOP"] = TokenType::MNEMONIC;
    mnemonics["LOOPE"] = TokenType::MNEMONIC;
    mnemonics["LOOPNE"] = TokenType::MNEMONIC;
    // Rotates
    mnemonics["ROL"] = TokenType::MNEMONIC;
    mnemonics["ROR"] = TokenType::MNEMONIC;
    mnemonics["RCL"] = TokenType::MNEMONIC;
    mnemonics["RCR"] = TokenType::MNEMONIC;
    // sign extension
    mnemonics["CBW"] = TokenType::MNEMONIC;
    mnemonics["CWD"] = TokenType::MNEMONIC;
    mnemonics["CWDE"] = TokenType::MNEMONIC;
    mnemonics["CDQ"] = TokenType::MNEMONIC;
    // exchange
    mnemonics["XCHG"] = TokenType::MNEMONIC;
    // flag ops
    mnemonics["CLC"] = TokenType::MNEMONIC;
    mnemonics["STC"] = TokenType::MNEMONIC;
    mnemonics["CMC"] = TokenType::MNEMONIC;
    mnemonics["CLD"] = TokenType::MNEMONIC;
    mnemonics["STD"] = TokenType::MNEMONIC;
    // signed divide
    mnemonics["IDIV"] = TokenType::MNEMONIC;
    // byte swap
    mnemonics["BSWAP"] = TokenType::MNEMONIC;
    // flag load/store
    mnemonics["LAHF"] = TokenType::MNEMONIC;
    mnemonics["SAHF"] = TokenType::MNEMONIC;
    // enter/leave
    mnemonics["ENTER"] = TokenType::MNEMONIC;
    // conditional moves
    mnemonics["CMOVZ"] = TokenType::MNEMONIC;
    mnemonics["CMOVNZ"] = TokenType::MNEMONIC;
    mnemonics["CMOVC"] = TokenType::MNEMONIC;
    mnemonics["CMOVNC"] = TokenType::MNEMONIC;
    mnemonics["CMOVO"] = TokenType::MNEMONIC;
    mnemonics["CMOVNO"] = TokenType::MNEMONIC;
    mnemonics["CMOVS"] = TokenType::MNEMONIC;
    mnemonics["CMOVNS"] = TokenType::MNEMONIC;
    mnemonics["CMOVG"] = TokenType::MNEMONIC;
    mnemonics["CMOVGE"] = TokenType::MNEMONIC;
    mnemonics["CMOVL"] = TokenType::MNEMONIC;
    mnemonics["CMOVLE"] = TokenType::MNEMONIC;
    mnemonics["CMOVA"] = TokenType::MNEMONIC;
    mnemonics["CMOVAE"] = TokenType::MNEMONIC;
    mnemonics["CMOVB"] = TokenType::MNEMONIC;
    mnemonics["CMOVBE"] = TokenType::MNEMONIC;
    // atomic / bit
    mnemonics["CMPXCHG"] = TokenType::MNEMONIC;
    mnemonics["XADD"] = TokenType::MNEMONIC;
    mnemonics["BT"] = TokenType::MNEMONIC;
    mnemonics["BTS"] = TokenType::MNEMONIC;
    mnemonics["BTR"] = TokenType::MNEMONIC;
    mnemonics["BTC"] = TokenType::MNEMONIC;
    // string ops + REP
    mnemonics["MOVSB"] = TokenType::MNEMONIC;
    mnemonics["MOVSW"] = TokenType::MNEMONIC;
    mnemonics["MOVSD"] = TokenType::MNEMONIC;
    mnemonics["STOSB"] = TokenType::MNEMONIC;
    mnemonics["STOSW"] = TokenType::MNEMONIC;
    mnemonics["STOSD"] = TokenType::MNEMONIC;
    mnemonics["LODSB"] = TokenType::MNEMONIC;
    mnemonics["LODSW"] = TokenType::MNEMONIC;
    mnemonics["LODSD"] = TokenType::MNEMONIC;
    mnemonics["REP"] = TokenType::MNEMONIC;
    // processor info
    mnemonics["CPUID"] = TokenType::MNEMONIC;
    mnemonics["RDTSC"] = TokenType::MNEMONIC;
    // fast syscall
    mnemonics["SYSCALL"] = TokenType::MNEMONIC;
    mnemonics["SYSENTER"] = TokenType::MNEMONIC;
    mnemonics["INC"] = TokenType::MNEMONIC;
    mnemonics["DEC"] = TokenType::MNEMONIC;
    mnemonics["AND"] = TokenType::MNEMONIC;
    mnemonics["OR"] = TokenType::MNEMONIC;
    mnemonics["XOR"] = TokenType::MNEMONIC;
    mnemonics["NOT"] = TokenType::MNEMONIC;
    mnemonics["NEG"] = TokenType::MNEMONIC;
    mnemonics["TEST"] = TokenType::MNEMONIC;
    mnemonics["SHL"] = TokenType::MNEMONIC;
    mnemonics["SHR"] = TokenType::MNEMONIC;
    mnemonics["CALL"] = TokenType::MNEMONIC;
    mnemonics["RET"] = TokenType::MNEMONIC;
    mnemonics["PUSH_ARG"] = TokenType::MNEMONIC;
    mnemonics["POP_ARG"] = TokenType::MNEMONIC;
    mnemonics["PUSH_FLAG"] = TokenType::MNEMONIC;
    mnemonics["POP_FLAG"] = TokenType::MNEMONIC;
    mnemonics["LEA"] = TokenType::MNEMONIC;
    mnemonics["SWAP"] = TokenType::MNEMONIC;
    mnemonics["IN"] = TokenType::MNEMONIC;
    mnemonics["OUT"] = TokenType::MNEMONIC;
    mnemonics["INB"] = TokenType::MNEMONIC;
    mnemonics["OUTB"] = TokenType::MNEMONIC;
    mnemonics["INW"] = TokenType::MNEMONIC;
    mnemonics["OUTW"] = TokenType::MNEMONIC;
    mnemonics["INL"] = TokenType::MNEMONIC;
    mnemonics["OUTL"] = TokenType::MNEMONIC;
    mnemonics["INSTR"] = TokenType::MNEMONIC;
    mnemonics["OUTSTR"] = TokenType::MNEMONIC;
    mnemonics["DB"] = TokenType::MNEMONIC;
    mnemonics["RESB"] = TokenType::MNEMONIC;
    mnemonics["EQU"] = TokenType::MNEMONIC;
    
    // Interrupt operations
    mnemonics["INT"] = TokenType::MNEMONIC;
    mnemonics["IRET"] = TokenType::MNEMONIC;
    mnemonics["CLI"] = TokenType::MNEMONIC;
    mnemonics["STI"] = TokenType::MNEMONIC;
    
    mnemonics["HALT"] = TokenType::MNEMONIC;
    
    // Extended 64-bit operations
    mnemonics["ADD64"] = TokenType::MNEMONIC;
    mnemonics["SUB64"] = TokenType::MNEMONIC;
    mnemonics["MOV64"] = TokenType::MNEMONIC;
    mnemonics["LOAD_IMM64"] = TokenType::MNEMONIC;
    mnemonics["MUL64"] = TokenType::MNEMONIC;
    mnemonics["DIV64"] = TokenType::MNEMONIC;
    mnemonics["AND64"] = TokenType::MNEMONIC;
    mnemonics["OR64"] = TokenType::MNEMONIC;
    mnemonics["XOR64"] = TokenType::MNEMONIC;
    mnemonics["NOT64"] = TokenType::MNEMONIC;
    mnemonics["SHL64"] = TokenType::MNEMONIC;
    mnemonics["SHR64"] = TokenType::MNEMONIC;
    mnemonics["CMP64"] = TokenType::MNEMONIC;
    mnemonics["MOD64"] = TokenType::MNEMONIC;
    mnemonics["INC64"] = TokenType::MNEMONIC;
    mnemonics["DEC64"] = TokenType::MNEMONIC;
    
    // Extended register operations
    mnemonics["MOVEX"] = TokenType::MNEMONIC;
    mnemonics["ADDEX"] = TokenType::MNEMONIC;
    mnemonics["SUBEX"] = TokenType::MNEMONIC;
    mnemonics["MULEX"] = TokenType::MNEMONIC;
    mnemonics["DIVEX"] = TokenType::MNEMONIC;
    mnemonics["CMPEX"] = TokenType::MNEMONIC;
    mnemonics["LOADEX"] = TokenType::MNEMONIC;
    mnemonics["STOREX"] = TokenType::MNEMONIC;
    mnemonics["PUSHEX"] = TokenType::MNEMONIC;
    mnemonics["POPEX"] = TokenType::MNEMONIC;
    
    // Mode control
    mnemonics["MODE32"] = TokenType::MNEMONIC;
    mnemonics["MODE64"] = TokenType::MNEMONIC;
    mnemonics["MODECMP"] = TokenType::MNEMONIC;
    
    // FPU Operations
    mnemonics["FLD"] = TokenType::MNEMONIC;
    mnemonics["FST"] = TokenType::MNEMONIC;
    mnemonics["FSTP"] = TokenType::MNEMONIC;
    mnemonics["FILD"] = TokenType::MNEMONIC;
    mnemonics["FIST"] = TokenType::MNEMONIC;
    mnemonics["FISTP"] = TokenType::MNEMONIC;
    mnemonics["FADD"] = TokenType::MNEMONIC;
    mnemonics["FSUB"] = TokenType::MNEMONIC;
    mnemonics["FMUL"] = TokenType::MNEMONIC;
    mnemonics["FDIV"] = TokenType::MNEMONIC;
    mnemonics["FSIN"] = TokenType::MNEMONIC;
    mnemonics["FCOS"] = TokenType::MNEMONIC;
    mnemonics["FTAN"] = TokenType::MNEMONIC;
    mnemonics["FSQRT"] = TokenType::MNEMONIC;
    mnemonics["FABS"] = TokenType::MNEMONIC;
    mnemonics["FCHS"] = TokenType::MNEMONIC;
    mnemonics["FINIT"] = TokenType::MNEMONIC;
    mnemonics["FLDZ"] = TokenType::MNEMONIC;
    mnemonics["FLD1"] = TokenType::MNEMONIC;
    mnemonics["FLDPI"] = TokenType::MNEMONIC;
    mnemonics["FSTCW"] = TokenType::MNEMONIC;
    mnemonics["FLDCW"] = TokenType::MNEMONIC;
    mnemonics["FSTSW"] = TokenType::MNEMONIC;
    mnemonics["FCOMPP"] = TokenType::MNEMONIC;
    mnemonics["FUCOMPP"] = TokenType::MNEMONIC;
    mnemonics["FCLEX"] = TokenType::MNEMONIC;
    
    // SIMD/Vector instructions
    mnemonics["VADD"] = TokenType::MNEMONIC;
    mnemonics["VMUL"] = TokenType::MNEMONIC;
    mnemonics["VDOT"] = TokenType::MNEMONIC;
    mnemonics["VMAX"] = TokenType::MNEMONIC;
    mnemonics["VBROADCAST"] = TokenType::MNEMONIC;
    mnemonics["VCMPGT"] = TokenType::MNEMONIC;
    mnemonics["PACKB"] = TokenType::MNEMONIC;
    mnemonics["UNPACKB"] = TokenType::MNEMONIC;

    // SSE Packed Single (float) Operations (0x80-0x8C)
    mnemonics["MOVAPS"] = TokenType::MNEMONIC;
    mnemonics["MOVUPS"] = TokenType::MNEMONIC;
    mnemonics["ADDPS"] = TokenType::MNEMONIC;
    mnemonics["SUBPS"] = TokenType::MNEMONIC;
    mnemonics["MULPS"] = TokenType::MNEMONIC;
    mnemonics["DIVPS"] = TokenType::MNEMONIC;
    mnemonics["SQRTPS"] = TokenType::MNEMONIC;
    mnemonics["MAXPS"] = TokenType::MNEMONIC;
    mnemonics["MINPS"] = TokenType::MNEMONIC;
    mnemonics["ANDPS"] = TokenType::MNEMONIC;
    mnemonics["ORPS"] = TokenType::MNEMONIC;
    mnemonics["XORPS"] = TokenType::MNEMONIC;
    mnemonics["CMPPS"] = TokenType::MNEMONIC;
    mnemonics["MOVPS"] = TokenType::MNEMONIC; // Alias for MOVAPS

    // SSE Packed Double Operations (0x8D-0x99)
    mnemonics["MOVAPD"] = TokenType::MNEMONIC;
    mnemonics["MOVUPD"] = TokenType::MNEMONIC;
    mnemonics["ADDPD"] = TokenType::MNEMONIC;
    mnemonics["SUBPD"] = TokenType::MNEMONIC;
    mnemonics["MULPD"] = TokenType::MNEMONIC;
    mnemonics["DIVPD"] = TokenType::MNEMONIC;
    mnemonics["SQRTPD"] = TokenType::MNEMONIC;
    mnemonics["MAXPD"] = TokenType::MNEMONIC;
    mnemonics["MINPD"] = TokenType::MNEMONIC;
    mnemonics["ANDPD"] = TokenType::MNEMONIC;
    mnemonics["ORPD"] = TokenType::MNEMONIC;
    mnemonics["XORPD"] = TokenType::MNEMONIC;
    mnemonics["CMPPD"] = TokenType::MNEMONIC;

    // Legacy 8-register names (R0-R7)
    for (int i = 0; i < 8; ++i) {
        registers["R" + std::to_string(i)] = TokenType::REGISTER;
    }
    
    // x64-style register names (based on the extended register system)
    registers["RAX"] = TokenType::REGISTER; // R0
    registers["RCX"] = TokenType::REGISTER; // R1
    registers["RDX"] = TokenType::REGISTER; // R2
    registers["RBX"] = TokenType::REGISTER; // R3
    registers["RSP"] = TokenType::REGISTER; // R4 (Stack Pointer)
    registers["RBP"] = TokenType::REGISTER; // R5 (Base Pointer)
    registers["RSI"] = TokenType::REGISTER; // R6
    registers["RDI"] = TokenType::REGISTER; // R7
    
    // Extended registers R8-R15
    for (int i = 8; i < 16; ++i) {
        registers["R" + std::to_string(i)] = TokenType::REGISTER;
        registers["R" + std::to_string(i) + "D"] = TokenType::REGISTER;
    }
    
    // Special registers
    registers["RIP"] = TokenType::REGISTER; // Instruction pointer (PC)
    registers["RFLAGS"] = TokenType::REGISTER; // Flags register
    
    // 32-bit register aliases
    registers["EAX"] = TokenType::REGISTER;
    registers["EBX"] = TokenType::REGISTER;
    registers["ECX"] = TokenType::REGISTER;
    registers["EDX"] = TokenType::REGISTER;
    registers["ESI"] = TokenType::REGISTER;
    registers["EDI"] = TokenType::REGISTER;
    registers["ESP"] = TokenType::REGISTER;
    registers["EBP"] = TokenType::REGISTER;

    // 16-bit register aliases
    registers["AX"] = TokenType::REGISTER;
    registers["BX"] = TokenType::REGISTER;
    registers["CX"] = TokenType::REGISTER;
    registers["DX"] = TokenType::REGISTER;

    // 8-bit register aliases
    registers["AL"] = TokenType::REGISTER;
    registers["BL"] = TokenType::REGISTER;
    registers["CL"] = TokenType::REGISTER;
    registers["DL"] = TokenType::REGISTER;

    // XMM registers (128-bit SIMD)
    for (int i = 0; i < 16; ++i) {
        registers["XMM" + std::to_string(i)] = TokenType::REGISTER;
        registers["XMM" + std::to_string(i) + "_HIGH"] = TokenType::REGISTER;
    }

    // YMM registers (256-bit SIMD, aliases for XMM lower parts)
    for (int i = 0; i < 16; ++i) {
        registers["YMM" + std::to_string(i)] = TokenType::REGISTER;
    }

    tables_initialized = true;
}

Lexer::Lexer(const std::string& source) 
    : source(source), pos(0), line(1), column(1) {
    init_tables();
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    
    while (pos < source.length()) {
        skip_whitespace();
        
        if (pos >= source.length()) break;
        
        char c = current_char();
        
        // Handle comments starting with # or ;
        if (c == '#' || c == ';') {
            skip_comment();
            continue;
        }
        
        // Handle newlines
        if (c == '\n') {
            tokens.emplace_back(TokenType::NEWLINE, "\\n", line, column);
            advance();
            line++;
            column = 1;
            continue;
        }
        
        // Handle directives
        if (c == '.') {
            tokens.push_back(parse_directive());
            continue;
        }
        
        // Handle strings
        if (c == '"' || c == '\'') {
            tokens.push_back(parse_string());
            continue;
        }
        
        // Handle numbers
        if (is_digit(c) || (c == '0' && pos + 1 < source.length() && 
                           (peek_char() == 'x' || peek_char() == 'b'))) {
            tokens.push_back(parse_number());
            continue;
        }
        
        // Handle identifiers and mnemonics
        if (is_identifier_start(c)) {
            tokens.push_back(parse_identifier());
            continue;
        }
        
        // Handle single-character tokens
        switch (c) {
            case ',': tokens.emplace_back(TokenType::COMMA, ",", line, column); break;
            case ':': tokens.emplace_back(TokenType::COLON, ":", line, column); break;
            case '[': tokens.emplace_back(TokenType::LBRACKET, "[", line, column); break;
            case ']': tokens.emplace_back(TokenType::RBRACKET, "]", line, column); break;
            case '+': tokens.emplace_back(TokenType::PLUS, "+", line, column); break;
            case '-': tokens.emplace_back(TokenType::MINUS, "-", line, column); break;
            case '*': tokens.emplace_back(TokenType::ASTERISK, "*", line, column); break;
            case '(': tokens.emplace_back(TokenType::LPAREN, "(", line, column); break;
            case ')': tokens.emplace_back(TokenType::RPAREN, ")", line, column); break;
            case '{': tokens.emplace_back(TokenType::LBRACE, "{", line, column); break;
            case '}': tokens.emplace_back(TokenType::RBRACE, "}", line, column); break;
            default:
                add_error("Unexpected character: '" + std::string(1, c) + "'");
                tokens.emplace_back(TokenType::INVALID, std::string(1, c), line, column);
                break;
        }
        advance();
    }
    
    tokens.emplace_back(TokenType::END_OF_FILE, "", line, column);
    return tokens;
}

char Lexer::current_char() const {
    if (pos >= source.length()) return '\0';
    return source[pos];
}

char Lexer::peek_char(size_t offset) const {
    size_t peek_pos = pos + offset;
    if (peek_pos >= source.length()) return '\0';
    return source[peek_pos];
}

void Lexer::advance() {
    if (pos < source.length()) {
        pos++;
        column++;
    }
}

void Lexer::skip_whitespace() {
    while (pos < source.length() && std::isspace(current_char()) && current_char() != '\n') {
        advance();
    }
}

void Lexer::skip_comment() {
    // Skip to end of line
    while (pos < source.length() && current_char() != '\n') {
        advance();
    }
}

Token Lexer::parse_identifier() {
    size_t start_line = line;
    size_t start_column = column;
    std::string text;
    
    while (pos < source.length() && is_identifier_part(current_char())) {
        text += current_char();
        advance();
    }
    
    // Convert to uppercase for case-insensitive matching
    std::string upper_text = text;
    std::transform(upper_text.begin(), upper_text.end(), upper_text.begin(), ::toupper);
    
    // Check if it's a keyword (section, global, extern)
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);

    if (lower_text == "byte" || lower_text == "word" || lower_text == "dword" || lower_text == "qword") {
        return Token(TokenType::SIZE_SPECIFIER, lower_text, start_line, start_column);
    }

    auto keyword_it = keywords.find(lower_text);
    if (keyword_it != keywords.end()) {
        return Token(keyword_it->second, lower_text, start_line, start_column);
    }
    
    // Check if it's a mnemonic
    if (mnemonics.find(upper_text) != mnemonics.end()) {
        return Token(TokenType::MNEMONIC, upper_text, start_line, start_column);
    }
    
    // Check if it's a register
    if (registers.find(upper_text) != registers.end()) {
        return Token(TokenType::REGISTER, upper_text, start_line, start_column);
    }
    
    // Otherwise it's an identifier (label, symbol, etc.)
    return Token(TokenType::IDENTIFIER, text, start_line, start_column);
}

Token Lexer::parse_number() {
    size_t start_line = line;
    size_t start_column = column;
    std::string text;
    uint64_t value = 0;
    int base = 10;
    
    // Check for hex (0x), binary (0b), or octal (0o) prefix
    if (current_char() == '0' && pos + 1 < source.length()) {
        char next = peek_char();
        if (next == 'x' || next == 'X') {
            base = 16;
            text += current_char(); advance(); // '0'
            text += current_char(); advance(); // 'x'
        } else if (next == 'b' || next == 'B') {
            base = 2;
            text += current_char(); advance(); // '0'
            text += current_char(); advance(); // 'b'
        } else if (next == 'o' || next == 'O') {
            base = 8;
            text += current_char(); advance(); // '0'
            text += current_char(); advance(); // 'o'
        }
    }
    
    // Parse digits
    while (pos < source.length()) {
        char c = current_char();
        bool valid_digit = false;
        
        if (base == 16 && is_hex_digit(c)) {
            valid_digit = true;
        } else if (base == 2 && is_binary_digit(c)) {
            valid_digit = true;
        } else if (base == 8 && c >= '0' && c <= '7') {
            valid_digit = true;
        } else if (base == 10 && is_digit(c)) {
            valid_digit = true;
        }
        
        if (!valid_digit) break;
        
        text += c;
        
        // Calculate value
        if (base == 16) {
            value = value * 16 + (is_digit(c) ? (c - '0') : (std::toupper(c) - 'A' + 10));
        } else if (base == 2) {
            value = value * 2 + (c - '0');
        } else if (base == 8) {
            value = value * 8 + (c - '0');
        } else {
            value = value * 10 + (c - '0');
        }
        
        advance();
    }
    
    Token token(TokenType::NUMBER, text, start_line, start_column);
    token.value = value;
    return token;
}

Token Lexer::parse_string() {
    size_t start_line = line;
    size_t start_column = column;
    char quote_char = current_char();
    std::string text;
    std::string value;
    
    text += quote_char;
    advance(); // Skip opening quote
    
    while (pos < source.length() && current_char() != quote_char) {
        char c = current_char();
        
        if (c == '\\' && pos + 1 < source.length()) {
            // Handle escape sequences
            advance(); // Skip backslash
            char escaped = current_char();
            switch (escaped) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                case '\'': value += '\''; break;
                case '0': value += '\0'; break;
                default: 
                    value += escaped; 
                    break;
            }
            text += '\\';
            text += escaped;
        } else if (c == '\n') {
            add_error("Unterminated string literal");
            break;
        } else {
            value += c;
            text += c;
        }
        advance();
    }
    
    if (pos < source.length() && current_char() == quote_char) {
        text += quote_char;
        advance(); // Skip closing quote
    } else {
        add_error("Unterminated string literal");
    }
    
    Token token(TokenType::STRING, text, start_line, start_column);
    token.value = value;
    return token;
}

Token Lexer::parse_directive() {
    size_t start_line = line;
    size_t start_column = column;
    std::string text;
    
    text += current_char(); // '.'
    advance();
    
    while (pos < source.length() && is_identifier_part(current_char())) {
        text += current_char();
        advance();
    }
    
    std::string upper_text = text;
    std::transform(upper_text.begin(), upper_text.end(), upper_text.begin(), ::tolower);
    
    auto it = keywords.find(upper_text);
    if (it != keywords.end()) {
        return Token(it->second, upper_text, start_line, start_column);
    }
    
    add_error("Unknown directive: " + text);
    return Token(TokenType::INVALID, text, start_line, start_column);
}

bool Lexer::is_identifier_start(char c) const {
    return std::isalpha(c) || c == '_';
}

bool Lexer::is_identifier_part(char c) const {
    return std::isalnum(c) || c == '_';
}

bool Lexer::is_digit(char c) const {
    return c >= '0' && c <= '9';
}

bool Lexer::is_hex_digit(char c) const {
    return is_digit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

bool Lexer::is_binary_digit(char c) const {
    return c == '0' || c == '1';
}

void Lexer::add_error(const std::string& message) {
    errors.push_back("Line " + std::to_string(line) + ", Column " + std::to_string(column) + ": " + message);
    Logging::ErrorHandler::instance().report_parse(Logging::ErrorCode::PARSE_GENERIC, message, "", line, column);
}

} // namespace Assembler
