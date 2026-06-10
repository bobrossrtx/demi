# DemiEngine Future Features Roadmap

## Overview

This document outlines future features and enhancements for DemiEngine, prioritized based on test coverage, user needs, and architectural importance.

## Phase 1: Core CPU Enhancements (High Priority)

### 1.1 Signed Comparison Operations

**Status**: Not Implemented  
**Priority**: HIGH  
**Effort**: Medium

**Description**: Add signed integer comparison support with proper jump instructions.

**New Instructions**:

- `JG` (0xXX) - Jump if Greater (signed)
- `JL` (0xXX) - Jump if Less (signed)
- `JGE` (0xXX) - Jump if Greater or Equal (signed)
- `JLE` (0xXX) - Jump if Less or Equal (signed)

**Benefits**:

- Proper signed arithmetic support
- Better algorithm implementations
- Complete comparison instruction set

**Tests Using This**: `conditional_chain`, `prime_check`, `vector_max_future`

---

### 1.2 Memory Protection and Bounds Checking

**Status**: Partial  
**Priority**: HIGH  
**Effort**: Medium

**Description**: Enhance memory protection with proper bounds checking and error reporting.

**Features**:

- Runtime bounds checking for all memory operations
- Configurable memory protection levels
- Detailed error messages for violations
- Memory region permissions (read/write/execute)

**Benefits**:

- Safer program execution
- Better debugging information
- Prevention of memory corruption

**Tests Using This**: `memory_boundary_access`, `memory_access_violation`

---

### 1.3 Enhanced Stack Operations

**Status**: Partial  
**Priority**: MEDIUM  
**Effort**: Low

**Description**: Add bulk stack operations for efficient register save/restore.

**New Instructions**:

- `PUSHALL` (0xXX) - Push all registers to stack
- `POPALL` (0xXX) - Pop all registers from stack
- `PUSHA` (0xXX) - Push accumulator registers
- `POPA` (0xXX) - Pop accumulator registers

**Benefits**:

- Faster context switching
- Easier exception handler implementation
- Better function prologue/epilogue support

**Tests Using This**: `exception_register_preservation`

---

## Phase 2: Exception and Interrupt System (High Priority)

### 2.1 Exception Handling Framework

**Status**: Not Implemented  
**Priority**: HIGH  
**Effort**: High

**Description**: Complete exception handling system with handlers and recovery.

**New Instructions**:

- `SETEXH <addr>` (0xXX) - Set exception handler address
- `RETEX` (0xXX) - Return from exception
- `GETEXINFO` (0xXX) - Get exception information

**Exception Types**:

- Division by zero
- Stack overflow/underflow
- Invalid opcode
- Memory access violation
- Arithmetic overflow (configurable)

**Benefits**:

- Robust error handling
- Graceful program recovery
- Better debugging capabilities

**Tests Using This**: All tests in `test_exceptions_future.asm`

---

### 2.2 Software Interrupt System

**Status**: Not Implemented  
**Priority**: MEDIUM  
**Effort**: Medium

**Description**: Software interrupt mechanism for system calls and IPC.

**New Instructions**:

- `INT <vector>` (0xXX) - Trigger software interrupt
- `IRET` (0xXX) - Return from interrupt
- `SETINT <vector>, <handler>` (0xXX) - Set interrupt handler
- `CLI` (0xXX) - Clear interrupt flag (disable)
- `STI` (0xXX) - Set interrupt flag (enable)

**Features**:

- 256 interrupt vectors
- Interrupt priority levels
- Nested interrupt support
- Interrupt masking

**Benefits**:

- System call interface
- Inter-process communication
- Event-driven programming

**Tests Using This**: `interrupt_simulation`, `nested_exception_handling`

---

### 2.3 Hardware Interrupts

**Status**: Not Implemented  
**Priority**: LOW  
**Effort**: High

**Description**: Hardware interrupt support for I/O devices and timers.

**Features**:

- Timer interrupts (configurable intervals)
- Device interrupts (keyboard, disk, network)
- Interrupt controller (PIC/APIC-like)
- Interrupt prioritization

**Benefits**:

- Real-time responsiveness
- Device driver support
- Preemptive multitasking foundation

**Tests Using This**: `timer_interrupt_future`

---

## Phase 3: SIMD and Parallel Operations (Medium Priority)

### 3.1 Basic Vector Operations

**Status**: Not Implemented  
**Priority**: MEDIUM  
**Effort**: High

**Description**: SIMD instructions for parallel operations on vectors.

**New Instructions**:

- `VADD <dest>, <src1>, <src2>` - Vector addition
- `VSUB <dest>, <src1>, <src2>` - Vector subtraction
- `VMUL <dest>, <src1>, <src2>` - Vector multiplication
- `VDIV <dest>, <src1>, <src2>` - Vector division

**Vector Sizes**: 2, 4, 8, 16 elements (configurable)

**Benefits**:

- 2-8x performance for parallel workloads
- Better data processing capabilities
- Modern CPU features

**Tests Using This**: `vector_addition_future`, `vector_multiplication_future`

---

### 3.2 Advanced Vector Operations

**Status**: Not Implemented  
**Priority**: LOW  
**Effort**: High

**Description**: Advanced SIMD operations for complex computations.

**New Instructions**:

- `VDOT <dest>, <src1>, <src2>` - Dot product
- `VCROSS <dest>, <src1>, <src2>` - Cross product (3D)
- `VMAX <dest>, <src>` - Find maximum element
- `VMIN <dest>, <src>` - Find minimum element
- `VSUM <dest>, <src>` - Sum all elements
- `VBROADCAST <dest>, <scalar>` - Broadcast scalar to vector
- `VCMPGT <dest>, <src1>, <src2>` - Compare greater than

**Benefits**:

- Efficient mathematical operations
- Graphics/physics computations
- Signal processing

**Tests Using This**: `vector_dot_product_future`, `vector_max_future`, `vector_broadcast_future`

---

### 3.3 Packed Data Operations

**Status**: Not Implemented  
**Priority**: LOW  
**Effort**: Medium

**Description**: Operations on packed byte/word data in registers.

**New Instructions**:

- `PACKB <dest>, <src1>, <src2>` - Pack bytes
- `UNPACKB <dest>, <src>` - Unpack bytes
- `PACKW <dest>, <src1>, <src2>` - Pack words
- `UNPACKW <dest>, <src>` - Unpack words

**Benefits**:

- Efficient data representation
- Better memory utilization
- SIMD-style operations on sub-register data

**Tests Using This**: `packed_byte_operations_future`

---

## Phase 4: Performance and Debugging (Medium Priority)

### 4.1 Performance Counters

**Status**: Not Implemented  
**Priority**: MEDIUM  
**Effort**: Medium

**Description**: Hardware performance counters for profiling.

**Features**:

- Cycle counter
- Instruction counter
- Cache hit/miss counters (if caching implemented)
- Branch prediction statistics
- Memory access counters

**New Instructions**:

- `RDTSC <dest>` - Read timestamp counter
- `RDPMC <counter>, <dest>` - Read performance counter

**Benefits**:

- Performance profiling
- Optimization guidance
- Benchmarking support

**Tests Using This**: All tests in `test_benchmarks.asm`

---

### 4.2 Advanced Debugging Support

**Status**: Basic  
**Priority**: MEDIUM  
**Effort**: Medium

**Description**: Enhanced debugging features for development.

**Features**:

- Breakpoint support (`INT 3` equivalent)
- Single-step mode
- Register/memory watch points
- Call stack tracing
- Debug symbol support

**New Instructions**:

- `BREAK` (0xXX) - Trigger breakpoint
- `TRACE` (0xXX) - Enable single-step mode

**Benefits**:

- Easier program debugging
- Better error diagnosis
- Development tool support

---

### 4.3 Memory Management Unit (MMU)

**Status**: Not Implemented  
**Priority**: LOW  
**Effort**: Very High

**Description**: Virtual memory support with page tables.

**Features**:

- Virtual address translation
- Page tables (4KB pages)
- Memory protection (read/write/execute)
- Demand paging
- Memory mapped I/O

**New Instructions**:

- `LOADCR3 <page_table>` - Load page table base
- `INVLPG <addr>` - Invalidate TLB entry

**Benefits**:

- Process isolation
- Virtual memory support
- Better memory protection
- Foundation for OS development

---

## Phase 5: Advanced Features (Low Priority)

### 5.1 Floating Point Enhancements

**Status**: Complete (Basic)  
**Priority**: LOW  
**Effort**: Medium

**Description**: Additional FPU features beyond current implementation.

**Potential Additions**:

- `FPREM` - Partial remainder
- `FRNDINT` - Round to integer
- `FSCALE` - Scale by power of 2
- `FYL2X` - Y \* log2(X)
- `FPTAN` - Partial tangent
- `FPATAN` - Partial arctangent

**Benefits**:

- Complete FPU instruction set
- Better IEEE 754 compliance
- Advanced mathematical functions

---

### 5.2 Cryptographic Instructions

**Status**: Not Implemented  
**Priority**: LOW  
**Effort**: High

**Description**: Hardware-accelerated cryptographic operations.

**Potential Instructions**:

- `AES` - AES encryption/decryption
- `SHA256` - SHA-256 hash computation
- `RDRAND` - Hardware random number generation

**Benefits**:

- Secure computation
- Performance for crypto workloads
- Modern CPU feature parity

---

### 5.3 Multithreading Support

**Status**: Not Implemented  
**Priority**: MEDIUM (bumped from LOW — foundation for Demi high-level language)  
**Effort**: High

**Description**: Shared-memory multithreading with 1:1 OS thread mapping and atomic synchronization.

**Design doc**: `docs/development/FUTURE_OPCODES.md#phase-6-multithreading-support---medium-priority`

**Features**:

- Multiple threads sharing the VM's flat 64KB memory space
- 1:1 thread mapping via Linux `SYS_CLONE` (or `pthread_create` for portability)
- Per-thread register file (134 registers each — no shared register state)
- Hardware atomics via `LOCK` prefix (ADD, SUB, XCHG, CMPXCHG)
- Memory ordering with `MFENCE`/`LFENCE`/`SFENCE`
- Futex-based `THREAD_JOIN` for waiting on thread completion
- Sandbox-gated: thread creation requires `--allow-exec` capability flag

**New Instructions (10 opcodes)**:

| Opcode | Purpose |
|---|---|
| `THREAD_SPAWN` | Create thread at entry point |
| `THREAD_JOIN` | Wait for thread to exit |
| `THREAD_EXIT` | Terminate calling thread |
| `LOCK ADD` | Atomic add to memory |
| `LOCK SUB` | Atomic subtract from memory |
| `LOCK XCHG` | Atomic exchange |
| `CMPXCHG` | Compare and exchange |
| `MFENCE` / `LFENCE` / `SFENCE` | Memory barriers |
| `THREAD_ID` | Get current thread ID |

**New Syscalls**: `SYS_CLONE` (56), `SYS_FUTEX` (202), `SYS_SET_TID_ADDRESS` (218)

**x86-64 Translation**: Each threading opcode maps directly to native instructions:
- `THREAD_SPAWN` → `syscall` with `SYS_clone` + `CLONE_VM|CLONE_THREAD|...` flags (0x3D0F00)
- `LOCK ADD` → `lock add` (F0 prefix + ADD opcode)
- `CMPXCHG` → `cmpxchg` (or `lock cmpxchg` for atomic variant)
- `MFENCE` → `mfence` (0F AE F0)
- `THREAD_EXIT` → `syscall` with `SYS_exit` (already wired, already translates)

See `docs/development/FUTURE_OPCODES.md#phase-6` for full translation table, thread lifecycle diagram, and example DASM code.

**Benefits**:

- Parallel programming within Demi programs
- Foundation for Demi language `spawn`/`sync` primitives
- Zero-overhead native threading when compiled with `-o`
- Safe: sandbox prevents guest code from DoS via thread bombs (threads inherit VFS jail)

**Implementation dependencies**:
- `elf_emitter.cpp` needs `PT_TLS` segment support for CLONE_SETTLS
- Interpreter needs thread-safe memory access (mutex or lock-free) for the shared `memory[]` array
- `SYS_CLONE` and `SYS_FUTEX` must be wired through the INT 0x80 gateway

---

## Implementation Priority Matrix

| Feature              | Priority | Effort    | Impact | Phase |
| -------------------- | -------- | --------- | ------ | ----- |
| Signed Comparisons   | HIGH     | Medium    | High   | 1     |
| Exception Handling   | HIGH     | High      | High   | 2     |
| Memory Protection    | HIGH     | Medium    | High   | 1     |
| Software Interrupts  | MEDIUM   | Medium    | Medium | 2     |
| Stack Operations     | MEDIUM   | Low       | Medium | 1     |
| Basic SIMD           | MEDIUM   | High      | Medium | 3     |
| Performance Counters | MEDIUM   | Medium    | Medium | 4     |
| Debugging Support    | MEDIUM   | Medium    | High   | 4     |
| Hardware Interrupts  | LOW      | High      | Low    | 2     |
| Advanced SIMD        | LOW      | High      | Low    | 3     |
| Packed Data          | LOW      | Medium    | Low    | 3     |
| MMU                  | LOW      | Very High | Low    | 4     |
| FPU Enhancements     | LOW      | Medium    | Low    | 5     |
| Cryptographic        | LOW      | High      | Low    | 5     |
| Multi-core           | MEDIUM   | High      | High   | 5     |

---

## Test Coverage by Feature

- **Signed Comparisons**: 3 tests waiting
- **Exception Handling**: 8 tests (skeleton implementations)
- **Memory Protection**: 2 tests
- **Interrupts**: 3 tests (skeleton implementations)
- **SIMD Operations**: 7 tests (simulated)
- **Performance/Benchmarks**: 8 tests (5 working, 3 need fixes)

---

## Next Steps

1. **Fix Existing Test Failures** (6 tests with logic errors)
2. **Implement Signed Comparisons** (JG, JL, JGE, JLE)
3. **Design Exception Handling Framework**
4. **Implement Enhanced Stack Operations** (PUSHALL, POPALL)
5. **Begin Software Interrupt System Design**

---

## Contributing

When implementing new features:

1. Start with test cases that define expected behavior
2. Update this roadmap with implementation status
3. Document new instructions in API_REFERENCE.md
4. Add examples to the examples/ directory
5. Update TEST_SUITE_SUMMARY.md

---

**Last Updated**: June 9, 2026  
**Next Review**: After Phase 2 completion
