# Demi Standard Library — Architecture & Roadmap

## Architecture Overview

The Demi stdlib has three implementation tiers:

| Tier | Mechanism | Performance | Complexity | Example |
|------|-----------|-------------|------------|---------|
| **Compiler Builtins** | Inline bytecode emitted by IRGenerator | Fastest (zero call overhead) | Low (fixed patterns) | `math.abs`, `console.print_i32` |
| **VM Syscalls** | New INT 0x80 vectors added to syscall handler | Fast (single interrupt) | Medium (VM changes) | `sys.time_ms`, `sys.random` |
| **Demi Library Files** | `.dem` source files compiled and linked | Normal (function calls) | Low (pure Demi code) | `str.len`, `array.sort` |

### When to use each tier:
- **Builtins**: Simple operations with 1-10 bytecode instructions. Fixed register usage.
- **Syscalls**: Operations needing OS or VM internals (time, random, alloc).
- **Library files**: Complex algorithms, multi-function modules, user-extensible code.

---

## Module Breakdown

### 1. console (I/O) — Priority: P0

| Function | Tier | Status | Notes |
|----------|------|--------|-------|
| `console.print_i32(n)` | Builtin | ✅ DONE | Raw byte output |
| `console.print_hex(n)` | Builtin | 🔲 | 8-char hex: "0000002A" |
| `console.print_int(n)` | Builtin | 🔲 | Decimal ASCII digits |
| `console.print_uint(n)` | Builtin | 🔲 | Unsigned decimal |
| `console.print_float(f)` | Builtin | 🔲 | Via FPU ops |
| `console.println(s)` | Builtin | ✅ DONE | OUTSTR + newline |
| `console.print(s)` | Builtin | ✅ DONE | String output |
| `console.read_i32()` → i32 | Syscall | 🔲 | SYS_READ from stdin |
| `console.read_line()` → str | Syscall | 🔲 | Buffered line read |

**Implementation plan:**
- `print_hex`: CMP+JL for nibble→ASCII, 8× OUT (complex but doable, ~80 bytes of bytecode)
- `print_int`: divide-by-10 loop with digit stack (needs new DIV loop pattern)
- `print_uint`: same as print_int without sign handling
- `read_i32`: new syscall in handle_syscall (0x80), reads 4 bytes from fd 0
- `read_line`: new syscall, reads until newline into heap buffer

### 2. math — Priority: P0

| Function | Tier | Status | Notes |
|----------|------|--------|-------|
| `math.abs(n)` → i32 | Builtin | ✅ DONE | CMP+JGE+SUB from zero |
| `math.min(a,b)` → i32 | Builtin | ✅ DONE | CMP+JLE+MOV |
| `math.max(a,b)` → i32 | Builtin | ✅ DONE | CMP+JGE+MOV |
| `math.clamp(x, lo, hi)` → i32 | Builtin | 🔲 | min(max(x, lo), hi) |
| `math.pow(base, exp)` → i32 | Library | 🔲 | Repeated MUL loop |
| `math.sqrt_f(f)` → float | Builtin | 🔲 | FSQRT FPU opcode |
| `math.sin_f(f)` → float | Syscall | 🔲 | Need trig in VM or library |
| `math.cos_f(f)` → float | Syscall | 🔲 | |
| `math.random()` → i32 | Syscall | 🔲 | SYS_RANDOM |
| `math.random_range(lo, hi)` → i32 | Library | 🔲 | random() % range + lo |
| `math.seed_random(seed)` | Syscall | 🔲 | Seed the RNG |

**Implementation plan:**
- `clamp`: emit `min(max(x, lo), hi)` bytecode
- `pow`: Demi library function with MUL loop
- `sqrt_f`: emit FSQRT (opcode 0xAA) — FPU handler exists, codegen needed
- `sin_f/cos_f`: complex — defer to phase 2, or implement as Taylor series in library
- `random`: new syscall using std::rand or xorshift

### 3. string — Priority: P1

| Function | Tier | Status | Notes |
|----------|------|--------|-------|
| `str.len(addr)` → i32 | Library | 🔲 | Count until null byte |
| `str.compare(a, b)` → i32 | Library | 🔲 | Byte-by-byte CMP |
| `str.copy(dst, src)` | Library | 🔲 | Byte copy loop |
| `str.concat(a, b)` → addr | Syscall | 🔲 | Alloc + copy |
| `str.to_int(addr)` → i32 | Library | 🔲 | Parse decimal digits |
| `str.from_int(n, buf)` | Builtin | 🔲 | Write decimal to buffer |

**Implementation plan:**
- String operations work on memory addresses
- `str.len`: LOAD byte loop until 0
- `str.compare`: LOAD+CMP loop
- `str.from_int`: reverse of print_int — write digits to memory buffer
- String literals in .dem files are stored as null-terminated byte sequences

### 4. array — Priority: P2

| Function | Tier | Status | Notes |
|----------|------|--------|-------|
| `array.new(size)` → addr | Syscall | 🔲 | Alloc size*4 bytes |
| `array.get(arr, i)` → i32 | Builtin | 🔲 | LOAD from arr+i*4 |
| `array.set(arr, i, val)` | Builtin | 🔲 | STORE to arr+i*4 |
| `array.len(arr)` → i32 | Library | 🔲 | First word stores length |
| `array.fill(arr, val, n)` | Library | 🔲 | STORE loop |
| `array.copy(dst, src, n)` | Library | 🔲 | LOAD+STORE loop |

**Implementation plan:**
- Arrays stored as length-prefixed blocks: [len:4][elem0:4][elem1:4]...
- `array.get`: LOAD from address with offset (need LEA or ADD for offset)
- `array.set`: STORE to offset address
- `array.new`: syscall allocating (len+1)*4 bytes

### 5. sys — Priority: P1

| Function | Tier | Status | Notes |
|----------|------|--------|-------|
| `sys.time_ms()` → i64 | Syscall | 🔲 | clock_gettime or rdtsc |
| `sys.sleep_ms(n)` | Syscall | 🔲 | usleep or nanosleep |
| `sys.exit(code)` | Syscall | ✅ DONE | SYS_EXIT (INT 0x80 with RAX=1) |
| `sys.alloc(bytes)` → addr | Syscall | 🔲 | Bump allocator in VM |
| `sys.free(addr)` | Syscall | 🔲 | No-op for bump, or free list |
| `sys.heap_used()` → i32 | Syscall | 🔲 | Current heap usage |
| `sys.argc()` → i32 | Syscall | 🔲 | Command line arg count |
| `sys.argv(i)` → addr | Syscall | 🔲 | Command line arg string |

**Implementation plan:**
- `time_ms`: new syscall reading wall clock
- `sleep_ms`: syscall wrapping usleep
- `alloc`: bump pointer in VM memory (starts at 0x10000 after code+stack)
- `free`: no-op for now (bump allocator)
- `argc/argv`: syscall accessing main() args

### 6. memory — Priority: P2

| Function | Tier | Status | Notes |
|----------|------|--------|-------|
| `mem.copy(dst, src, n)` | Builtin | 🔲 | REP MOVSB-style loop |
| `mem.set(addr, val, n)` | Builtin | 🔲 | REP STOSB-style loop |
| `mem.compare(a, b, n)` → i32 | Library | 🔲 | Byte-by-byte CMP |
| `mem.zero(addr, n)` | Builtin | 🔲 | STORE 0 loop |

**Implementation plan:**
- Memory ops emit LOAD/STORE loops
- `mem.copy`: LOAD src[i], STORE dst[i], INC both, DEC counter, JNZ loop
- `mem.zero`: STORE 0 to addr[i], INC addr, DEC n, JNZ loop

### 7. file — Priority: P2

| Function | Tier | Status | Notes |
|----------|------|--------|-------|
| `file.open(path, mode)` → fd | Syscall | 🔲 | open() syscall |
| `file.read(fd, buf, n)` → i32 | Syscall | 🔲 | read() syscall |
| `file.write(fd, buf, n)` → i32 | Syscall | 🔲 | write() syscall |
| `file.close(fd)` | Syscall | 🔲 | close() syscall |
| `file.size(fd)` → i32 | Syscall | 🔲 | fstat |

---

## Implementation Priority Roadmap

### Phase 1: Core Builtins (current session + 1-2 sessions)
- [ ] `console.print_hex(n)` — hex nibble extraction
- [ ] `console.print_int(n)` — decimal digit output  
- [ ] `math.clamp(x, lo, hi)` — min/max composition
- [ ] `math.sqrt_f(f)` — FSQRT codegen
- [ ] `sys.alloc(bytes)` — bump allocator syscall
- [ ] `sys.time_ms()` — clock syscall
- [ ] `sys.random()` — RNG syscall

### Phase 2: String & Array (2-3 sessions)
- [ ] `str.len`, `str.compare`, `str.from_int`
- [ ] `array.new`, `array.get`, `array.set`
- [ ] `mem.copy`, `mem.zero`
- [ ] `console.read_i32()`

### Phase 3: Full Library (3-5 sessions)
- [ ] `.dem` library files with pre-compiled modules
- [ ] `math.pow`, `math.random_range`
- [ ] `file.*` I/O module
- [ ] `str.to_int`, `str.concat`

---

## Syscall Number Allocation (INT 0x80)

| Vector | Name | Args | Returns |
|--------|------|------|---------|
| 1 | SYS_EXIT | RAX=1 | — |
| 2 | SYS_ALLOC | RAX=bytes | RAX=addr |
| 3 | SYS_FREE | RAX=addr | — |
| 4 | SYS_TIME_MS | — | RAX=ms |
| 5 | SYS_RANDOM | — | RAX=random |
| 6 | SYS_SLEEP_MS | RAX=ms | — |
| 7 | SYS_OPEN | RAX=path, RBX=mode | RAX=fd |
| 8 | SYS_READ | RAX=fd, RBX=buf, RCX=n | RAX=bytes_read |
| 9 | SYS_WRITE | RAX=fd, RBX=buf, RCX=n | RAX=bytes_written |
| 10 | SYS_CLOSE | RAX=fd | — |
| 11 | SYS_ARGC | — | RAX=count |
| 12 | SYS_ARGV | RAX=i | RAX=addr |

---

## Design Principles

1. **Builtins for hot paths**: emit raw bytecode for <20 instruction operations
2. **Syscalls for OS/VM access**: time, random, alloc, file I/O
3. **Library files for algorithms**: .dem source code for complex logic
4. **Register convention**: 
   - Builtin calls use R0-R7 for args (R0=arg0, R1=arg1, ...)
   - Return values in R0
   - Caller-saves: builtins may clobber R0-R7
5. **Error handling**: 
   - Syscalls return -1 in RAX on error (like Linux)
   - Builtins return sentinel values (0, -1)
   - Future: exception handling via interrupt vectors
6. **Memory model**:
   - Heap starts at 0x10000 (above code + stack)
   - Bump allocator: `sys.alloc` increments heap pointer
   - Strings are null-terminated byte sequences at heap addresses

---

## Test Strategy

| Tier | Test approach |
|------|---------------|
| Builtins | VM execution tests (hex bytecode → verify output) |
| Syscalls | Unit tests calling syscall directly |
| Library | Compile .dem → execute → verify output |

---

## Next Immediate Task

Phase 1, starting with `console.print_hex(n)` — the highest-impact missing builtin.
