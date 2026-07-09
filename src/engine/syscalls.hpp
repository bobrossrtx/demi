#pragma once

#include <cstdint>

#ifdef _WIN32
// MingW / Windows headers sometimes define these as macros, clashing with our enum
#undef SYS_OPEN
#undef SYS_CLOSE
#undef SYS_READ
#undef SYS_WRITE
#endif

namespace DemiEngine {

// Linux i386 syscall numbers (INT 0x80)
// Reference: https://chromium.googlesource.com/chromiumos/docs/+/master/constants/syscalls.md
enum class Syscall : uint32_t {
    // Process Control
    SYS_EXIT        = 1,
    SYS_ALLOC       = 200,   // Demi: bump-allocate bytes from VM heap
    SYS_FREE        = 201,   // Demi: free allocation (no-op for bump)
    SYS_TIME_MS     = 202,   // Demi: get monotonic time in ms
    SYS_RANDOM      = 203,   // Demi: get random u32
    SYS_MEMCPY      = 204,   // Demi: mem.copy(dst, src, n)
    SYS_MEMSET      = 205,   // Demi: mem.set(addr, val, n)
    SYS_STRLEN      = 206,   // Demi: str.len(addr)
    SYS_PRINT_INT   = 207,   // Demi: console.print_int(n)
    SYS_ARRAY_NEW   = 208,   // Demi: array.new(size) → addr
    SYS_ARRAY_GET   = 209,   // Demi: array.get(arr, i) → value
    SYS_ARRAY_SET   = 210,   // Demi: array.set(arr, i, val)
    SYS_FILE_OPEN   = 211,   // Demi: file.open(path, mode) → fd
    SYS_FILE_READ   = 212,   // Demi: file.read(fd, buf, n) → bytes_read
    SYS_FILE_WRITE  = 213,   // Demi: file.write(fd, buf, n) → bytes_written
    SYS_FILE_CLOSE  = 214,   // Demi: file.close(fd)
    SYS_FORK        = 2,
    SYS_ACCESS       = 33,
    SYS_WAITPID     = 7,
    SYS_EXECVE      = 11,
    SYS_KILL        = 37,
    SYS_GETPID      = 20,
    SYS_GETPPID     = 64,
    
    // File Operations
    SYS_READ        = 3,
    SYS_WRITE       = 4,
    SYS_OPEN        = 5,
    SYS_CLOSE       = 6,
    SYS_CREAT       = 8,
    SYS_LINK        = 9,
    SYS_UNLINK      = 10,
    SYS_CHDIR       = 12,
    SYS_LSEEK       = 19,
    SYS_RENAME      = 38,
    SYS_MKDIR       = 39,
    SYS_RMDIR       = 40,
    SYS_DUP         = 41,
    SYS_PIPE        = 42,
    SYS_IOCTL       = 54,
    SYS_FCNTL       = 55,
    SYS_DUP2        = 63,
    SYS_STAT        = 106,
    SYS_FSTAT       = 108,
    SYS_READLINK     = 85,
    SYS_GETCWD       = 183,
    SYS_GETDENTS     = 141,
    SYS_FSYNC       = 118,
    
    // Memory Management
    SYS_BRK         = 45,
    SYS_MMAP        = 90,
    SYS_MUNMAP      = 91,
    SYS_MPROTECT    = 125,
    SYS_MMAP2       = 192,
    
    // Time
    SYS_TIME        = 13,
    SYS_GETTIMEOFDAY = 78,
    SYS_NANOSLEEP   = 162,
    
    // Signals
    SYS_SIGNAL      = 48,
    SYS_SIGACTION   = 67,
    SYS_SIGRETURN   = 119,
    
    // Network (sockets)
    SYS_SOCKET      = 359,  // Actually socketcall on i386
    SYS_BIND        = 361,
    SYS_CONNECT     = 362,
    SYS_LISTEN      = 363,
    SYS_ACCEPT      = 364,
    SYS_SEND        = 369,
    SYS_RECV        = 371,
    
    // Information
    SYS_UNAME       = 122,
    SYS_LSTAT       = 107,
    
    // Other
    SYS_GETUID      = 24,
    SYS_GETEUID     = 49,
    SYS_GETGID      = 47,
    SYS_GETEGID     = 50,
    
    // Placeholder for unsupported
    SYS_UNSUPPORTED = 0xFFFFFFFF
};

// Helper to convert syscall number to enum
inline Syscall to_syscall(uint32_t num) {
    switch (num) {
        case 1: return Syscall::SYS_EXIT;
        case 200: return Syscall::SYS_ALLOC;
        case 201: return Syscall::SYS_FREE;
        case 202: return Syscall::SYS_TIME_MS;
        case 203: return Syscall::SYS_RANDOM;
        case 204: return Syscall::SYS_MEMCPY;
        case 205: return Syscall::SYS_MEMSET;
        case 206: return Syscall::SYS_STRLEN;
        case 207: return Syscall::SYS_PRINT_INT;
        case 208: return Syscall::SYS_ARRAY_NEW;
        case 209: return Syscall::SYS_ARRAY_GET;
        case 210: return Syscall::SYS_ARRAY_SET;
        case 211: return Syscall::SYS_FILE_OPEN;
        case 212: return Syscall::SYS_FILE_READ;
        case 213: return Syscall::SYS_FILE_WRITE;
        case 214: return Syscall::SYS_FILE_CLOSE;
        case 7: return Syscall::SYS_WAITPID;
        case 2: return Syscall::SYS_FORK;
        case 33: return Syscall::SYS_ACCESS;
        case 183: return Syscall::SYS_GETCWD;
        case 141: return Syscall::SYS_GETDENTS;
        case 3: return Syscall::SYS_READ;
        case 4: return Syscall::SYS_WRITE;
        case 5: return Syscall::SYS_OPEN;
        case 10: return Syscall::SYS_UNLINK;
        case 11: return Syscall::SYS_EXECVE;
        case 6: return Syscall::SYS_CLOSE;
        case 45: return Syscall::SYS_BRK;
        case 106: return Syscall::SYS_STAT;
        case 108: return Syscall::SYS_FSTAT;
        case 85: return Syscall::SYS_READLINK;
        case 54: return Syscall::SYS_IOCTL;
        case 90: return Syscall::SYS_MMAP;
        case 192: return Syscall::SYS_MMAP2;
        default: return Syscall::SYS_UNSUPPORTED;
    }
}

// Helper to get syscall name for logging
inline const char* syscall_name(Syscall sc) {
    switch (sc) {
        case Syscall::SYS_EXIT: return "sys_exit";
        case Syscall::SYS_ALLOC: return "sys_alloc";
        case Syscall::SYS_FREE: return "sys_free";
        case Syscall::SYS_TIME_MS: return "sys_time_ms";
        case Syscall::SYS_RANDOM: return "sys_random";
        case Syscall::SYS_MEMCPY: return "sys_memcpy";
        case Syscall::SYS_MEMSET: return "sys_memset";
        case Syscall::SYS_STRLEN: return "sys_strlen";
        case Syscall::SYS_PRINT_INT: return "sys_print_int";
        case Syscall::SYS_ARRAY_NEW: return "sys_array_new";
        case Syscall::SYS_ARRAY_GET: return "sys_array_get";
        case Syscall::SYS_ARRAY_SET: return "sys_array_set";
        case Syscall::SYS_FILE_OPEN: return "sys_file_open";
        case Syscall::SYS_FILE_READ: return "sys_file_read";
        case Syscall::SYS_FILE_WRITE: return "sys_file_write";
        case Syscall::SYS_FILE_CLOSE: return "sys_file_close";
            case Syscall::SYS_ACCESS: return "sys_access";
            case Syscall::SYS_GETDENTS: return "sys_getdents";
            case Syscall::SYS_GETCWD: return "sys_getcwd";
            case Syscall::SYS_WAITPID: return "sys_waitpid";
        case Syscall::SYS_FORK: return "sys_fork";
            case Syscall::SYS_UNLINK: return "sys_unlink";
        case Syscall::SYS_EXECVE: return "sys_execve";
        case Syscall::SYS_READ: return "sys_read";
        case Syscall::SYS_WRITE: return "sys_write";
        case Syscall::SYS_OPEN: return "sys_open";
        case Syscall::SYS_CLOSE: return "sys_close";
        case Syscall::SYS_BRK: return "sys_brk";
            case Syscall::SYS_READLINK: return "sys_readlink";
        case Syscall::SYS_IOCTL: return "sys_ioctl";
        case Syscall::SYS_MMAP: return "sys_mmap";
        case Syscall::SYS_MMAP2: return "sys_mmap2";
        default: return "sys_unknown";
    }
}

} // namespace DemiEngine
