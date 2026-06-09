#pragma once
#include <cstdint>

namespace Assembler {

// DemiEngine instruction opcodes (standalone version for assembler)
enum class Opcode : uint8_t {
    NOP = 0x00,         // No operation
    LOAD_IMM = 0x01,    // Load immediate value into reg
    ADD = 0x02,         // Add reg1, reg2
    SUB = 0x03,         // Subtract reg1, reg2
    MOV = 0x04,         // Move reg1, reg2 (reg1 = reg2)
    JMP = 0x05,         // Jump to address
    LOAD = 0x06,        // Load value from memory to reg (direct addressing)
    STORE = 0x07,       // Store value from reg to memory
    LOADR = 0x41,       // Load value from memory to reg (indirect addressing - addr in register)
    DEBUG = 0x42,       // Debug directive (print, break, dump, etc.)
    STORER = 0x43,      // Store value from reg to memory (indirect addressing - addr in register)

    PUSH = 0x08,        // Push reg onto stack
    POP = 0x09,         // Pop value from stack to reg
    CMP = 0x0A,         // Compare reg1, reg2

    JZ  = 0x0B,         // Jump if zero flag set
    JNZ = 0x0C,         // Jump if zero flag not set
    JS  = 0x0D,         // Jump if sign flag set
    JNS = 0x0E,         // Jump if sign flag not set
    JC  = 0x0F,         // Jump if carry flag set
    JNC = 0x22,         // Jump if carry flag not set
    JO  = 0x23,         // Jump if overflow flag set
    JNO = 0x24,         // Jump if overflow flag not set
    JG  = 0x25,         // Jump if greater (signed)
    JL  = 0x26,         // Jump if less (signed)
    JGE = 0x27,         // Jump if greater or equal (signed)
    JLE = 0x28,         // Jump if less or equal (signed)
    MOD = 0x29,         // Modulo (remainder) reg1, reg2

    MUL = 0x10,         // Multiply reg1, reg2
    DIV = 0x11,         // Divide reg1, reg2
    INC = 0x12,         // Increment reg
    DEC = 0x13,         // Decrement reg
    AND = 0x14,         // Bitwise AND reg1, reg2
    OR  = 0x15,         // Bitwise OR reg1, reg2
    XOR = 0x16,         // Bitwise XOR reg1, reg2
    NOT = 0x17,         // Bitwise NOT reg
    SHL = 0x18,         // Shift Left reg, imm
    SHR = 0x19,         // Shift Right reg, imm
    CALL = 0x1A,        // Call subroutine
    RET  = 0x1B,        // Return from subroutine
    PUSH_ARG = 0x1C,    // Push argument onto stack
    POP_ARG  = 0x1D,    // Pop argument from stack
    PUSH_FLAG = 0x1E,   // Push flags onto stack
    POP_FLAG  = 0x1F,   // Pop flags from stack

    LEA = 0x20,         // Load Effective Address - load address into register
    SWAP = 0x21,        // Swap - swap values between register and memory

    IN = 0x30,          // Input from port/device to register
    OUT = 0x31,         // Output from register to port/device
    INB = 0x32,         // Input byte from port/device to register
    OUTB = 0x33,        // Output byte from register to port/device
    INW = 0x34,         // Input word from port/device to register
    OUTW = 0x35,        // Output word from register to port/device
    INL = 0x36,         // Input long from port/device to register
    OUTL = 0x37,        // Output long from register to port/device
    INSTR = 0x38,       // Input instruction from port/device to register
    OUTSTR = 0x39,      // Output string from register to port/device

    DB = 0x40,          // Define byte

    // Extended 64-bit Register Operations (0x50-0x6F range)
    ADD64 = 0x50,       // 64-bit Add reg1, reg2
    SUB64 = 0x51,       // 64-bit Subtract reg1, reg2
    MOV64 = 0x52,       // 64-bit Move reg1, reg2 (reg1 = reg2)
    LOAD_IMM64 = 0x53,  // Load 64-bit immediate
    MUL64 = 0x54,       // 64-bit Multiply reg1, reg2
    DIV64 = 0x55,       // 64-bit Divide reg1, reg2
    AND64 = 0x56,       // 64-bit Bitwise AND reg1, reg2
    OR64 = 0x57,        // 64-bit Bitwise OR reg1, reg2
    XOR64 = 0x58,       // 64-bit Bitwise XOR reg1, reg2
    NOT64 = 0x59,       // 64-bit Bitwise NOT reg
    SHL64 = 0x5A,       // 64-bit Shift Left reg, imm
    SHR64 = 0x5B,       // 64-bit Shift Right reg, imm
    CMP64 = 0x5C,       // 64-bit Compare reg1, reg2
    INC64 = 0x5D,       // 64-bit Increment reg
    DEC64 = 0x5E,       // 64-bit Decrement reg
    MOD64 = 0x5F,       // 64-bit Modulo (remainder) reg1, reg2

    // Extended Register Set Operations (0x60-0x6F range)
    MOVEX = 0x60,       // Move between extended registers (R8-R15)
    ADDEX = 0x61,       // Add with extended registers
    SUBEX = 0x62,       // Subtract with extended registers
    MULEX = 0x63,       // Multiply with extended registers
    DIVEX = 0x64,       // Divide with extended registers
    CMPEX = 0x65,       // Compare with extended registers
    LOADEX = 0x66,      // Load from memory to extended register
    STOREX = 0x67,      // Store from extended register to memory
    PUSHEX = 0x68,      // Push extended register onto stack
    POPEX = 0x69,       // Pop from stack to extended register

    // Mode Control Operations (0x70-0x7F range)
    MODE32 = 0x70,      // Switch to 32-bit mode
    MODE64 = 0x71,      // Switch to 64-bit mode
    MODECMP = 0x72,     // Compare current CPU mode
    MODEFLAG = 0x73,    // Set mode flag in RFLAGS register

    // SIMD Operations (0x80-0x9F range)
    MOVAPS = 0x80,      // Move Aligned Packed Single
    MOVUPS = 0x81,      // Move Unaligned Packed Single
    ADDPS = 0x82,       // Add Packed Single
    SUBPS = 0x83,       // Subtract Packed Single
    MULPS = 0x84,       // Multiply Packed Single
    DIVPS = 0x85,       // Divide Packed Single
    SQRTPS = 0x86,      // Square Root Packed Single
    MAXPS = 0x87,       // Maximum Packed Single
    MINPS = 0x88,       // Minimum Packed Single
    ANDPS = 0x89,       // Bitwise AND Packed Single
    ORPS = 0x8A,        // Bitwise OR Packed Single
    XORPS = 0x8B,       // Bitwise XOR Packed Single
    CMPPS = 0x8C,       // Compare Packed Single

    // Packed Double Operations
    MOVAPD = 0x8D,      // Move Aligned Packed Double
    MOVUPD = 0x8E,      // Move Unaligned Packed Double
    ADDPD = 0x8F,       // Add Packed Double
    SUBPD = 0x90,       // Subtract Packed Double
    MULPD = 0x91,       // Multiply Packed Double
    DIVPD = 0x92,       // Divide Packed Double
    SQRTPD = 0x93,      // Square Root Packed Double
    MAXPD = 0x94,       // Maximum Packed Double
    MINPD = 0x95,       // Minimum Packed Double
    ANDPD = 0x96,       // Bitwise AND Packed Double
    ORPD = 0x97,        // Bitwise OR Packed Double
    XORPD = 0x98,       // Bitwise XOR Packed Double
    CMPPD = 0x99,       // Compare Packed Double

    // FPU Operations (0xA0-0xBF range)
    FLD = 0xA0,         // Load floating point value
    FST = 0xA1,         // Store floating point value
    FSTP = 0xA2,        // Store floating point value and pop
    FILD = 0xA3,        // Load integer as floating point
    FIST = 0xA4,        // Store floating point as integer
    FISTP = 0xA5,       // Store floating point as integer and pop
    FADD = 0xA6,        // Floating point add
    FSUB = 0xA7,        // Floating point subtract
    FMUL = 0xA8,        // Floating point multiply
    FDIV = 0xA9,        // Floating point divide
    FSIN = 0xAA,        // Floating point sine
    FCOS = 0xAB,        // Floating point cosine
    FTAN = 0xAC,        // Floating point tangent
    FSQRT = 0xAD,       // Floating point square root
    FABS = 0xAE,        // Floating point absolute value
    FCHS = 0xAF,        // Floating point change sign

    // FPU Control Operations
    FINIT = 0xB0,       // Initialize FPU
    FCLEX = 0xB1,       // Clear exceptions
    FSTCW = 0xB2,       // Store control word
    FLDCW = 0xB3,       // Load control word
    FSTSW = 0xB4,       // Store status word
    FCOMPP = 0xB5,      // Compare and pop twice
    FUCOMPP = 0xB6,     // Unordered compare and pop twice

    // AVX Operations (0xC0-0xDF range)
    VADDPS = 0xC0,      // AVX Add Packed Single
    VSUBPS = 0xC1,      // AVX Subtract Packed Single
    VMULPS = 0xC2,      // AVX Multiply Packed Single
    VDIVPS = 0xC3,      // AVX Divide Packed Single
    VSQRTPS = 0xC4,     // AVX Square Root Packed Single
    VMAXPS = 0xC5,      // AVX Maximum Packed Single
    VMINPS = 0xC6,      // AVX Minimum Packed Single
    VANDPS = 0xC7,      // AVX Bitwise AND Packed Single
    VORPS = 0xC8,       // AVX Bitwise OR Packed Single
    VXORPS = 0xC9,      // AVX Bitwise XOR Packed Single

    // AVX Packed Double Operations
    VADDPD = 0xCA,      // AVX Add Packed Double
    VSUBPD = 0xCB,      // AVX Subtract Packed Double
    VMULPD = 0xCC,      // AVX Multiply Packed Double
    VDIVPD = 0xCE,      // AVX Divide Packed Double
    VSQRTPD = 0xDD,     // AVX Square Root Packed Double
    VMAXPD = 0xDC,      // AVX Maximum Packed Double
    VMINPD = 0xD0,      // AVX Minimum Packed Double
    VANDPD = 0xD1,      // AVX Bitwise AND Packed Double
    VORPD = 0xD2,       // AVX Bitwise OR Packed Double
    VXORPD = 0xD3,      // AVX Bitwise XOR Packed Double

    // Custom SIMD Operations (0xD4-0xDB range)
    VADD = 0xD4,        // Vector add
    VMUL = 0xD5,        // Vector multiply
    VDOT = 0xD6,        // Vector dot product
    VMAX = 0xD7,        // Vector maximum
    VBROADCAST = 0xD8,  // Vector broadcast
    VCMPGT = 0xD9,      // Vector compare greater than
    PACKB = 0xDA,       // Pack bytes
    UNPACKB = 0xDB,     // Unpack bytes

    // MMX Operations (0xE0-0xEF range)
    MOVQ = 0xE0,        // Move Quadword
    PADDB = 0xE1,       // Add Packed Bytes
    PADDW = 0xE2,       // Add Packed Words
    PADDD = 0xE3,       // Add Packed Doublewords
    PSUBB = 0xE4,       // Subtract Packed Bytes
    PSUBW = 0xE5,       // Subtract Packed Words
    PSUBD = 0xE6,       // Subtract Packed Doublewords
    PCMPEQB = 0xE7,     // Compare Packed Bytes for Equality
    PCMPEQW = 0xE8,     // Compare Packed Words for Equality
    PCMPEQD = 0xE9,     // Compare Packed Doublewords for Equality
    EMMS = 0xEA,        // Empty MMX State

    // Interrupt operations
    INT = 0xCD,         // Software interrupt
    IRET = 0xCF,        // Interrupt return
    CLI = 0xFA,         // Clear interrupt flag (disable interrupts)
    STI = 0xFB,         // Set interrupt flag (enable interrupts)

    HALT = 0xFF         // Halt execution
};

} // namespace Assembler
