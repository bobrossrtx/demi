# Demi VM — Missing Opcode Handlers for Assembler Mnemonics

**Date:** 2026-07-09 (updated)  
**Context:** The assembler lexer now recognizes 83+ x86 mnemonics for the x86/x64 backend.  
The Demi VM's opcode registry implements only the original Demi ISA.  
These mnemonics parse correctly but have NO VM handler — they would cause "unknown opcode" if assembled to Demi bytecode and executed.

**Status: COMPLETE.** All 75 unique VM opcodes implemented + 4 x86 aliases (CMOVB/CMOVAE/SETE/SETNE). 79 of 79 mnemonics covered (100%).

---

## New Mnemonics Missing VM Handlers

### Arithmetic with Carry (2)
- ~~**ADC** — add with carry (ADD exists, but no carry flag propagation)~~ ✅ DONE
- ~~**SBB** — subtract with borrow (SUB exists, but no borrow flag propagation)~~ ✅ DONE

### Rotates (4)
- ~~**ROL** — rotate left~~ ✅
- ~~**ROR** — rotate right~~ ✅
- ~~**RCL** — rotate left through carry~~ ✅
- ~~**RCR** — rotate right through carry~~ ✅

### Arithmetic Shifts (2)
- ~~**SAL** — shift arithmetic left (SHL exists but SAL is semantically distinct)~~ ✅ DONE
- ~~**SAR** — shift arithmetic right (SHR exists but SAR sign-extends)~~ ✅ DONE

### Signed Multiply/Divide (2)
- ~~**IMUL** — signed multiply (MUL exists as unsigned; IMUL has 1/2/3 operand forms)~~ ✅ DONE
- ~~**IDIV** — signed divide (DIV exists as unsigned)~~ ✅ DONE

### Sign/Zero Extend Moves (2) ✅
- ~~**MOVSX** — move with sign extension~~ ✅
- ~~**MOVZX** — move with zero extension~~ ✅

### Conditional Set (12) — ALL DONE ✅
- ~~SETZ, SETNZ, SETC, SETNC, SETO, SETNO, SETS, SETNS, SETG, SETGE, SETL, SETLE~~ ✅

### Exchange (1) ✅
- ~~**XCHG** — exchange register/memory contents~~ ✅

### Byte Swap (1) ✅
- ~~**BSWAP** — reverse byte order~~ ✅

### Flag Operations (7) — ALL DONE ✅
- ~~**CLC** — clear carry flag~~ ✅
- ~~**STC** — set carry flag~~ ✅
- ~~**CMC** — complement carry flag~~ ✅
- ~~**CLD** — clear direction flag~~ ✅
- ~~**STD** — set direction flag~~ ✅
- ~~**LAHF** — load flags into AH~~ ✅
- ~~**SAHF** — store AH into flags~~ ✅

### Sign Extension (4) — ALL DONE ✅
- ~~**CBW** — convert byte to word (AL→AX)~~ ✅
- ~~**CWDE** — convert word to doubleword (AX→EAX)~~ ✅
- ~~**CWD** — convert word to doubleword (AX→DX:AX)~~ ✅
- ~~**CDQ** — convert doubleword to quadword (EAX→EDX:EAX)~~ ✅

### Stack Frame (1) ✅
- ~~ENTER — create stack frame~~ ✅

### Conditional Moves (16 mnemonics, 14 opcodes) ✅
- ~~CMOVZ/E, CMOVNZ/NE, CMOVC/B/NAE, CMOVNC/NB/AE, CMOVO, CMOVNO, CMOVS, CMOVNS, CMOVG/NLE, CMOVGE/NL, CMOVL/NGE, CMOVLE/NG, CMOVA/NBE, CMOVBE/NA~~ ✅

### Atomic Operations (2) ✅
- ~~CMPXCHG, XADD~~ ✅

### Bit Test (4) ✅
- ~~BT, BTS, BTR, BTC~~ ✅

### String Operations (10) — ALL DONE ✅
- ~~MOVSB/SW/SD, STOSB/SW/SD, LODSB/SW/SD, REP~~ ✅

### Processor Identification (2) ✅
- ~~CPUID, RDTSC~~ ✅

### Fast System Calls (2) ✅
- ~~SYSCALL, SYSENTER~~ ✅ (both delegate to INT 0x80)

### Counter Loops (3) — ALL DONE ✅
- ~~**LOOP** — decrement ECX and jump if not zero~~ ✅
- ~~**LOOPE** — decrement ECX and jump if not zero and ZF=1~~ ✅
- ~~**LOOPNE** — decrement ECX and jump if not zero and ZF=0~~ ✅

---

## Summary

**Total: 79/79 mnemonics covered (100%).** 75 unique opcodes + 4 aliases. Opcode space 0x00-0xFF fully allocated.

The Demi VM now handles every mnemonic the assembler recognizes for the Demi bytecode target.

### Priority for VM Implementation

**High (commonly used in DASM programs) — ALL DONE ✅:**
- ~~ADC, SBB — needed for multi-precision arithmetic~~ ✅
- ~~IMUL, IDIV — signed variants of existing MUL/DIV~~ ✅
- ~~SAL, SAR — arithmetic shifts complement existing SHL/SHR~~ ✅

**Medium (useful but less critical) — ALL DONE ✅:**
- ~~Flag ops (CLC, STC, LAHF, SAHF)~~ ✅
- ~~Sign extension (CBW, CWD)~~ ✅
- ~~ROL, ROR~~ ✅
- ~~LOOP~~ ✅

**ALL categories DONE ✅ — 100% coverage.** The Demi VM is now a complete x86-subset VM.
