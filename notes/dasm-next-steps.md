# DASM Assembler — Next Steps

## Current: R8–R15 Register Support for x86-64

Add R8–R15 (and sub-register variants R8D–R15D, R8W–R15W, R8B–R15B) to
the lexer and x86 backend. These use REX.B/REX.R prefix bits to extend
the 3-bit register fields in ModR/M and opcode bytes to 4 bits.

### Implementation Plan
1. Lexer: add R8–R15, R8D–R15D, R8W–R15W, R8B–R15B register name maps
2. parse_reg_id: return IDs 8-15 for new registers
3. Encoding: emit REX prefix (0x41-0x4F) when source/dest reg >= 8
4. Update all instruction encoders to accept reg IDs 0-15
5. Test: MOV R8, R9; ADD R10, R11; PUSH R12; etc.

### Design Note
- REX prefix byte = 0x40 | (W << 3) | (R << 2) | (X << 1) | B
- REX.W already used for 64-bit operand size (0x48)
- REX.R = 1 when reg field (in ModR/M) uses bit 3
- REX.B = 1 when rm/base field uses bit 3
- REX.X = 1 when index field uses bit 3 (for SIB addressing)
- REX byte always comes before the opcode byte

### Remaining Priority
1. ✅ R8–R15 registers ← IN PROGRESS
2. "Programming from the Ground Up" book examples
3. PUSH imm / PUSH mem / more opcodes
4. ELF64 executable output
5. `.macro` / `.endm` preprocessor
6. VM opcode handlers (79 mnemonics)
