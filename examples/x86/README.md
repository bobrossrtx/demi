# x86 32-bit Examples

This directory contains assembly examples targeting **x86 32-bit architecture**.

## Register Usage

x86 32-bit uses the following general-purpose registers:

- **EAX** - Accumulator register (32-bit)
- **EBX** - Base register (32-bit)
- **ECX** - Counter register (32-bit)
- **EDX** - Data register (32-bit)
- **ESI** - Source index (32-bit)
- **EDI** - Destination index (32-bit)
- **ESP** - Stack pointer (32-bit)
- **EBP** - Base pointer (32-bit)

## Directory Structure

### basic/

Fundamental assembly operations and concepts:

- **simple_addition.asm** - Basic arithmetic operations
- **hello_world.asm** - String output using DB directive
- **simple_digit.asm** - Number to ASCII conversion and output
- **stack_operations.asm** - PUSH and POP demonstrations

### control_flow/

Branching and loop constructs:

- **counting_loop.asm** - Loop demonstration with output
- **conditional_jumps.asm** - Conditional branching (JE, JNE, JG, JL)

### data/

Data storage and manipulation:

- **data_storage.asm** - DB directive and string operations
- **data_labels.asm** - Multiple labeled data blocks
- **indirect_addressing.asm** - LOADR instruction demonstration
- **string_reverse.asm** - In-place string reversal algorithm

### io/

Input/output operations:

- **char_output.asm** - Single character output test
- **decimal_output.asm** - Number to decimal string conversion

### features/

Feature demonstrations:

- **core_instructions.asm** - All basic instruction demonstrations
- **fpu_showcase.asm** - Narrated x87 FPU showcase with self-checking verification

### interrupts/

Interrupt handling:

- **cli_sti.asm** - Interrupt enable/disable and handlers

### syscalls/

System call demonstrations:

- **hello_world.asm** - sys_write syscall example
- **simple_write.asm** - Basic stdout write test

### advanced/

Complex algorithms and computations:

- **fibonacci.asm** - Fibonacci sequence generator
- **factorial.asm** - Iterative factorial calculation
- **factorial_recursive.asm** - Recursive factorial with stack frames

## Running Examples

```bash
# Assemble only
./bin/demi-engine-debug -A examples/x86/basic/simple_addition.asm

# Assemble and run
./bin/demi-engine-debug examples/x86/basic/simple_addition.asm

# With hexdump
./bin/demi-engine-debug --hexdump -A examples/x86/basic/simple_addition.asm
```

## DASM Coding Standards

All examples follow DASM coding standards:

- Entry point labeled as `_start`
- Uppercase instructions (LOAD_IMM, MOV, ADD, etc.)
- Clear comments explaining each section
- Proper use of `.org` directives for memory layout
- Consistent indentation (4 spaces)

## Key Differences from x64

- Registers are 32-bit (E prefix) instead of 64-bit (R prefix)
- Smaller address space (4GB max)
- Stack alignment is 4 bytes instead of 8 bytes
- Cannot use R8-R15 registers (x64 only)

## Notes

All examples in this directory use 32-bit register conventions and are designed to demonstrate core assembly concepts in x86 architecture.
