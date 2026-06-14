# x86 32-bit Examples

This directory contains assembly examples targeting **x86 32-bit architecture**.
All examples are in the `ground-up/` subdirectory, ported from the book
"Programming from the Ground Up" by Jonathan Bartlett.

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

## Examples (ground-up/)

| File | What It Does | Expected Output |
|------|-------------|-----------------|
| `exit.asm` | Minimal exit with code 0 | `echo $?` → `0` |
| `hello.asm` | Write "Hello, World!\n" to stdout | `Hello, World!` |
| `hello-macro.asm` | Same as hello, using macros | `Hello, World!` |
| `maximum.asm` | Find max in array `[3,67,34,222,...]` | `echo $?` → `222` |
| `power.asm` | Recursive 2^5 computation | `echo $?` → `32` |
| `factorial.asm` | Recursive 5! computation | `echo $?` → `120` |
| `toupper.asm` | Filter stdin: lowercase → uppercase | `echo "hello" \| ./toupper` → `HELLO` |
| `toupper-macro.asm` | Same as toupper, using macros | `echo "hello" \| ./toupper-macro` → `HELLO` |
| `write-records.asm` | Write 3 structured records to file | Creates `records.dat` (252 bytes) |
| `read-records.asm` | Read records.dat, print names+ages | `Fredrick - age 34` etc. |
| `macros.inc` | Shared macro definitions | (included by other files) |

## Assembling

All x86 examples must be assembled with the `--assembly-target x86-elf32` flag:

```bash
# Assemble to ELF32 object
./bin/demi-engine-debug --assembly-target x86-elf32 -A examples/x86/ground-up/hello.asm

# Assemble and run
./bin/demi-engine-debug --assembly-target x86-elf32 examples/x86/ground-up/hello.asm

# To produce a standalone executable (ELF32):
./bin/demi-engine-debug --assembly-target x86-elf32 -o hello examples/x86/ground-up/hello.asm
```

## Running the Executables

```bash
# Programs that use sys_write print directly to stdout:
./examples/bin/x86/basic/hello_world

# Programs that return a value via exit, check with:
./examples/bin/x86/basic/simple_addition; echo $?

# Interactive/filter programs:
echo "hello world" | ./examples/bin/x86/syscalls/echo_input
```

## Key Differences from x64

- Registers are 32-bit (E prefix) instead of 64-bit (R prefix)
- Smaller address space (4GB max)
- Stack alignment is 4 bytes instead of 8 bytes
- Cannot use R8-R15 registers (x64 only)
- Syscalls use `INT 0x80` with EAX=syscall number
