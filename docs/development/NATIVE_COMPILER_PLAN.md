# D-ISA to x86-64 Native Compiler Implementation Plan

> Status note: this document started as the implementation plan for the native compiler.
> The encoder, register allocator, ELF emitter, and `DISAToX86Compiler` are now implemented.
> Native compilation currently covers core arithmetic, control flow, memory operations,
> INT 0x80 bridging, and the x87 FPU showcase in
> `examples/x86/features/fpu_showcase.asm`, including FINIT, FCLEX, FLD, FST,
> FSTP, FILD, FIST, FISTP, FADD, FSUB, FMUL, FDIV, FSIN, FCOS, FTAN, FSQRT,
> FABS, FCHS, FSTCW, FLDCW, FSTSW, FCOMPP, and FUCOMPP.
> The remaining sections below are best read as historical planning notes and future
> optimization ideas, not as a current checklist.

## 🎯 Overview

Transform DemiEngine from a VM-based system to a true native code compiler that translates D-ISA bytecode directly to x86-64 machine code.

## 📋 Implementation Phases

### Phase 1: Foundation (Weeks 1-2)

**Goal**: Basic x86-64 instruction encoding capability

#### Week 1: x86-64 Encoder Core

- [ ] Implement `X86Encoder` basic instruction emission
  - [ ] MOV (register-to-register, immediate-to-register)
  - [ ] ADD/SUB (register-to-register)
  - [ ] CMP (register-to-register)
  - [ ] JMP/JZ/JNZ (relative jumps)
  - [ ] NOP, RET
- [ ] REX prefix handling for 64-bit operations
- [ ] ModR/M byte encoding for register addressing
- [ ] Basic unit tests for instruction encoding

#### Week 2: Register Allocation Foundation

- [ ] Implement `RegisterAllocator` core functionality
  - [ ] Simple linear scan allocation
  - [ ] Basic spill/reload mechanism
  - [ ] Register lifetime tracking
- [ ] Map DemiEngine's 134 registers to x86-64's 16 registers
- [ ] Stack frame management for spilled registers

### Phase 2: Core Translation (Weeks 3-4)

**Goal**: Translate basic D-ISA instructions to x86-64

#### Week 3: Instruction Translation Framework

- [ ] Implement `DISAToX86Compiler` structure
- [ ] D-ISA bytecode parsing and decoding
- [ ] Jump target scanning and label management
- [ ] Basic instruction translation:
  - [ ] NOP → NOP
  - [ ] LOAD_IMM → MOV reg, imm64
  - [ ] ADD → ADD reg, reg (with register allocation)
  - [ ] SUB → SUB reg, reg
  - [ ] MOV → MOV reg, reg
  - [ ] CMP → CMP reg, reg

#### Week 4: Control Flow Translation

- [ ] Implement jump instruction translation
  - [ ] JMP → JMP rel32
  - [ ] JZ → JZ rel32
  - [ ] JNZ → JNZ rel32
- [ ] Forward reference resolution for jumps
- [ ] Basic block identification and optimization

### Phase 3: Memory Operations (Weeks 5-6)

**Goal**: Handle memory access and stack operations

#### Week 5: Memory Access Translation

- [ ] LOAD instruction → MOV reg, [base+offset]
- [ ] STORE instruction → MOV [base+offset], reg
- [ ] Memory addressing mode translation
- [ ] Handle DemiEngine's memory model vs x86-64 memory model

#### Week 6: Stack Operations

- [ ] PUSH instruction → PUSH reg
- [ ] POP instruction → POP reg
- [ ] Function call/return mechanism
- [ ] Stack frame setup and teardown

### Phase 4: Advanced Features (Weeks 7-8)

**Goal**: Handle complex D-ISA features

#### Week 7: Extended Instruction Set

- [ ] Arithmetic operations (MUL, DIV, bitwise ops)
- [ ] Extended addressing modes
- [ ] SIMD instruction translation (if applicable)
- [ ] Flag handling and conditional operations

#### Week 8: Device I/O Integration

- [ ] Device I/O instruction translation
- [ ] Runtime function calls for complex operations
- [ ] System call interface

### Phase 5: Optimization & Testing (Weeks 9-10)

**Goal**: Performance optimization and comprehensive testing

#### Week 9: Optimization Passes

- [ ] Dead code elimination
- [ ] Redundant move elimination
- [ ] Constant folding
- [ ] Peephole optimization
- [ ] Register allocation improvements

#### Week 10: Integration & Testing

- [ ] Integrate compiler into main DemiEngine executable
- [ ] Add `--native-compile` flag to command line
- [ ] Comprehensive test suite
- [ ] Performance benchmarking vs VM execution
- [ ] Debug output and error handling

## 🔧 Technical Challenges & Solutions

### Challenge 1: Register Pressure

**Problem**: DemiEngine has 134 registers, x86-64 has 16
**Solution**:

- Intelligent register allocation with spilling
- Live range analysis to minimize spills
- Register renaming for better utilization

### Challenge 2: Memory Model Differences

**Problem**: DemiEngine's memory addressing vs x86-64
**Solution**:

- Abstract memory interface
- Address translation layer
- Efficient mapping of DemiEngine addressing modes

### Challenge 3: Device I/O Translation

**Problem**: DemiEngine's device model doesn't map to x86-64
**Solution**:

- Runtime library for device operations
- Function calls for complex I/O operations
- Maintain device compatibility layer

### Challenge 4: Debugging Generated Code

**Problem**: Debugging native code is harder than VM code
**Solution**:

- DWARF debug info generation
- Source mapping from D-ISA to x86-64
- Integrated debugger support

## 📊 Success Metrics

### Performance Targets

- [ ] 10-50x speedup over VM execution for compute-heavy code
- [ ] Sub-second compilation time for small programs
- [ ] <5% overhead for I/O-heavy programs

### Compatibility Targets

- [ ] 100% compatibility with existing D-ISA programs
- [ ] All 63 implemented opcodes translated correctly (88 planned opcodes for future phases)
- [ ] Device I/O functions identically to VM

### Quality Targets

- [ ] Generated code passes all existing unit tests
- [ ] Memory-safe execution (no buffer overflows)
- [ ] Proper error handling and reporting

## 🚀 Immediate Next Steps

1. **Start with x86_encoder.cpp implementation**
2. **Create simple test program to verify instruction encoding**
3. **Implement basic register allocation**
4. **Write first D-ISA → x86-64 instruction translation**
5. **Test with simple ADD/MOV program**

## 💡 Future Enhancements

- Multiple architecture support (ARM64, RISC-V)
- Link-time optimization
- Profile-guided optimization
- LLVM IR backend as alternative
- Cross-compilation support
