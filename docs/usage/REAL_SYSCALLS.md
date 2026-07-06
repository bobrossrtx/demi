# Real Linux Syscalls in DemiEngine

## Overview

DemiEngine now supports **real Linux system calls** via `INT 0x80`. When your assembly program executes `INT 0x80`, the VM invokes the actual Linux kernel `syscall()` function, not a simulation.

## Verification

You can verify that real syscalls are being made using `strace`:

```bash
strace -e trace=write,read,open,close ./bin/demi-engine-debug -A tests/syscall_demo.test.asm
```

## Supported Syscalls

### File I/O

- **sys_read (3)**: Read from file descriptor
  - Reads directly from Linux kernel into VM memory
  - Returns actual bytes read or error code
- **sys_write (4)**: Write to file descriptor
  - Writes VM memory buffer to real file descriptor
  - Works with stdout (1), stderr (2), and real file descriptors
  - Returns actual bytes written or error code

- **sys_open (5)**: Open file
  - Opens real files on the host filesystem
  - Returns actual file descriptor or error code
  - Pathname must be null-terminated string in VM memory

- **sys_close (6)**: Close file descriptor
  - Closes real file descriptors
  - Returns 0 on success or error code

### Process Control

- **sys_exit (1)**: Exit program
  - Stops VM execution (doesn't exit host process)
  - Behaves like normal program termination

### Other

- **sys_ioctl (54)**: I/O control operations
- **sys_brk (45)**: Memory management (simulated)
- **socketcall (102)**: Socket operations — native mode only, see [NETWORKING.md](NETWORKING.md)

## Register Conventions (Linux i386 INT 0x80)

| Register | Purpose        | DemiEngine            |
| -------- | -------------- | --------------------- |
| EAX (R0) | Syscall number | LOAD_IMM R0, <number> |
| EBX (R3) | Argument 1     | LOAD_IMM R3, <arg1>   |
| ECX (R1) | Argument 2     | LOAD_IMM R1, <arg2>   |
| EDX (R2) | Argument 3     | LOAD_IMM R2, <arg3>   |
| ESI (R6) | Argument 4     | LOAD_IMM R6, <arg4>   |
| EDI (R7) | Argument 5     | LOAD_IMM R7, <arg5>   |
| EAX (R0) | Return value   | After INT 0x80        |

## Example: Hello World

```asm
main:
    LOAD_IMM R0, 4          ; sys_write
    LOAD_IMM R3, 1          ; stdout
    LOAD_IMM R1, msg        ; buffer address
    LOAD_IMM R2, 14         ; length
    INT 0x80                ; Real kernel syscall!

    LOAD_IMM R0, 1          ; sys_exit
    LOAD_IMM R3, 0          ; exit code
    INT 0x80

    HALT

msg:
    DB "Hello, World!", 10
```

## Example: Write to stderr

```asm
    LOAD_IMM R0, 4          ; sys_write
    LOAD_IMM R3, 2          ; stderr
    LOAD_IMM R1, error_msg
    LOAD_IMM R2, 20
    INT 0x80

error_msg:
    DB "Error occurred!", 10
```

## Memory Translation

When you pass a memory address (like a buffer pointer or filename), DemiEngine automatically translates VM memory addresses to host pointers. The syscall sees:

- **VM memory address**: Offset within VM's memory array
- **Host pointer**: `&memory[vm_address]`
- **Bounds checking**: Automatic - prevents access outside VM memory

## Error Handling

Real Linux error codes are returned in R0 (EAX):

- **Positive values**: Success (bytes read/written, file descriptor, etc.)
- **Negative values**: Error codes (e.g., -9 = EBADF, -14 = EFAULT)

## Security & Isolation

- **File access**: VM can access host filesystem with same permissions as the demi-engine process
- **sys_exit**: Stops VM only, doesn't terminate host process
- **Memory bounds**: All memory accesses are validated to stay within VM memory
- **Unsupported syscalls**: Return -ENOSYS (-38)

## Test Files

- `tests/syscall_hello.test.asm` - Basic sys_write example
- `tests/syscall_demo.test.asm` - Multiple syscall demonstrations
- Use `strace` to verify real kernel invocations!
