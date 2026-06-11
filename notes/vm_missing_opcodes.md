# Demi VM — Missing Opcode Handlers for Assembler Mnemonics

**Date:** 2026-06-11  
**Context:** The assembler lexer now recognizes 83+ x86 mnemonics for the x86/x64 backend.  
The Demi VM's opcode registry implements only the original Demi ISA.  
These mnemonics parse correctly but have NO VM handler — they would cause "unknown opcode" if assembled to Demi bytecode and executed.

---

## New Mnemonics Missing VM Handlers

### Arithmetic with Carry (2)
- **ADC** — add with carry (ADD exists, but no carry flag propagation)
- **SBB** — subtract with borrow (SUB exists, but no borrow flag propagation)

### Rotates (4)
- **ROL** — rotate left
- **ROR** — rotate right
- **RCL** — rotate left through carry
- **RCR** — rotate right through carry

### Arithmetic Shifts (2)
- **SAL** — shift arithmetic left (SHL exists but SAL is semantically distinct)
- **SAR** — shift arithmetic right (SHR exists but SAR sign-extends)

### Signed Multiply/Divide (2)
- **IMUL** — signed multiply (MUL exists as unsigned; IMUL has 1/2/3 operand forms)
- **IDIV** — signed divide (DIV exists as unsigned)

### Sign/Zero Extend Moves (2)
- **MOVSX** — move with sign extension (byte→dword, word→dword)
- **MOVZX** — move with zero extension

### Conditional Set (12)
- **SETZ, SETNZ, SETC, SETNC, SETO, SETNO, SETS, SETNS, SETG, SETGE, SETL, SETLE**  
  Set byte to 0/1 based on condition flags

### Exchange (1)
- **XCHG** — exchange register/memory contents (SWAP exists but is different)

### Byte Swap (1)
- **BSWAP** — reverse byte order (endian conversion)

### Flag Operations (7)
- **CLC** — clear carry flag
- **STC** — set carry flag
- **CMC** — complement carry flag
- **CLD** — clear direction flag
- **STD** — set direction flag
- **LAHF** — load flags into AH
- **SAHF** — store AH into flags

### Sign Extension (4)
- **CBW** — convert byte to word (AL→AX)
- **CWDE** — convert word to doubleword (AX→EAX)
- **CWD** — convert word to doubleword (AX→DX:AX)
- **CDQ** — convert doubleword to quadword (EAX→EDX:EAX)

### Stack Frame (1)
- **ENTER** — create stack frame with nesting (LEAVE not needed — VM handles stack differently)

### Conditional Moves (16)
- **CMOVZ, CMOVNZ, CMOVC, CMOVNC, CMOVO, CMOVNO, CMOVS, CMOVNS, CMOVG, CMOVGE, CMOVL, CMOVLE, CMOVA, CMOVAE, CMOVB, CMOVBE**  
  Conditionally move data based on flags

### Atomic Operations (2)
- **CMPXCHG** — compare and exchange (requires atomic semantics)
- **XADD** — exchange and add (requires atomic semantics)

### Bit Test (4)
- **BT** — bit test
- **BTS** — bit test and set
- **BTR** — bit test and reset
- **BTC** — bit test and complement

### String Operations (10)
- **MOVSB, MOVSW, MOVSD** — move string (byte/word/dword)
- **STOSB, STOSW, STOSD** — store string
- **LODSB, LODSW, LODSD** — load string
- **REP** — repeat prefix (requires ECX counter + direction flag)

### Processor Identification (2)
- **CPUID** — CPU identification (platform-specific)
- **RDTSC** — read timestamp counter (platform-specific)

### Fast System Calls (2)
- **SYSCALL** — fast system call (x86-64 only)
- **SYSENTER** — fast system call (x86-32 only)

### Counter Loops (3)
- **LOOP** — decrement ECX and jump if not zero
- **LOOPE** — decrement ECX and jump if not zero and ZF=1
- **LOOPNE** — decrement ECX and jump if not zero and ZF=0

---

## Summary

**Total missing VM handlers: 79 mnemonics** across 20 instruction groups.

The assembler (lexer + x86 backend) correctly encodes ALL of these to real x86 machine code.  
The gap is only in the Demi VM — none of these have Demi bytecode opcode assignments or handler implementations.

### Priority for VM Implementation

**High (commonly used in DASM programs):**
- ADC, SBB — needed for multi-precision arithmetic
- IMUL, IDIV — signed variants of existing MUL/DIV
- SAL, SAR — arithmetic shifts complement existing SHL/SHR

**Medium (useful but less critical):**
- Flag ops (CLC, STC, LAHF, SAHF) — needed for conditional logic
- Sign extension (CBW, CWD) — needed for type conversion
- ROL, ROR — rotate instructions
- LOOP — counter loops

**Low (rarely used or x86-specific):**
- CMOVcc, SETcc — modern optimization patterns
- String ops — REP MOVSB etc.
- CMPXCHG, XADD — atomic operations
- CPUID, RDTSC, SYSCALL — platform-specific

**Not applicable to VM:**
- ENTER — VM uses different stack management
- REP prefix — VM has different iteration model
