# Floating Point Unit (FPU) Reference

## Overview

DemiEngine includes a full-featured Floating Point Unit (FPU) based on the x87 architecture, supporting both 32-bit (float) and 64-bit (double) precision floating-point operations.

For an end-to-end, self-checking example that exercises every currently native-compiled
x87 opcode, see `examples/x86/features/fpu_showcase.asm`.

## FPU Register Stack

The FPU uses a stack-based register architecture:

- **ST(0)** through **ST(7)**: 8 floating-point registers organized as a stack
- **ST(0)**: Top of stack (TOS) - primary register for operations
- Operations typically work with ST(0) and either another stack register or memory

## Data Types

- **32-bit Float**: Single precision (float)
- **64-bit Double**: Double precision (double)
- **Integer Support**: Can load/store integers and convert to floating-point

## Instruction Set

### Arithmetic Operations

#### FADD - Floating Point Addition

```assembly
FADD <source>       ; ST(0) = ST(0) + source
```

**Operand Types**:

- **Memory Address**: Adds value at address to ST(0).
- **Immediate Value**: Adds immediate value to ST(0).

**Example**:

```assembly
FLD 0x100           ; Load float from memory
FADD 3.14           ; Add immediate 3.14
FADD 0x200          ; Add value at memory address 0x200
```

#### FSUB - Floating Point Subtraction

```assembly
FSUB <source>       ; ST(0) = ST(0) - source
```

**Example**:

```assembly
FLD 0x200
FSUB 0x204          ; Subtract float at memory address 0x204
FSUB 1.5            ; Subtract immediate 1.5
```

#### FMUL - Floating Point Multiplication

```assembly
FMUL <source>       ; ST(0) = ST(0) * source
```

**Example**:

```assembly
FLD 0x300
FMUL 0x308          ; Multiply by value at memory address 0x308
FMUL 2.0            ; Multiply by immediate 2.0
```

#### FDIV - Floating Point Division

```assembly
FDIV <source>       ; ST(0) = ST(0) / source
```

**Example**:

```assembly
FLD 0x400
FDIV 2.0            ; Divide by immediate 2.0
FDIV 0x404          ; Divide by value at memory address 0x404
```

### Transcendental Functions

#### FSIN - Sine

```assembly
FSIN                ; ST(0) = sin(ST(0))
```

**Example**:

```assembly
FLD 0x03, 1.5708    ; Load π/2
FSIN                ; Result = 1.0
```

#### FCOS - Cosine

```assembly
FCOS                ; ST(0) = cos(ST(0))
```

**Example**:

```assembly
FLD 0x03, 3.14159   ; Load π
FCOS                ; Result = -1.0
```

#### FTAN - Tangent

```assembly
FTAN                ; ST(0) = tan(ST(0))
```

**Note**:

- On x87 hardware, tangent is implemented via `FPTAN`, which also pushes an extra `1.0`
	onto the FPU stack. Demi's native codegen compensates for that so the observable VM
	behavior matches the showcase expectations.

#### FSQRT - Square Root

```assembly
FSQRT               ; ST(0) = sqrt(ST(0))
```

**Example**:

```assembly
FLD 0x03, 16.0
FSQRT               ; Result = 4.0
```

### Sign and Absolute Value

#### FABS - Absolute Value

```assembly
FABS                ; ST(0) = |ST(0)|
```

**Example**:

```assembly
FLD 0x03, -5.5
FABS                ; Result = 5.5
```

#### FCHS - Change Sign

```assembly
FCHS                ; ST(0) = -ST(0)
```

**Example**:

```assembly
FLD 0x03, 3.14
FCHS                ; Result = -3.14
```

### Load and Store Operations

#### FLD - Load Floating Point

```assembly
FLD [source]        ; Push source onto FPU stack
```

**Operand Types**:

- Memory (32-bit): `FLD 0x01 <address>`
- Memory (64-bit): `FLD 0x02 <address>`
- Immediate (64-bit): `FLD 0x03 <8-byte-value>`
- Stack register: `FLD 0x04 <ST(i)>`

**Examples**:

```assembly
FLD 0x01 0x1000     ; Load 32-bit float from 0x1000
FLD 0x02 0x2000     ; Load 64-bit double from 0x2000
FLD 0x03, 2.71828   ; Load immediate value (e)
FLD 0x04 0x02       ; Push copy of ST(2)
```

#### FST - Store Floating Point

```assembly
FST [dest]          ; Store ST(0) to destination (keep on stack)
```

**Operand Types**:

- Memory (32-bit): `FST 0x01 <address>`
- Memory (64-bit): `FST 0x02 <address>`
- Stack register: `FST 0x04 <ST(i)>`

**Examples**:

```assembly
FST 0x01 0x3000     ; Store as 32-bit float to 0x3000
FST 0x02 0x4000     ; Store as 64-bit double to 0x4000
FST 0x04 0x01       ; Copy ST(0) to ST(1)
```

#### FSTP - Store and Pop

```assembly
FSTP [dest]         ; Store ST(0) to destination and pop
```

**Example**:

```assembly
FSTP 0x02 0x5000    ; Store double and remove from stack
```

### Integer Operations

#### FILD - Integer Load

```assembly
FILD [source]       ; Load integer and convert to float
```

**Operand Types**:

- Memory (16-bit): `FILD 0x01 <address>`
- Memory (32-bit): `FILD 0x02 <address>`
- Memory (64-bit): `FILD 0x03 <address>`

**Example**:

```assembly
FILD 0x02 0x6000    ; Load 32-bit integer, convert to float
```

#### FIST - Integer Store

```assembly
FIST [dest]         ; Convert ST(0) to integer and store
```

**Operand Types**:

- Memory (16-bit): `FIST 0x01 <address>`
- Memory (32-bit): `FIST 0x02 <address>`

**Example**:

```assembly
FIST 0x02 0x7000    ; Round to 32-bit integer and store
```

#### FISTP - Integer Store and Pop

```assembly
FISTP [dest]        ; Convert, store, and pop
```

**Operand Types**:

- Memory (16-bit): `FISTP 0x01 <address>`
- Memory (32-bit): `FISTP 0x02 <address>`
- Memory (64-bit): `FISTP 0x03 <address>`

### Comparison Operations

#### FCOMPP - Compare and Pop Twice

```assembly
FCOMPP              ; Compare ST(0) with ST(1), pop both
```

**Sets FPU status flags based on comparison**:

- ST(0) > ST(1): C3=0, C2=0, C0=0
- ST(0) < ST(1): C3=0, C2=0, C0=1
- ST(0) = ST(1): C3=1, C2=0, C0=0

#### FUCOMPP - Unordered Compare and Pop Twice

```assembly
FUCOMPP             ; Unordered compare ST(0) with ST(1), pop both
```

Similar to FCOMPP but handles NaN differently.

### Control and Status

#### FINIT - Initialize FPU

```assembly
FINIT               ; Reset FPU to initial state
```

Resets:

- Control word to default
- Status word to 0
- Tag word to empty
- Clears exceptions

#### FCLEX - Clear Exceptions

```assembly
FCLEX               ; Clear exception flags
```

#### FLDCW - Load Control Word

```assembly
FLDCW <address>     ; Load FPU control word from memory
```

**Example**:

```assembly
FLDCW 0x8000        ; Load control word from address
```

#### FSTCW - Store Control Word

```assembly
FSTCW <address>     ; Store FPU control word to memory
```

**Example**:

```assembly
FSTCW 0x8004        ; Store control word to address
```

#### FSTSW - Store Status Word

```assembly
FSTSW <dest>        ; Store FPU status word
```

**Destinations**:

- Memory: `FSTSW 0x01 <address>`
- AX register: `FSTSW 0x02`

**Example**:

```assembly
FSTSW 0x01 0x9000   ; Store status to memory
FSTSW 0x02          ; Store status to AX (R0)
```

## Complete FPU Opcode List

**Note**: The FPU opcodes are currently mapped to the 0xA0-0xB6 range in the implementation.

| Opcode | Mnemonic | Description                     |
| ------ | -------- | ------------------------------- |
| 0xA0   | FLD      | Load floating point value       |
| 0xA1   | FST      | Store floating point value      |
| 0xA2   | FSTP     | Store and pop                   |
| 0xA3   | FILD     | Integer load                    |
| 0xA4   | FIST     | Integer store                   |
| 0xA5   | FISTP    | Integer store and pop           |
| 0xA6   | FADD     | Floating point addition         |
| 0xA7   | FSUB     | Floating point subtraction      |
| 0xA8   | FMUL     | Floating point multiplication   |
| 0xA9   | FDIV     | Floating point division         |
| 0xAA   | FSIN     | Sine                            |
| 0xAB   | FCOS     | Cosine                          |
| 0xAC   | FTAN     | Tangent                         |
| 0xAD   | FSQRT    | Square root                     |
| 0xAE   | FABS     | Absolute value                  |
| 0xAF   | FCHS     | Change sign                     |
| 0xB0   | FINIT    | Initialize FPU                  |
| 0xB1   | FCLEX    | Clear exceptions                |
| 0xB2   | FSTCW    | Store control word              |
| 0xB3   | FLDCW    | Load control word               |
| 0xB4   | FSTSW    | Store status word               |
| 0xB5   | FCOMPP   | Compare and pop twice           |
| 0xB6   | FUCOMPP  | Unordered compare and pop twice |

## Programming Examples

### Basic Arithmetic

```assembly
; Calculate (a + b) * c
FLD 0x02 0x1000     ; Load a (double)
FADD 0x02 0x1008    ; Add b
FMUL 0x02 0x1010    ; Multiply by c
FSTP 0x02 0x1018    ; Store result
```

### Temperature Conversion (Celsius to Fahrenheit)

```assembly
; F = C * 9/5 + 32
FLD 0x02 0x2000     ; Load Celsius
FMUL 0x03, 1.8      ; Multiply by 9/5 (1.8)
FADD 0x03, 32.0     ; Add 32
FSTP 0x02 0x2008    ; Store Fahrenheit
```

### Circle Area Calculation

```assembly
; Area = π * r²
FLD 0x02 0x3000     ; Load radius
FMUL 0x04 0x00      ; Square it (ST(0) * ST(0))
FLD 0x03, 3.14159   ; Load π
FMUL                ; Multiply
FSTP 0x02 0x3008    ; Store area
```

### Distance Formula

```assembly
; distance = sqrt((x2-x1)² + (y2-y1)²)
FLD 0x02 0x4000     ; Load x2
FSUB 0x02 0x4008    ; Subtract x1
FMUL 0x04 0x00      ; Square (dx²)
FLD 0x02 0x4010     ; Load y2
FSUB 0x02 0x4018    ; Subtract y1
FMUL 0x04 0x00      ; Square (dy²)
FADD                ; Add dx² + dy²
FSQRT               ; Take square root
FSTP 0x02 0x4020    ; Store distance
```

## FPU Status Word

**Bit Layout**:

```
15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
 B C3 ST ST ST C2 C1 C0 ES SF PE UE OE ZE DE IE
```

**Flags**:

- **B**: Busy flag
- **C3, C2, C1, C0**: Condition codes
- **ST**: Stack top pointer (3 bits)
- **ES**: Exception summary
- **SF**: Stack fault
- **PE**: Precision exception
- **UE**: Underflow exception
- **OE**: Overflow exception
- **ZE**: Divide-by-zero exception
- **DE**: Denormalized operand exception
- **IE**: Invalid operation exception

## Best Practices

1. **Always initialize**: Use `FINIT` at program start
2. **Check for errors**: Read status word with `FSTSW` after operations
3. **Manage stack**: Balance pushes/pops to avoid stack overflow/underflow
4. **Use appropriate precision**: 32-bit for speed, 64-bit for accuracy
5. **Handle exceptions**: Check and clear exception flags as needed

## Test Coverage

DemiEngine includes comprehensive FPU testing:

- 10 FPU tests in `tests/fpu.test.asm`
- All arithmetic operations tested
- Transcendental functions verified
- Load/store operations validated
- Integer conversion tested
- 100% FPU instruction coverage

See `tests/fpu.test.asm` for complete examples and test cases.
