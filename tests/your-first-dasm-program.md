# Your First DASM Program

Let's write a program.

If you've never touched assembly language before, don't worry. DASM is a lot simpler than it looks. In the next few minutes you'll write a program, run it, understand what every line does, and then make it do something a little more interesting.

DASM is the assembly language used by DemiEngine's virtual machine. Instead of compiling directly to your CPU, your program runs inside DemiEngine. That makes it a great place to learn how registers, instructions, memory, and control flow actually work.

## Before We Start

You'll need a working copy of DemiEngine.

Clone the repository and build the project by following the instructions in the main README:

`../../README.md`

Once you've built the project successfully, you're ready to write some assembly.

---

# Hello, World

Create a file called:

```text
hello.asm
```

Paste this into it:

```asm
; Hello World in DASM

LOAD_IMM R0, 72
OUT R0, 1

LOAD_IMM R0, 101
OUT R0, 1

LOAD_IMM R0, 108
OUT R0, 1

LOAD_IMM R0, 108
OUT R0, 1

LOAD_IMM R0, 111
OUT R0, 1

HALT
```

When you run it, you should see:

```text
Hello
```

Not very exciting yet, but let's see what's happening.

---

# Understanding Each Line

The first instruction is:

```asm
LOAD_IMM R0, 72
```

`LOAD_IMM` means "load an immediate value."

An immediate value is simply a number written directly into the instruction.

This instruction places the value `72` into register `R0`.

Think of a register as a tiny storage box inside the virtual CPU.

After that we have:

```asm
OUT R0, 1
```

`OUT` sends data to a device.

Device `1` is the console.

The value currently stored in `R0` is sent to the console output.

The number `72` corresponds to the ASCII character:

```text
H
```

The rest of the program repeats the same pattern for:

```text
101 -> e
108 -> l
108 -> l
111 -> o
```

Finally:

```asm
HALT
```

stops program execution.

Without it, the CPU would continue trying to execute instructions.

---

# Registers in DASM

You'll see registers everywhere in assembly programs.

Examples:

```asm
R0
R1
R2
R3
```

Registers are where your program stores values while it runs.

For example:

```asm
LOAD_IMM R0, 5
LOAD_IMM R1, 3
ADD R0, R1
```

After these instructions:

```text
R0 = 8
R1 = 3
```

The result is stored back into `R0`.

---

# Let's Do Some Math

Create another file:

```text
addition.asm
```

```asm
; 5 + 3

LOAD_IMM R0, 5
LOAD_IMM R1, 3

ADD R0, R1

LOAD_IMM R2, 48
ADD R0, R2

OUT R0, 1

HALT
```

Output:

```text
8
```

Why add 48?

Computers store characters as numbers.

ASCII code:

```text
'0' = 48
'1' = 49
'2' = 50
...
'8' = 56
```

So after calculating:

```text
5 + 3 = 8
```

we add 48 to convert the number into a printable character.

---

# Let's Build a Loop

Now for something more interesting.

Create:

```text
count.asm
```

```asm
LOAD_IMM R0, 1
LOAD_IMM R1, 5

loop:

LOAD_IMM R2, 48
MOV R3, R0
ADD R3, R2

OUT R3, 1

CMP R0, R1
JZ done

LOAD_IMM R2, 1
ADD R0, R2

JMP loop

done:
HALT
```

Output:

```text
12345
```

This introduces some important ideas.

`CMP` compares two values.

```asm
CMP R0, R1
```

`JZ` means "jump if zero."

If the comparison indicates the values are equal, execution jumps to:

```asm
done:
```

Otherwise execution continues.

`JMP` performs an unconditional jump back to the start of the loop.

This is how loops are built in assembly language.

---

# Running Your Program

Execute a program with:

```bash
./bin/demi-engine -A hello.asm
```

Or:

```bash
./bin/demi-engine -A count.asm
```

You can also explore the examples included with the project:

```bash
./bin/demi-engine -A examples/hello_world.asm
```

```bash
./bin/demi-engine -A examples/counting_loop.asm
```

The examples directory contains working programs that demonstrate arithmetic, memory operations, loops, jumps, and more.

---

# Running Tests

DemiEngine includes a comprehensive test suite.

Run all tests:

```bash
./bin/demi-engine -at
```

Run a specific test file:

```bash
./bin/demi-engine -at tests_new/arithmetic.test.asm
```

Show detailed output:

```bash
./bin/demi-engine -at tests_new/control_flow.test.asm --show=all
```

Reading test files is one of the fastest ways to learn new instructions.

---

# Where To Go Next

Now that you've written a program, used registers, printed output, performed arithmetic, and built a loop, you're ready for the rest of the instruction set.

Good next stops:

* `examples/hello_world.asm`
* `examples/readable_calculator.asm`
* `examples/counting_loop.asm`
* `docs/reference/QUICK_REFERENCE.md`
* `docs/usage/README.md`

The best way to learn assembly is to experiment. Change a value, run the program, see what happens, and keep building.
