# Networking with INT 0x80 (socketcall)

## Availability

| Mode | Networking | Notes |
|------|-----------|-------|
| VM (`demi -A`) | **No** | Syscall 102 not in VM allowlist — returns `-ENOSYS` |
| Native 32-bit (`-x86 -o`) | **Yes** | Uses kernel's `socketcall(102)` directly |
| Native 64-bit (`-o`) | **Yes** | Maps i386 sub-calls to x86-64 individual syscalls |

Networking is only available when compiling to a standalone executable with the
`-o` flag. The VM sandbox blocks socket operations.

## i386 socketcall convention

Linux i386 uses `socketcall` (syscall 102) as a multiplexer. All socket
operations go through this single syscall:

```
eax = 102          ; socketcall
ebx = sub-call     ; which operation
ecx = *args        ; pointer to unsigned long array in memory
```

## Supported sub-calls

| sub-call | Operation    | Args                                      | Pointers           |
|----------|-------------|-------------------------------------------|--------------------|
| 1        | socket      | domain, type, protocol                    | none               |
| 2        | bind        | fd, *addr, addrlen                        | addr (sockaddr)    |
| 3        | connect     | fd, *addr, addrlen                        | addr (sockaddr)    |
| 4        | listen      | fd, backlog                               | none               |
| 5        | accept      | fd, *addr, *addrlen                       | addr, addrlen      |
| 9        | send        | fd, *buf, len, flags                      | buf                |
| 10       | recv        | fd, *buf, len, flags                      | buf                |

Unsupported sub-calls return `-ENOSYS` in 64-bit mode or pass through to the
kernel as-is in 32-bit mode.

## Building a sockaddr_in

The `sockaddr_in` struct is 16 bytes in memory. Build it with `STORE`
instructions at a known offset, then pass the offset to bind/connect/accept.

```
Offset  Size  Field
  0      2    sin_family  (AF_INET = 2)
  2      2    sin_port    (0 = any port, or htons(port))
  4      4    sin_addr    (0 = INADDR_ANY, or htonl(ip))
  8      8    sin_zero    (padding, must be 0)
```

Demi only has 32-bit STORE, so build sockaddr_in as four dwords:

```asm
; sockaddr_in at offset 0x200: AF_INET, port 8080, INADDR_ANY
LOAD_IMM EAX, 0x901F0002    ; sin_family=2, sin_port=0x1F90 (8080 big-endian)
STORE EAX, [0x200]
LOAD_IMM EAX, 0             ; sin_addr = INADDR_ANY
STORE EAX, [0x204]
LOAD_IMM EAX, 0             ; sin_zero[0..3]
STORE EAX, [0x208]
LOAD_IMM EAX, 0             ; sin_zero[4..7]
STORE EAX, [0x20C]
```

Port numbers must be in **network byte order** (big-endian). For port 8080:
`0x1F90` → stored as `0x901F` in the high word of the first dword.

## Example: socket + bind + listen

```asm
; Create a listening socket on port 8080
; Compiled with: demi -x86 -A server.asm -o server

.org 0x00
_start:
    ; ── socket(AF_INET, SOCK_STREAM, 0) ──
    LOAD_IMM EAX, 2            ; AF_INET
    STORE EAX, [0x100]
    LOAD_IMM EAX, 1            ; SOCK_STREAM
    STORE EAX, [0x104]
    LOAD_IMM EAX, 0            ; protocol
    STORE EAX, [0x108]

    LOAD_IMM EAX, 102          ; socketcall
    LOAD_IMM EBX, 1            ; SYS_SOCKET
    LOAD_IMM ECX, 0x100        ; args array
    INT 0x80

    ; Check for error (fd < 0)
    LOAD_IMM EBX, 0
    CMP EAX, EBX
    JL error_exit

    STORE EAX, [0x300]         ; save fd

    ; ── Build sockaddr_in at 0x200 ──
    LOAD_IMM EAX, 0x901F0002   ; AF_INET, port 8080
    STORE EAX, [0x200]
    LOAD_IMM EAX, 0
    STORE EAX, [0x204]         ; INADDR_ANY
    STORE EAX, [0x208]         ; sin_zero
    STORE EAX, [0x20C]

    ; ── bind(fd, &sockaddr, 16) ──
    LOAD EAX, [0x300]
    STORE EAX, [0x100]         ; args[0] = fd
    LOAD_IMM EAX, 0x200
    STORE EAX, [0x104]         ; args[1] = &sockaddr
    LOAD_IMM EAX, 16
    STORE EAX, [0x108]         ; args[2] = sizeof

    LOAD_IMM EAX, 102
    LOAD_IMM EBX, 2            ; SYS_BIND
    LOAD_IMM ECX, 0x100
    INT 0x80

    ; ── listen(fd, 5) ──
    LOAD EBX, [0x300]
    STORE EBX, [0x100]         ; args[0] = fd
    LOAD_IMM EAX, 5
    STORE EAX, [0x104]         ; args[1] = backlog

    LOAD_IMM EAX, 102
    LOAD_IMM EBX, 4            ; SYS_LISTEN
    LOAD_IMM ECX, 0x100
    INT 0x80

    ; Socket is now listening on port 8080
    ; ... accept loop would go here ...

    ; ── Close and exit ──
    LOAD_IMM EAX, 6            ; sys_close
    LOAD EBX, [0x300]
    INT 0x80

    LOAD_IMM EAX, 1            ; sys_exit
    LOAD_IMM EBX, 0
    INT 0x80
    HALT

error_exit:
    LOAD_IMM EAX, 1
    LOAD_IMM EBX, 1
    INT 0x80
    HALT
```

## How it works internally

### 32-bit native mode

The compiler patches pointer arguments in the args array before calling
`socketcall(102)`. For each sub-call, it knows which args are pointers:

- **socket, listen**: all integer args — no patching needed
- **bind, connect**: arg[1] is a sockaddr pointer — add ESI (memory base)
- **accept**: arg[1] and arg[2] are pointers — both get ESI added
- **send, recv**: arg[1] is a buffer pointer — add ESI

The kernel receives a real userspace pointer to the args array, with pointer
args already resolved to real addresses.

### 64-bit native mode

The compiler maps i386 `socketcall` sub-calls to x86-64 individual syscalls:

| i386 sub-call | x86-64 syscall | Number |
|---------------|---------------|--------|
| socket        | sys_socket    | 41     |
| bind          | sys_bind      | 49     |
| connect       | sys_connect   | 42     |
| listen        | sys_listen    | 50     |
| accept        | sys_accept    | 43     |
| send          | sys_sendto    | 44     |
| recv          | sys_recvfrom  | 45     |

The args array is unpacked into the appropriate registers (RDI, RSI, RDX, R10,
R8, R9). Pointer args get the memory base (RSI) added.

## Limitations

- **VM mode**: socketcall is not in the VM syscall table. Use native `-o` mode.
- **Accept**: returns the new fd but the peer address fields may not be
  populated correctly (bytecode offsets in the output struct are not converted
  back).
- **send/recv with flags**: MSG_PEEK and other flags may not work correctly
  with Demi's memory model.
- **No sandbox integration yet**: there is no `--allow-network` flag parallel
  to `--allow-read`/`--allow-write`. The sandbox policy (`sandbox_policy.hpp`)
  explicitly denies `SYS_SOCKET`.
- **Blocking only**: Demi has no event loop or non-blocking I/O — all socket
  operations are blocking.

## Key source files

| File | What |
|------|------|
| `src/codegen/disa_compiler.cpp` | `translate_int80()` — 32-bit socketcall handler (~line 1900), 64-bit dispatch (~line 2100) |
| `src/engine/syscalls.hpp` | Syscall number enum (socketcall=102) |
| `src/engine/sandbox_policy.hpp` | Sandbox rules (SYS_SOCKET denied line 72) |
