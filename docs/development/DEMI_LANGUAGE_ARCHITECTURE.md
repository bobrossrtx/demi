# Demi Language Frontend — Architectural Proposal

**Date:** 2026-07-09  
**Status:** Proposal / Design Phase — decisions locked

---

## 0. Base Language Decisions (LOCKED)

| # | Decision | Rationale |
|---|----------|-----------|
| 1 | **C-like syntax** | Statements end with `;`, explicit `return`, braces for blocks, `if/while/for` are statements not expressions |
| 2 | **Static typing with inference** | `let x = 42` infers `i32`; `let x: i64 = 42` overrides. No dynamic typing. |
| 3 | **RAII + ownership (eventual)** | Stage 1: manual `alloc`/`free`. Stage 3+: destructors, move semantics, optional borrow checker |
| 4 | **Filesystem-based modules with namespaces** | `import "math/vec.dem"` → `vec.add(x,y)`. `pub`/private. Hierarchical namespaces map to directory tree. |
| 5 | **Inline asm + intrinsics + register access** | `asm { ... }` blocks map to DASM. `@cpuid()` for CPU intrinsics. `register eax = 42` for direct register access. |
| 6 | **Explicit entry point** | `fn main() -> i32 { return 0; }` |
| 7 | **File extension** | `.dem` |
| 8 | **Null safety** | `Option<T>` — no null pointers. `T?` sugar for `Option<T>`. |

---

## 1. Vision (from roadmap.md)

A fully customizable C-like systems language where **syntax, semantics, type system, memory management, execution model, and standard library** are configurable per project via `demi.toml`. Ultimate goal: you don't adapt to the language — the language adapts to your project.

---

## 2. Existing Infrastructure to Leverage

The Demi toolchain already has a mature backend pipeline:

```
Source → Lexer → Parser → AST → IRProgram → Backend → Output
                                              ├── Demi Bytecode → Demi VM
                                              ├── x86-elf64 → .so/.o/executable
                                              └── DISAToX86Compiler → native x86-64
```

Key assets:
- **Demi VM**: 134 registers (R0-R133), FPU (23 ops), SIMD (8 ops), I/O subsystem, syscall dispatch
- **IRProgram** (`ir.hpp`): sections, relocations, symbols, data records — target for codegen
- **LoweringContext** (`lowering.hpp`): AST → IR translation (used by DASM)
- **DISAToX86Compiler**: VM bytecode → native x86-64 JIT/AOT compilation
- **ELF Emitter**: `.so`, `.o`, executable generation — `ET_DYN`/`ET_EXEC`/`ET_REL`
- **Testing framework**: `TestContext` with assembler integration

The Demi language frontend targets **IRProgram** — reusing the entire existing backend unchanged.

---

## 3. Language Specification (Stage 1)

### 3.1 Hello World

```dem
// hello.dem
import "console";

fn main() -> i32 {
    console.println("Hello, Demi!");
    return 0;
}
```

### 3.2 Types

```
Primitives:   i8, i16, i32, i64, u8, u16, u32, u64, f32, f64, bool, void
Pointers:     *T        (raw pointer to T)
Option:       T?        (sugar for Option<T>, no null)
Arrays:       T[N]      (fixed-size, N must be compile-time constant)
Slices:       T[]       (ptr + length, fat pointer)
Strings:      string    (alias for u8[], UTF-8 encoded)
```

### 3.3 Variables

```dem
let x: i32 = 42;        // explicit type
let y = 42;             // type inference → i32
let z: i64 = 42;        // explicit i64
let mut counter = 0;    // mutable variable
counter = counter + 1;  // mutation allowed with `mut`

let ptr: *i32 = alloc(i32);  // heap allocation
*ptr = 42;
free(ptr);
```

### 3.4 Functions

```dem
fn add(x: i32, y: i32) -> i32 {
    return x + y;
}

// void return
fn log(message: string) {
    console.println(message);
}

// Multiple return values (via struct)
fn divide(a: i32, b: i32) -> struct { quot: i32; rem: i32; } {
    return { quot: a / b, rem: a % b };
}
```

### 3.5 Control Flow

```dem
// if/else
if x > 0 {
    console.println("positive");
} else if x < 0 {
    console.println("negative");
} else {
    console.println("zero");
}

// while
let mut i = 0;
while i < 10 {
    console.println(i);
    i = i + 1;
}

// for (C-style)
for let mut i = 0; i < 10; i = i + 1 {
    console.println(i);
}

// for-in (over arrays/slices)
let arr: i32[5] = [1, 2, 3, 4, 5];
for let item in arr {
    console.println(item);
}
```

### 3.6 Structs

```dem
struct Point {
    x: i32;
    y: i32;
}

fn distance(p: Point) -> f64 {
    return sqrt(p.x * p.x + p.y * p.y);
}

// Struct literals
let p = Point { x: 3, y: 4 };
let d = distance(p);
```

### 3.7 Enums (tagged unions)

```dem
enum Result {
    Ok(i32);
    Err(string);
}

fn divide_safe(a: i32, b: i32) -> Result {
    if b == 0 {
        return Result::Err("division by zero");
    }
    return Result::Ok(a / b);
}

// Pattern matching
match divide_safe(10, 2) {
    Result::Ok(val) => console.println(val),
    Result::Err(msg) => console.println("Error: " + msg),
}
```

### 3.8 Modules and Namespaces

```
Directory tree:
  src/
  ├── main.dem
  ├── math/
  │   ├── vec.dem
  │   └── matrix.dem
  └── utils/
      └── log.dem
```

```dem
// main.dem
import "math/vec";       // imports module, binds to namespace `vec`
import "utils/log";      // binds to `log`

fn main() -> i32 {
    let v = vec.Vec3 { x: 1.0, y: 2.0, z: 3.0 };
    log.info("vector created");
    return 0;
}
```

```dem
// math/vec.dem
pub struct Vec3 {
    x: f64;
    y: f64;
    z: f64;
}

pub fn length(v: Vec3) -> f64 {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

// Private — not exported
fn internal_helper() -> i32 {
    return 42;
}
```

### 3.9 Inline Assembly

```dem
// Direct DASM syntax — maps to the assembler pipeline
fn get_cpu_vendor() -> string {
    let vendor: u8[12];
    asm {
        mov eax, 0
        cpuid
        mov [vendor + 0], ebx
        mov [vendor + 4], edx
        mov [vendor + 8], ecx
    }
    return vendor;
}

// With register readback
fn rdtsc() -> u64 {
    let low: u32;
    let high: u32;
    asm {
        rdtsc
        mov [low], eax
        mov [high], edx
    }
    return (high as u64) << 32 | (low as u64);
}
```

### 3.10 Intrinsics

```dem
// CPU intrinsics — compiler-known functions mapping to single opcodes
let vendor = @cpuid(0);       // returns struct with eax/ebx/ecx/edx
let cycles = @rdtsc();        // returns u64 timestamp
@cli();                       // clear interrupt flag
@sti();                       // set interrupt flag
@hlt();                       // halt
let swapped = @bswap(0x1234); // byte swap
```

### 3.11 Register Access

```dem
// Direct register read/write — for systems programming
register eax = 42;            // write to EAX
let value = register eax;     // read from EAX
register xmm0 = 3.14;         // write to XMM0 (FPU/SIMD)

// Named Demi VM registers
register r8 = register r0 + register r1;  // R8 = R0 + R1

// Useful for:
// - Accessing CPU flags: register eflags
// - Reading syscall results: register eax after INT 0x80
// - Setting up calling conventions before asm blocks
```

---

## 4. Pipeline (Revised with C-like Semantics)

```
┌─────────────────────────────────────────────────────────────────┐
│                     demi.toml (project config)                    │
│  syntax_profile, type_system, memory_model, custom_keywords...   │
└──────────────────────────┬──────────────────────────────────────┘
                           │ feeds into every phase
                           ▼
┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐
│  Lexer   │──▶│  Parser  │──▶│ Semantic │──▶│   IR     │──▶│ Backend  │
│ (config) │   │ (C-like) │   │ Analyzer │   │ Generator│   │(existing)│
└──────────┘   └──────────┘   └──────────┘   └──────────┘   └──────────┘
     │               │               │               │
     │           Grammar:        Type Check     DemiAST→IR
     │           - statements    Name Resolve    asm blocks
     │           - explicit ret  Scope Rules     map directly
     │           - C precedence                  to DASM
     │
     │  Tokens map:
     │  fn while for if else return let mut
     │  struct enum match import pub register
     │  i8 i16 i32 i64 u8 u16 u32 u64 f32 f64 bool
     │  string *T T? T[] T[N]
     │  asm @intrinsic
```

### 4.1 Demi AST (C-like)

```cpp
struct DemiModule {
    std::string name;                              // module name (derived from file path)
    std::vector<std::string> imports;              // import "path/to/module"
    std::vector<std::unique_ptr<DemiFunction>> functions;
    std::vector<std::unique_ptr<DemiStruct>> structs;
    std::vector<std::unique_ptr<DemiEnum>> enums;
    std::vector<std::unique_ptr<DemiGlobal>> globals;
};

struct DemiFunction {
    std::string name;
    std::vector<DemiParam> params;                 // (name, type, default?)
    std::unique_ptr<DemiType> return_type;         // void if null
    std::vector<std::unique_ptr<DemiStmt>> body;   // statement list
    bool is_public;
};

enum class DemiStmtKind {
    Return, If, While, For, ForIn, Let, Assign, Expr,
    Block, AsmBlock, RegisterWrite, Match
};

struct DemiStmt {
    DemiStmtKind kind;
    // Tagged union per kind
};

enum class DemiExprKind {
    Literal, Var, Binary, Unary, Call, Index,
    Member, StructLit, RegisterRead, Intrinsic, Cast, Ref, Deref
};
```

---

## 5. Project Structure

```
src/language/
├── lexer/                  # Configurable tokenization (Stage 2)
│   ├── lexer.hpp           # ConfigurableLexer base
│   ├── lexer_c.cpp         # C-like tokenizer (Stage 1 — hardcoded)
│   └── token_profiles.hpp  # Token sets per syntax profile (Stage 2)
│
├── parser/                 # C-like recursive descent parser
│   ├── parser.hpp
│   ├── parser.cpp           # expr → stmt → decl → module
│   └── precedence.hpp       # C operator precedence table
│
├── ast/                    # Demi language AST
│   ├── demi_ast.hpp        # Module, Function, Stmt, Expr
│   ├── types.hpp           # Type representation (Primitive, Ptr, Array, Struct, Enum, Option)
│   └── symbols.hpp         # Symbol table (persistent, immutable)
│
├── semantic/               # Type checking + name resolution
│   ├── type_checker.hpp    # Infer + check types
│   ├── symbol_resolver.hpp # Resolve names, imports, scopes
│   └── constant_folder.hpp # CTFE for const expressions
│
├── codegen/                # DemiAST → IRProgram
│   ├── ir_generator.hpp    # Walk DemiAST, emit IRInstructions
│   ├── asm_codegen.hpp     # asm { ... } → IRProgram (delegates to DASM)
│   └── ir_optimizer.hpp    # Peephole, dead code (Stage 3+)
│
├── modules/                # Module system
│   ├── module_loader.hpp   # import "path" → parse + resolve
│   └── module_registry.hpp # Loaded modules cache, namespace binding
│
└── profiles/               # Built-in syntax profiles (Stage 2)
    ├── c_like.toml
    ├── rust_like.toml
    └── custom.toml

demi.toml                    # Per-project config
```

---

## 6. Memory Model (Stage 1: Manual)

```dem
let ptr: *i32 = alloc(i32);   // allocate sizeof(i32) bytes, returns *i32
*ptr = 42;
free(ptr);                     // deallocate

let arr: *i32 = alloc(i32, 10);  // allocate array of 10 i32s
arr[0] = 1;
arr[9] = 10;
free(arr);
```

Maps to Demi VM:
- `alloc(T)` → calls VM memory allocator
- `free(ptr)` → returns memory to VM
- `*ptr = val` → write to memory at address
- `let x = *ptr` → read from memory at address

Stage 3+ (RAII): compiler inserts destructor calls at scope exit. Optional borrow checker for reference safety.

---

## 7. Inline Assembly → IRProgram Integration

`asm { ... }` blocks are compiled by the existing DASM pipeline:

```
asm { mov eax, [var]; add eax, ebx; }
        │
        ▼
   DemiAsmBlock (AST)
        │
        ▼
   DASM Parser (lexer.cpp → parser.cpp)
        │
        ▼
   IRProgram (same as standalone .asm files)
        │
        ▼
   Merge into main module's IRProgram
```

The asm block inherits the Demi function's symbol table — local variable names are resolved to stack offsets or VM registers before passing to DASM.

---

## 8. Register Access Implementation

```
register eax = 42         →  emit LOAD_IMM R0, 42   (EAX = R0 in VM)
let x = register eax      →  emit MOV R8, R0          (copy EAX to variable)
register r8 = register r0 →  emit MOV R8, R0          (VM register-to-register)
```

The compiler maintains a mapping of x86 register names → VM register indices:
```
eax/rax  → R0    ebx/rbx  → R3    ecx/rcx → R1    edx/rdx → R2
esi/rsi  → R6    edi/rdi  → R7    ebp/rbp → R5    esp/rsp → R4
r8-r15   → R8-R15 (VM extended registers)
xmm0-7   → R133-R126 (VM SIMD registers, reversed)
eflags   → RFLAGS (read-only via get_flags)
```

---

## 9. Design Decisions

| Question | Decision |
|----------|----------|
| AST ownership | `unique_ptr` tree for Stage 1 |
| Type representation | `std::variant<Primitive, Ptr, Array, Struct, Enum, Option>` |
| Error recovery | Panic mode parser (consume until sync token) |
| Symbol table | Persistent (immutable, hash map per scope, shared between phases) |
| IR target | IRProgram directly |
| File extension | `.dem` |
| Entry point | `fn main() -> i32` |
| C ABI | `extern "C" fn` blocks for library interop |
| Semicolons | Required after all statements (C-like) |
| Blocks | `{ ... }` only, no significant whitespace |
| Comments | `// line comment`, `/* block comment */` |
| Case sensitivity | Case-sensitive (C-like) |

---

## 10. Stage 1 Target

```
// A complete, compilable Stage 1 program
import "console";

struct Vec3 {
    x: f64;
    y: f64;
    z: f64;
}

fn length(v: Vec3) -> f64 {
    let sum = v.x * v.x + v.y * v.y + v.z * v.z;
    return sqrt(sum);
}

pub fn main() -> i32 {
    let v = Vec3 { x: 3.0, y: 4.0, z: 0.0 };
    let len = length(v);
    console.println("length = " + len.to_string());

    // inline asm for fun
    let vendor: string;
    asm {
        mov eax, 0
        cpuid
        mov [vendor], ebx
    }
    console.println("CPU: " + vendor);

    return 0;
}
```

This exercises: modules, structs, functions, locals, type inference, arithmetic, asm blocks, and the standard library. Every feature has a clear path to IRProgram.
