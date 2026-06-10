# Future Opcode Implementation Plan

**Status**: Planning Phase  
**Goal**: Expand from 63 to 151 total opcodes  
**Timeline**: Q1-Q2 2026  
**Purpose**: Enable native x86-64 code generation and professional-grade computing

---

## Overview

DemiEngine currently has **63 core opcodes** fully implemented and tested. These provide solid arithmetic, logic, memory, control flow, I/O, and stack operations. To enable seamless translation to native x86-64 code and support advanced computing applications, we're implementing **88 additional opcodes** across five categories.

### Current Status (94 Opcodes - 100% Complete)

✅ **Core Arithmetic**: ADD, SUB, MUL, DIV, INC, DEC (+ 64-bit variants)  
✅ **Logic Operations**: AND, OR, XOR, NOT, SHL, SHR  
✅ **Memory Operations**: LOAD, STORE, MOV, LEA, SWAP (+ 64-bit/extended variants)  
✅ **Control Flow**: JMP, JZ, JNZ, JG, JL, JGE, JLE, JC, JNC, JO, JNO, JS, JNS, CALL, RET  
✅ **Stack Operations**: PUSH, POP, PUSH_FLAG, POP_FLAG, PUSH_ARG, POP_ARG  
✅ **I/O Operations**: IN, OUT (byte/word/long variants), INSTR, OUTSTR  
✅ **Comparison**: CMP, MODECMP  
✅ **System**: NOP, HALT, DB, MODE32, MODE64  
✅ **FPU Operations**: ALL 23 floating-point opcodes implemented and tested (FLD, FST, FSTP, FADD, FSUB, FMUL, FDIV, etc.)
✅ **SIMD Operations**: 8 basic vector opcodes implemented (VADD, VMUL, VDOT, VMAX, VBROADCAST, VCMPGT, PACKB, UNPACKB)

### ⚠️ CRITICAL: Performance Optimization Required

**Current Status**: VM interpretation overhead causing 5-20x performance penalty

- **95-case switch dispatcher** with branch prediction misses
- **Per-instruction overhead** from bounds checking, logging, flag calculations
- **Function call overhead** for each opcode handler

**Immediate Priority**: VM optimization before continuing with remaining opcodes

- **Quick wins available**: Threaded code + inlining for 2-5x improvement
- **Timeline**: 2-4 weeks
- **See**: `docs/development/VM_PERFORMANCE_ANALYSIS.md` for detailed analysis

### Remaining Planned Additions (57 Opcodes)

🔄 **SIMD/SSE** (18 opcodes) - Advanced vector operations [NEXT PRIORITY]  
🔄 **Extended 64-bit** (18 opcodes) - Complete addressing modes  
🔄 **AVX** (20 opcodes) - Advanced vector processing  
🔄 **MMX** (11 opcodes) - Legacy multimedia extensions

---

## ✅ **Phase 1 COMPLETE: Floating-Point Unit (FPU)**

**Status**: ✅ **100% COMPLETE** - All 23 FPU opcodes implemented  
**Completed**: October 2025  
**Result**: Full floating-point arithmetic support operational

### FPU Implementation Summary

**All 23 FPU opcodes implemented in `src/engine/opcodes/f*.hpp`:**

#### Load/Store Operations ✅

- **FLD** (0xA0) - Load floating-point value onto FP stack
- **FST** (0xA1) - Store floating-point value from FP stack
- **FSTP** (0xA2) - Store floating-point value and pop stack
- **FILD** (0xA3) - Load integer as floating-point
- **FIST** (0xA4) - Store floating-point as integer
- **FISTP** (0xA5) - Store floating-point as integer and pop

#### Arithmetic Operations ✅

- **FADD** (0xA6) - Floating-point addition
- **FSUB** (0xA7) - Floating-point subtraction
- **FMUL** (0xA8) - Floating-point multiplication
- **FDIV** (0xA9) - Floating-point division

#### Mathematical Functions ✅

- **FSIN** (0xAA) - Sine function
- **FCOS** (0xAB) - Cosine function
- **FTAN** (0xAC) - Tangent function
- **FSQRT** (0xAD) - Square root
- **FABS** (0xAE) - Absolute value
- **FCHS** (0xAF) - Change sign

#### Comparison & Control ✅

- **FINIT** (0xB0) - Initialize FPU
- **FCLEX** (0xB1) - Clear exception flags
- **FSTCW** (0xB2) - Store control word
- **FLDCW** (0xB3) - Load control word
- **FSTSW** (0xB4) - Store status word
- **FCOMPP** (0xB5) - Compare and pop twice
- **FUCOMPP** (0xB6) - Unordered compare and pop twice

**Achievement**: Complete floating-point support enables scientific computing, graphics, physics simulations, and financial applications.

---

## 🚀 **Phase 2 IMMEDIATE: VM Performance Optimization**

**Priority**: ⚠️ **CRITICAL** - Must address before continuing opcode expansion  
**Timeline**: 2-4 weeks (parallel with SIMD planning)  
**Goal**: Reduce VM interpretation penalty from 5-20x to 2-5x slower than native

### Performance Bottlenecks Identified

1. **Switch Dispatch Overhead** - 87-case switch causing branch prediction misses
2. **Per-Instruction Overhead** - Bounds checking, logging, flag calculations
3. **Function Call Overhead** - Separate handler functions for each opcode
4. **Memory Access Patterns** - 134 virtual registers vs 16 native

### Quick Win Optimizations (2-4 weeks)

1. **Threaded Code Interpretation** → 2-3x faster dispatch
2. **Instruction Fusion** → 1.5-2x faster for common sequences
3. **Aggressive Inlining** → Eliminate function call overhead
4. **Conditional Compilation** → 2-3x faster in release builds

**Expected Result**: 5-10x overall performance improvement

**Documentation**: See `docs/development/VM_PERFORMANCE_ANALYSIS.md`

---

## 🎯 **Phase 3 NEXT: SIMD/Vector Operations**

**Priority**: HIGH - Next major capability expansion  
**Timeline**: Q1 2026 (4-6 weeks after VM optimization)  
**Goal**: Enable vectorized computation for performance-critical applications

```
FLD    (0xA0) - Load floating-point value onto FP stack
FST    (0xA1) - Store floating-point value from FP stack
FSTP   (0xA2) - Store floating-point value and pop stack
FILD   (0xA3) - Load integer as floating-point
FIST   (0xA4) - Store floating-point as integer
FISTP  (0xA5) - Store floating-point as integer and pop
```

#### Arithmetic Operations

```
FADD   (0xA6) - Floating-point add
FSUB   (0xA7) - Floating-point subtract
FMUL   (0xA8) - Floating-point multiply
FDIV   (0xA9) - Floating-point divide
FADDP  (0xAA) - Add and pop
FSUBP  (0xAB) - Subtract and pop
FMULP  (0xAC) - Multiply and pop
FDIVP  (0xAD) - Divide and pop
```

#### Mathematical Functions

```
FABS   (0xAE) - Absolute value
FCHS   (0xAF) - Change sign
FSQRT  (0xB0) - Square root
FSIN   (0xB1) - Sine
FCOS   (0xB2) - Cosine
FTAN   (0xB3) - Tangent
```

#### Constants

```
FLDZ   (0xB4) - Load +0.0
FLD1   (0xB5) - Load +1.0
FLDPI  (0xB6) - Load π
```

### Implementation Details

**FPU Stack**: 8-register stack (ST0-ST7) following x87 architecture  
**Precision**: IEEE 754 double-precision (64-bit)  
**x86-64 Mapping**: Direct mapping to x87 FPU instructions

**Example Usage**:

```asm
; Calculate area of circle: π * r²
FLD radius        ; ST0 = radius
FMUL ST0, ST0     ; ST0 = radius²
FLDPI             ; ST0 = π, ST1 = radius²
FMUL              ; ST0 = π * radius²
FST area          ; Store result
```

---

## Phase 2: SIMD/SSE Operations - HIGH PRIORITY

**Opcodes**: 26  
**Timeline**: Q1 2026 (4-6 weeks)  
**Rationale**: Performance multiplier - enables vectorized computation for graphics, audio, ML

### Why SIMD/SSE?

1. **Performance**: Process 4 floats simultaneously (4x throughput)
2. **Graphics/Image Processing**: Essential for real-time rendering
3. **Audio/Video**: Required for media processing
4. **Scientific Computing**: Accelerates numerical computations
5. **Machine Learning**: Vector operations for inference
6. **Native Mapping**: Direct mapping to SSE/SSE2 instructions

### SIMD/SSE Opcodes to Implement

#### Packed Single-Precision (128-bit, 4x float32)

```
MOVPS    (0xC0) - Move packed single-precision (aligned)
MOVUPS   (0xC1) - Move packed single-precision (unaligned)
MOVAPS   (0xC2) - Move aligned packed single-precision
ADDPS    (0xC3) - Add packed single-precision
SUBPS    (0xC4) - Subtract packed single-precision
MULPS    (0xC5) - Multiply packed single-precision
DIVPS    (0xC6) - Divide packed single-precision
MAXPS    (0xC7) - Maximum of packed single-precision
MINPS    (0xC8) - Minimum of packed single-precision
SQRTPS   (0xC9) - Square root packed single-precision
RCPPS    (0xCA) - Reciprocal packed single-precision
RSQRTPS  (0xCB) - Reciprocal square root packed
ANDPS    (0xCC) - Bitwise AND packed single-precision
ORPS     (0xCD) - Bitwise OR packed single-precision
XORPS    (0xCE) - Bitwise XOR packed single-precision
```

#### Scalar Single-Precision

```
MOVSS    (0xCF) - Move scalar single-precision
ADDSS    (0xD0) - Add scalar single-precision
SUBSS    (0xD1) - Subtract scalar single-precision
MULSS    (0xD2) - Multiply scalar single-precision
DIVSS    (0xD3) - Divide scalar single-precision
MAXSS    (0xD4) - Maximum scalar single-precision
MINSS    (0xD5) - Minimum scalar single-precision
SQRTSS   (0xD6) - Square root scalar
RCPSS    (0xD7) - Reciprocal scalar
RSQRTSS  (0xD8) - Reciprocal square root scalar
```

#### Comparison

```
CMPPS    (0xD9) - Compare packed single-precision
```

### Implementation Details

**XMM Registers**: 16 registers (XMM0-XMM15), 128 bits each  
**Data Layout**: 4x 32-bit floats per register  
**x86-64 Mapping**: Direct mapping to SSE/SSE2 instructions

**Example Usage**:

```asm
; Add two vectors of 4 floats each
MOVUPS XMM0, [vector_a]    ; Load 4 floats from vector_a
MOVUPS XMM1, [vector_b]    ; Load 4 floats from vector_b
ADDPS  XMM0, XMM1          ; Add all 4 pairs simultaneously
MOVUPS [result], XMM0      ; Store result
```

---

## Phase 3: Extended 64-bit Operations - MEDIUM PRIORITY

**Opcodes**: 18  
**Timeline**: Q1-Q2 2026 (2-3 weeks)  
**Rationale**: Complete register architecture and large program support

### Why Extended 64-bit?

1. **Large Programs**: Support programs >4GB
2. **Complete Architecture**: Utilize all 134 registers fully
3. **Advanced Addressing**: Extended memory access modes
4. **Future-Proofing**: Ready for modern system requirements

### Extended Opcodes to Implement

#### Memory Operations

```
LOADEX      (0x50) - Load with extended addressing
STOREEX     (0x51) - Store with extended addressing
LOAD_IMMEX  (0x52) - Load immediate to extended register
```

#### Control Flow

```
JMPEX   (0x53) - Jump with extended addressing
CALLEX  (0x54) - Call with extended addressing
JZEX    (0x55) - Jump if zero (extended)
JNZEX   (0x56) - Jump if not zero (extended)
JGEX    (0x57) - Jump if greater (extended)
JLEX    (0x58) - Jump if less (extended)
JGEEX   (0x59) - Jump if greater/equal (extended)
JLEEX   (0x5A) - Jump if less/equal (extended)
JCEX    (0x5B) - Jump if carry (extended)
JNCEX   (0x5C) - Jump if no carry (extended)
JOEX    (0x5D) - Jump if overflow (extended)
JNOEX   (0x5E) - Jump if no overflow (extended)
JSEX    (0x5F) - Jump if sign (extended)
JNSEX   (0x60) - Jump if no sign (extended)
```

#### Comparison

```
CMPEX   (0x61) - Compare extended registers
```

---

## Phase 4: AVX Operations - MEDIUM PRIORITY

**Opcodes**: 20  
**Timeline**: Q2 2026 (3-4 weeks)  
**Rationale**: Modern CPU optimization - 256-bit vectors (8x float32)

### Why AVX?

1. **Double Throughput**: Process 8 floats vs SSE's 4
2. **Modern CPUs**: Standard on Intel/AMD since 2011
3. **Future-Proofing**: Professional-grade vector processing
4. **Scientific Computing**: Essential for HPC applications

### AVX Opcodes to Implement

#### 256-bit Packed Operations

```
VMOVPS   (0xE0) - Move packed single-precision (256-bit)
VMOVUPS  (0xE1) - Move unaligned packed (256-bit)
VMOVAPS  (0xE2) - Move aligned packed (256-bit)
VADDPS   (0xE3) - Add 8x float32
VSUBPS   (0xE4) - Subtract 8x float32
VMULPS   (0xE5) - Multiply 8x float32
VDIVPS   (0xE6) - Divide 8x float32
VMAXPS   (0xE7) - Maximum 8x float32
VMINPS   (0xE8) - Minimum 8x float32
VSQRTPS  (0xE9) - Square root 8x float32
VANDPS   (0xEA) - Bitwise AND (256-bit)
VORPS    (0xEB) - Bitwise OR (256-bit)
VXORPS   (0xEC) - Bitwise XOR (256-bit)
```

#### Scalar Operations

```
VMOVSS   (0xED) - Move scalar single-precision
VADDSS   (0xEE) - Add scalar
VSUBSS   (0xEF) - Subtract scalar
VMULSS   (0xF0) - Multiply scalar
VDIVSS   (0xF1) - Divide scalar
VMAXSS   (0xF2) - Maximum scalar
VMINSS   (0xF3) - Minimum scalar
```

**YMM Registers**: 16 registers (YMM0-YMM15), 256 bits each  
**Data Layout**: 8x 32-bit floats per register

---

## Phase 5: MMX Operations - LOW PRIORITY

**Opcodes**: 11  
**Timeline**: Q2 2026 (1-2 weeks)  
**Rationale**: Legacy support and integer vector operations

### Why MMX?

1. **Legacy Compatibility**: Support older codebases
2. **Integer Vectors**: 64-bit SIMD for integers
3. **Completeness**: Full x86-64 instruction set coverage
4. **Multimedia**: Specialized image/audio processing

### MMX Opcodes to Implement

```
MOVQ   (0x70) - Move 64-bit quadword
PADDB  (0x71) - Packed add bytes
PADDW  (0x72) - Packed add words
PADDD  (0x73) - Packed add doublewords
PSUBB  (0x74) - Packed subtract bytes
PSUBW  (0x75) - Packed subtract words
PSUBD  (0x76) - Packed subtract doublewords
PMULLW (0x77) - Packed multiply low words
PAND   (0x78) - Packed bitwise AND
POR    (0x79) - Packed bitwise OR
PXOR   (0x7A) - Packed bitwise XOR
```

**MM Registers**: 8 registers (MM0-MM7), 64 bits each

---

## Implementation Strategy

### Development Approach

1. **Opcode Definitions** ✅ Already Complete
   - All 88 opcodes defined in `src/assembler/opcodes.hpp`
   - Assembler can parse all opcodes
   - Ready for CPU implementation

2. **CPU Dispatcher Implementation**
   - Add case statements to `src/engine/opcodes/opcodes_consolidated.cpp`
   - Follow existing patterns and architecture
   - Implement execution logic for each opcode

3. **Test-Driven Development**
   - Write unit tests BEFORE implementing each opcode
   - Create assembly test programs
   - Verify correct behavior and flag handling
   - Maintain 100% test coverage target

4. **x86-64 Alignment**
   - Ensure each opcode maps cleanly to native instructions
   - Document translation patterns
   - Design with native codegen in mind

5. **Performance Validation**
   - Benchmark implementations
   - Optimize critical paths
   - Verify SIMD/FPU correctness

### Priority Rationale

**FPU First** → Most fundamental, widest application  
**SIMD Second** → Biggest performance impact  
**Extended 64-bit Third** → Complete architecture  
**AVX Fourth** → Modern optimization  
**MMX Last** → Legacy support, less critical

### Success Criteria

- ✅ All 151 opcodes implemented in CPU dispatcher
- ✅ 100% test coverage maintained
- ✅ Clear x86-64 mapping documented for each opcode
- ✅ Assembly programs can use all opcodes
- ✅ Performance benchmarks validate implementations
- ✅ Ready for native code generation phase

---

## Native Code Generation Readiness

**After Implementation**: With all 151 opcodes complete, we'll have:

1. **Complete Instruction Set**: Every opcode proven through VM execution
2. **Clear Native Mapping**: Each opcode documents its x86-64 equivalent
3. **Comprehensive Tests**: Full validation suite ensures correctness
4. **Seamless Translation**: Direct path from D-ISA → x86-64 assembly

**Next Phase**: Implement native code generator using documented opcode mappings, then build Demi high-level language on top of the complete infrastructure.

---

## Phase 6: Multithreading Support — MEDIUM PRIORITY

**Opcodes**: 10  
**Timeline**: Q2-Q3 2026 (3-5 weeks)  
**Rationale**: Enable concurrent execution within a single VM instance

### Why Multithreading?

1. **Modern workloads**: Parallel computation is the norm — data processing, servers, simulations
2. **Host OS utilization**: Demi threads map 1:1 to OS threads via `clone()`, letting the kernel schedule across real CPU cores
3. **Shared-memory model**: All threads share the Demi VM's flat memory space (the `memory[]` array), matching the x86 threading model
4. **Foundation for Demi language**: The high-level Demi language will need `spawn`/`sync` primitives; threading in DASM provides the ISA-level substrate

### Threading Model

- **1:1 threading**: Each DASM thread is a real OS thread created via `SYS_CLONE`
- **Shared everything**: All threads share the VM's 64KB memory space, register file, and I/O state
- **Cooperative by default**: Threads run independently; synchronization is explicit via atomics and futexes
- **Sandbox-aware**: Thread creation is gated by `--allow-exec` capability flag; without it, `THREAD_SPAWN` returns `-EPERM`

### New Opcodes

#### Thread Lifecycle

```
THREAD_SPAWN (0x??) - Spawn a new thread
  Input:  RDI = entry point address, RSI = initial stack pointer
  Output: RAX = thread ID on success, negative errno on failure

THREAD_JOIN  (0x??) - Wait for a thread to exit
  Input:  RDI = thread ID
  Output: RAX = exit code of the joined thread, or negative errno

THREAD_EXIT  (0x??) - Terminate the calling thread
  Input:  RDI = exit code
  Output: Does not return
```

#### Atomic Operations (LOCK prefix)

```
LOCK ADD  (0x??) - Atomic add to memory
LOCK SUB  (0x??) - Atomic subtract from memory
LOCK XCHG (0x??) - Atomic exchange register with memory
```

#### Atomic Compare-and-Exchange

```
CMPXCHG (0x??) - Compare and exchange
  Compares RAX with memory at [RDI]. If equal, stores RSI to [RDI] and sets ZF.
  If not equal, loads [RDI] into RAX and clears ZF.
```

#### Memory Ordering

```
MFENCE (0x??) - Full memory fence (StoreLoad barrier)
LFENCE (0x??) - Load fence (LoadLoad + LoadStore barrier)
SFENCE (0x??) - Store fence (StoreStore barrier)
```

#### Thread Introspection

```
THREAD_ID (0x??) - Get current thread ID
  Output: RAX = calling thread's ID
```

### New Syscalls Required

| Syscall | Number (x86-64) | Purpose |
|---|---|---|
| `SYS_CLONE` | 56 | Create child thread/process |
| `SYS_FUTEX` | 202 | Fast userspace mutex (used by THREAD_JOIN) |
| `SYS_SET_TID_ADDRESS` | 218 | Set pointer to thread ID for robust futexes |
| `SYS_EXIT` | 60 | Already wired — used by THREAD_EXIT |

### x86-64 Translation

The native compiler already emits `syscall` (0F 05) for INT 0x80. Threading opcodes translate directly to x86-64 instructions, keeping the zero-VM-overhead guarantee:

| DASM Instruction | x86-64 Translation | Notes |
|---|---|---|
| `THREAD_SPAWN rdi, rsi` | `mov rax, 56`<br>`mov rdi, 0x3D0F00`<br>`mov rsi, [entry]`<br>`mov rdx, [stack]`<br>`xor r10, r10`<br>`xor r8, r8`<br>`xor r9, r9`<br>`syscall` | `0x3D0F00` = CLONE_VM\|CLONE_FS\|CLONE_FILES\|CLONE_SIGHAND\|CLONE_THREAD\|CLONE_SYSVSEM\|CLONE_SETTLS\|CLONE_PARENT_SETTID\|CLONE_CHILD_CLEARTID. Child starts at `entry`, uses `stack`. Returns child TID in RAX. |
| `THREAD_JOIN rdi` | Futex wait loop:<br>`mov [clear_tid], rdi`<br>`mov rax, 202`<br>`lea rdi, [join_word]`<br>`mov rsi, 0` (FUTEX_WAIT)<br>`mov rdx, 0`<br>`syscall` | Spins on a futex word the child clears on exit via CLONE_CHILD_CLEARTID. Falls through when child terminates. |
| `THREAD_EXIT rdi` | `mov rax, 60`<br>`syscall` | SYS_EXIT. Kernel clears the child_tid word (set by CLONE_CHILD_CLEARTID) and wakes any futex waiters. |
| `LOCK ADD r32, [mem]` | `lock add [mem], r32` | Hardware atomic. `lock` prefix (F0) + `add` opcode. |
| `LOCK CMPXCHG r32, [mem]` | `lock cmpxchg [mem], r32` | Hardware CAS. Implicitly uses RAX as the comparand. |
| `CMPXCHG r32, [mem]` | `cmpxchg [mem], r32` | Non-atomic variant for single-threaded optimization. |
| `MFENCE` | `mfence` | Full barrier (0F AE F0). Drains store buffer + load queue. |
| `LFENCE` | `lfence` | Load barrier (0F AE E8). Serializes load execution. |
| `SFENCE` | `sfence` | Store barrier (0F AE F8). Flushes write-combining buffer. |
| `THREAD_ID` | `mov rax, [fs:0x2D0]` | Reads the thread's TID from the TLS area (offset depends on glibc internals; use `syscall(SYS_gettid)` as fallback: `mov rax, 186; syscall`). |

### Thread Lifecycle in Detail

```
  Main thread                          Child thread
  ──────────                           ────────────
  THREAD_SPAWN entry, stack ──────→    starts at entry
    returns child TID                    uses provided stack
       │                                  │
       │  (main continues)                │  (child works)
       │                                  │
  THREAD_JOIN child_tid                   │
    blocks on futex ←───────────────    THREAD_EXIT 0
    wakes with exit code                  kernel clears clear_tid
       │                                  futex wakes waiter
       ▼                                  thread terminates
  continues with result
```

### Example: Parallel Sum with LOCK ADD

```asm
; Spawn 4 threads, each adds a quarter of an array to shared sum
; shared_sum is a memory location protected by LOCK ADD
; Demo of: THREAD_SPAWN, THREAD_JOIN, LOCK ADD

LOAD_IMM R1, shared_sum_ptr
STORE 0, [R1]                 ; shared_sum = 0

; Spawn 4 workers
LOAD_IMM RDI, worker_entry
LOAD_IMM RSI, worker_stack_1
THREAD_SPAWN                   ; RAX = tid1
MOV R8, RAX
; ... repeat for 3 more threads ...

; Join all 4
MOV RDI, R8
THREAD_JOIN                    ; wait for tid1
; ... repeat for other 3 ...

; shared_sum now holds the total
HALT

worker_entry:
; compute partial sum into a register...
LOAD_IMM R10, partial_sum
LOAD_IMM R11, shared_sum_ptr
LOCK ADD [R11], R10            ; atomically add to shared sum
THREAD_EXIT 0
```

### Sandbox Considerations

- `THREAD_SPAWN` → gated by `--allow-exec` (spawning threads is process creation)
- `SYS_CLONE` → blocked by sandbox unless `--allow-exec` is set
- Child threads inherit the parent's VFS jail — they can't escape the sandbox
- `CLONE_VM` flag means all threads share the same virtual memory; this is safe because the sandbox already prevents guest code from accessing host memory

### Implementation Notes

1. **Thread safety of the VM**: The DASM interpreter is single-threaded per CPU. When THREAD_SPAWN is called, the interpreter spawns a new OS thread that runs its own interpreter loop on a cloned CPU state. The shared `memory[]` array needs a mutex for the interpreter (or we switch to lock-free per-byte access).
2. **Register file sharing**: Each thread gets its own register context (134 registers). The register file is NOT shared — this prevents data races on register state.
3. **elf_emitter changes**: Threaded native executables need `PT_TLS` segment for thread-local storage (CLONE_SETTLS requires it). The ELF emitter currently only generates `PT_LOAD` segments.
4. **Futex word layout**: THREAD_JOIN uses a per-thread futex word in the VM's shared memory to implement joining. The parent writes the child's TID to the futex word, then FUTEX_WAITs. When the child calls THREAD_EXIT → SYS_EXIT, the kernel atomically clears the word at CLONE_CHILD_CLEARTID and wakes the futex.

---

## Previously Claimed Features

**Note**: Earlier documentation claimed these features were implemented. They are now correctly marked as **planned additions** for Q1-Q2 2026:

- ⚠️ **SIMD Support** → Planned for Q1 2026 (Phase 2)
- ⚠️ **FPU Operations** → Planned for Q1 2026 (Phase 1)
- ⚠️ **AVX Support** → Planned for Q2 2026 (Phase 4)
- ⚠️ **MMX Operations** → Planned for Q2 2026 (Phase 5)

All of these will become **implemented features** once Stage 1 (Assembly Language Expansion) is complete.

---

## References

- **Opcode Definitions**: `src/assembler/opcodes.hpp` (all 151 opcodes defined)
- **Current Implementation**: `src/engine/opcodes/opcodes_consolidated.cpp` (63 opcodes)
- **Complete Roadmap**: `/workspaces/demi/roadmap.md`
- **Verification Findings**: `/workspaces/demi/DOCUMENTATION_VERIFICATION_FINDINGS.md`
- **Quick Reference**: `/workspaces/demi/docs/QUICK_REFERENCE.md`

---

**Last Updated**: October 3, 2025  
**Status**: Planning and documentation phase complete, ready for implementation

## Missing Core Operations (Pending)
The following basic memory and arithmetic opcodes are lacking and currently break standard examples, needing urgent implementation:
- STORE_INDIRECT - Store register value to memory using indirect addressing
- LOAD_INDIRECT - Load value from memory to register using indirect addressing
- ADD_IMM - Add immediate value to register
