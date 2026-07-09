#pragma once
#include <string>
#include <cstddef>
#include <vector>
#include <cstdint>

// DemiEngine Version Information
#define DEMI_VERSION_MAJOR 1
#define DEMI_VERSION_MINOR 0
#define DEMI_VERSION_PATCH 0
#define DEMI_VERSION_STRING "1.0.0"

// Build information
#define DEMI_BUILD_DATE __DATE__
#define DEMI_BUILD_TIME __TIME__

// Engine capabilities
#define DEMI_SIMD_SUPPORT 1
#define DEMI_PREPROCESSOR_SUPPORT 1
#define DEMI_INCLUDE_SUPPORT 1
#define DEMI_FUSION_SUPPORT 1

// Memory constants
constexpr size_t DEMI_DEFAULT_MEMORY_SIZE = 1024 * 1024;           // 1MB default
constexpr size_t DEMI_MIN_MEMORY_SIZE = 256;                        // 256 bytes minimum (for tests)
constexpr size_t DEMI_MAX_MEMORY_SIZE = 4ULL * 1024 * 1024 * 1024;  // 4GB maximum

// Enum for test filtering modes
enum class TestShowMode {
    ALL,      // Show all tests (default)
    FAILS,    // Show only failed tests
    SUCCESS   // Show only successful tests
};

enum class Architecture {
    AUTO,
    X86,
    X64
};

enum class AssemblyTarget {
    DemiBytecode,
    X86Elf32,
    X86Elf64
};

// Memory size mode
enum class MemorySizeMode {
    DEFAULT,    // Use default 1MB
    FIXED,      // Use a fixed size specified by user
    AUTO        // Auto-detect based on system memory
};

class Config {
public:
    inline static bool debug = false;
    inline static bool trace = false;  // Runtime tracing enabled via .trace directive
    inline static bool verbose = false;  // Default to showing info messages
    inline static bool running_tests = false;
    inline static bool compile_only = false;  // Run without debug outputs
    inline static bool extended_registers = false;  // Show extended register output
    inline static bool assembly_mode = false;  // Assembly mode enabled
    inline static bool memdump = false; // Print memory dump after execution if true
    inline static bool interactive = false; // Interactive REPL mode
    inline static bool debug_step = false; // Step-through debugger mode
    inline static bool quiet_assembly_test = false;  // Quiet mode for assembly tests
    inline static bool quiet = false;  // Global quiet mode - suppress logs and only show results
    inline static bool test_mode = false;  // Test mode - suppress verbose logs during testing
    inline static bool test_debug = false;  // Test debug mode - verbose output for test diagnostics
    inline static bool sandbox_enabled = false;  // --sandbox: enable sandbox mode
    inline static bool allow_read  = false;     // --allow-read: allow host file reads in sandbox
    inline static bool allow_write = false;     // --allow-write: allow host file writes in sandbox
    inline static bool allow_exec  = false;     // --allow-exec: allow fork+execve in sandbox
    inline static bool allow_ioctl = false;    // --allow-ioctl: allow raw ioctl in sandbox
    inline static std::string vfs_container = "/tmp/demi_sandbox.vfs";  // --vfs-container: VFS container path
    inline static bool assembly_test_debug = false;  // Assembly test debug mode - verbose output for assembly test diagnostics
    inline static std::string test_select = "";  // Select specific test(s) by name (comma-separated)
    inline static bool show_test_metadata = false;  // Show test metadata (description, author, category, tags)
    inline static TestShowMode test_show_mode = TestShowMode::ALL;  // Test filtering mode
    inline static std::string debug_file = "debug.log";
    inline static std::string program_file = "";  // Hex file input
    inline static std::string assembly_file = "";  // Assembly source file
    inline static std::string assembly_output = "";  // Assembly bytecode output file (for -ao option)
    inline static std::string hex_out = "";  // Output assembled bytecode as a string hex byte array
    inline static std::string output_name = "";  // Output name for compiled executable
    inline static std::string entry_point_symbol = "_start";  // Entry point symbol name
    inline static bool profiling = false;  // Opcode profiling enabled
    inline static int error_count = 0;

    // Architecture detection and warning state
    inline static Architecture architecture = Architecture::AUTO;
    inline static AssemblyTarget assembly_target = AssemblyTarget::DemiBytecode;
    inline static bool debug_info = false;         // Emit DWARF debug sections in compiled binaries
    inline static bool listing = false;            // Generate .lst listing file
    inline static bool warn_all = true;            // Enable assembler warnings
    inline static bool warn_error = false;         // Treat warnings as errors
    inline static bool shared_library = false;     // Generate shared library (.so) instead of executable
    inline static bool arch_warn_silenced = false;
    inline static bool arch_warn_printed = false;

    // Memory configuration
    inline static MemorySizeMode memory_mode = MemorySizeMode::DEFAULT;
    inline static size_t memory_size = DEMI_DEFAULT_MEMORY_SIZE;  // Configured memory size
    inline static std::string memory_size_str = "";  // Original memory size string for display
};

// Declare device initialization for use in tests and main
class CPU;
void initialize_devices(CPU* cpu = nullptr);
void run_repl();
void run_step_debug(const std::vector<uint8_t>& program, uint32_t entry_addr = 0);
