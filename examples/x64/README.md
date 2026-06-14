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

| File | Expected Output |
|------|----------------|
| `simple_addition.asm` | `42 + 13 = 55` |
| `hello_world.asm` | `Hello, World!` |
| `simple_digit.asm` | `Number output: 5` |
| `stack_operations.asm` | `Stack test: PUSH 42,13,7 -> POP = 7,13,42 (LIFO)` |

### control_flow/

Branching and loop constructs:

| File | Expected Output |
|------|----------------|
| `counting_loop.asm` | `1 2 3 4 5` |
| `conditional_jumps.asm` | `Conditional jumps: JE, JNE, JG, JL all passed` |

### data/

Data storage and manipulation:

| File | Expected Output |
|------|----------------|
| `data_storage.asm` | `Hello, DemiEngine!` / `x64 64-bit mode` |
| `data_labels.asm` | `Hi!` / `Labels work!` |
| `indirect_addressing.asm` | `LOADR test: stored 42 at [200], loaded back = 42` |
| `string_reverse.asm` | `Reversed: !dlroW olleH` |
| `labels_and_strings.asm` | `Hello from labeled data!` |

### io/

Input/output operations:

| File | Expected Output |
|------|----------------|
| `char_output.asm` | `Character output: H` |
| `decimal_output.asm` | `Decimal output: 123` |

### features/

Feature demonstrations:

| File | Expected Output |
|------|----------------|
| `core_instructions.asm` | `Core instructions executed successfully` |

### interrupts/

Interrupt handling:

| File | Expected Output |
|------|----------------|
| `cli_sti.asm` | `Interrupt handler test passed (RBX set to 42 by handler)` |

### syscalls/

System call demonstrations:

| File | Expected Output |
|------|----------------|
| `hello_world.asm` | `Hello, World!` |
| `simple_write.asm` | Basic stdout write |
| `echo_input.asm` | `Enter your message: ` (interactive) |
| `calculator.asm` | `Enter first number: ` (interactive) |
| `line_calculator.asm` | `calc> ` (interactive) |
| `file_write.asm` | `File written successfully!` |
| `file_read.asm` | Reads back written file |
| `multiple_syscalls.asm` | `Using REAL Linux syscalls from DemiEngine VM!` |

### advanced/

Complex algorithms and computations:

| File | Expected Output |
|------|----------------|
| `fibonacci.asm` | `Fibonacci (10 terms): 0 1 1 2 3 5 8 13 21 34` |
| `factorial.asm` | `factorial(5) = 120` |
| `factorial_recursive.asm` | Prints 1! through 5! results |
| `readable_calculator.asm` | `5 + 3 = 8` |

### games/

Interactive programs:

| File | Expected Output |
|------|----------------|
| `number_guess.asm` | `Guess the hidden digit (0-9). You get 3 tries.` (interactive) |

## Running Examples

```bash
# Assemble and run in VM
./bin/demi-engine-debug -A examples/x64/basic/simple_addition.asm

# Compile to standalone executable
./bin/demi-engine-debug -A examples/x64/basic/simple_addition.asm -o my_program
./my_program

# With hexdump
./bin/demi-engine-debug --hexdump -A examples/x64/basic/simple_addition.asm
```

## Key Differences from x86

- Registers are 64-bit (R prefix) instead of 32-bit (E prefix)
- Larger address space (16 exabytes theoretical)
- Stack alignment is 8 bytes instead of 4 bytes
- Access to R8-R15 additional registers
- Can handle larger integers natively
