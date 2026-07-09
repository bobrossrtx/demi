# Demi Standard Library Reference

All functions are available as builtins (no imports needed).

## console — Output

| Function | Signature | Description |
|----------|-----------|-------------|
| `console.print_i32(n: i32)` | Builtin | Write low byte of n to stdout (raw byte output) |
| `console.print_hex(n: i32)` | Builtin | Write n as 8-character hex string (e.g., "0000002A") |
| `console.println(s: str)` | Builtin | Write null-terminated string + newline |
| `console.print(s: str)` | Builtin | Write null-terminated string |

```
console.print_hex(0xDEADBEEF)   // → "DEADBEEF"
console.print_i32(65)           // → 'A'
```

## math — Arithmetic

| Function | Signature | Description |
|----------|-----------|-------------|
| `math.abs(n: i32) → i32` | Builtin | Absolute value (0 - n if n < 0) |
| `math.min(a: i32, b: i32) → i32` | Builtin | Smaller of a and b |
| `math.max(a: i32, b: i32) → i32` | Builtin | Larger of a and b |
| `math.clamp(x: i32, lo: i32, hi: i32) → i32` | Builtin | Clamp x to [lo, hi] range |

```
math.abs(-5)       // → 5
math.min(3, 8)     // → 3
math.max(3, 8)     // → 8
math.clamp(100, 0, 75)  // → 75
math.clamp(-10, 0, 75)  // → 0
```

## sys — System

| Function | Signature | Description |
|----------|-----------|-------------|
| `sys.alloc(bytes: i32) → i32` | Syscall 200 | Bump-allocate bytes from VM heap; returns address |
| `sys.time_ms() → i32` | Syscall 202 | Monotonic time in milliseconds |
| `sys.random() → i32` | Syscall 203 | Pseudo-random 32-bit integer |
| `sys.exit(code: i32)` | Syscall 1 | Halt VM with exit code |

```
let p = sys.alloc(16)          // → 0x10000
let q = sys.alloc(8)           // → 0x10010
let t = sys.time_ms()          // → e.g., 10922465
let r = sys.random()           // → e.g., 0xD3DC167E
```

## Heap Memory Model

- Heap starts at `0x10000` (64KB offset from VM memory base)
- `sys.alloc(n)` returns 4-byte-aligned addresses
- Bump allocator: no `free()` support yet
- Stack grows down from `memory.size()` (default 1MB)
- Heap grows up; collision is an OOM condition

## Register Convention

Builtin calls use these registers for arguments:
- R0 = first argument or return value
- R1 = second argument
- R3 = third argument (syscall convention)

All builtin registers (R0-R7) may be clobbered by calls.

## Example: Random Hex Display

```
fn main() {
    let r = sys.random();
    console.print_hex(r);
}
// Output: e.g., "D3DC167E"
```

## Example: Bounded Value

```
fn main() {
    let score = 150;
    let clamped = math.clamp(score, 0, 100);
    console.print_hex(clamped);
}
// Output: "00000064"  (100 = 0x64)
```
