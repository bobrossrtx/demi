# x64 64-bit Examples

This directory contains assembly examples targeting **x64 64-bit architecture**.

## Register Usage

x64 64-bit uses the following general-purpose registers:

- **RAX** - Accumulator register (64-bit)
- **RBX** - Base register (64-bit)
- **RCX** - Counter register (64-bit)
- **RDX** - Data register (64-bit)
- **RSI** - Source index (64-bit)
- **RDI** - Destination index (64-bit)
- **RSP** - Stack pointer (64-bit)
- **RBP** - Base pointer (64-bit)
- **R8-R15** - Additional general-purpose registers (64-bit, x64 only)

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
- **factorial.asm** - Iterative factorial calculation (supports larger values)
- **factorial_recursive.asm** - Recursive factorial with stack frames

### games/

Small interactive programs:

- **number_guess.asm** - Three-try number guessing game using stdin/stdout syscalls

## Running Examples

```bash
# Assemble only
./bin/demi-engine-debug -A examples/x64/basic/simple_addition.asm

# Assemble and run
./bin/demi-engine-debug examples/x64/basic/simple_addition.asm

# With hexdump
./bin/demi-engine-debug --hexdump -A examples/x64/basic/simple_addition.asm
```

## DASM Coding Standards

All examples follow DASM coding standards:

- Entry point labeled as `_start`
- Uppercase instructions (LOAD_IMM, MOV, ADD, etc.)
- Clear comments explaining each section
- Proper use of `.org` directives for memory layout
- Consistent indentation (4 spaces)

## Key Differences from x86

- Registers are 64-bit (R prefix) instead of 32-bit (E prefix)
- Larger address space (16 exabytes theoretical)
- Stack alignment is 8 bytes instead of 4 bytes
- Access to R8-R15 additional registers
- Can handle larger integers natively (e.g., factorial of 20!)

## Notes

All examples in this directory use 64-bit register conventions and demonstrate the advantages of x64 architecture for larger computations and memory addressing.
