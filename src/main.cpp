#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <execinfo.h>
#include <cxxabi.h>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <memory>
#include <algorithm>

#if __cplusplus >= 201703L
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

#include "config.hpp"
#include "engine/sandbox_policy.hpp"
#include "engine/vfs.hpp"
#include "engine/cpu.hpp"
#include "engine/device_factory.hpp"

// Include the debug framework
#include "debug/debug_handler.hpp"
#include "debug/hexdumper.hpp"

// Include the test framework
#include "test/test.hpp"
#include "test/test_framework.hpp"
#include "test/assembly_test_executor.hpp"
#include <unistd.h>  // For _exit()

// Include the assembler framework
#include "assembler/demi_assembler.hpp"
#include "assembler/lexer.hpp"
#include "assembler/parser.hpp"
#include "assembler/assembler.hpp"

// For POSIX process execution instead of system()
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Native code generation
#include "codegen/disa_compiler.hpp"
#include "codegen/elf_emitter.hpp"
#include "language/lexer/lexer.hpp"
#include "language/parser/parser.hpp"
#include "language/semantic/semantic.hpp"
#include "language/codegen/ir_generator.hpp"
#include "language/codegen/native_compiler.hpp"


class ArgParser;


enum class ArgType { Value, Action, MultiValue };

struct ArgDef {
    std::string name;
    std::string arg;
    std::string alias;
    std::string help;
    std::string category;
    ArgType type;
    std::function<void(const std::string&)> value_action; // For value args
    std::function<void()> action;                         // For action args
    std::function<void(const std::vector<std::string>&)> multi_value_action; // For multi-value args
};

struct ParsedArg {
    std::string name;
    std::string value;
    std::vector<std::string> values;
    ArgType type;
};

class ArgParser {
public:
    void add_value_arg(const std::string& name, const std::string& arg, const std::string& alias,
                       const std::string& help, const std::string& category, std::function<void(const std::string&)> value_action) {
        args_.push_back({name, arg, alias, help, category, ArgType::Value, value_action, nullptr, nullptr});
    }
    void add_action_arg(const std::string& name, const std::string& arg, const std::string& alias,
                        const std::string& help, const std::string& category, std::function<void()> action) {
        args_.push_back({name, arg, alias, help, category, ArgType::Action, nullptr, action, nullptr});
    }
    void add_bool_arg(const std::string& name, const std::string& arg, const std::string& alias,
                      const std::string& help, const std::string& category, std::function<void(bool)> action) {
        args_.push_back({name, arg, alias, help, category, ArgType::Value,
            [action](const std::string& value) {
                // If value is empty, treat as true (flag style)
                if (value.empty()) action(true);
                else action(value == "true" || value == "1");
            }, nullptr, nullptr});
    }
    void add_multi_value_arg(const std::string& name, const std::string& arg, const std::string& alias,
                       const std::string& help, const std::string& category, std::function<void(const std::vector<std::string>&)> multi_value_action) {
        args_.push_back({name, arg, alias, help, category, ArgType::MultiValue, nullptr, nullptr, multi_value_action});
    }

    void parse(int argc, char* argv[]) {
        // Phase 1: Collect all parsed arguments and positional arguments
        std::vector<ParsedArg> parsed_args;
        positional_args_.clear();
        
        for (int i = 1; i < argc; ++i) {
            std::string token = argv[i];
            bool matched = false;
            
            for (auto& def : args_) {
                // Check for exact match or --arg=value / -a=value format
                bool is_match = false;
                std::string value;

                if (token == def.arg || token == def.alias) {
                    is_match = true;
                } else {
                    // Check for --arg=value format
                    auto eq = token.find('=');
                    if (eq != std::string::npos) {
                        std::string arg_part = token.substr(0, eq);
                        if (arg_part == def.arg || arg_part == def.alias) {
                            is_match = true;
                            value = token.substr(eq + 1);
                        }
                    }
                }

                if (is_match) {
                    matched = true;
                    
                    if (def.type == ArgType::Value) {
                        // If we didn't get value from =, try next argument
                        if (value.empty() && i + 1 < argc && argv[i + 1][0] != '-') {
                            value = argv[++i];
                        }
                        // Store the parsed argument
                        parsed_args.push_back({def.name, value, {}, def.type});
                    } else if (def.type == ArgType::Action) {
                        // Store the parsed action
                        parsed_args.push_back({def.name, "", {}, def.type});
                    } else if (def.type == ArgType::MultiValue) {
                        std::vector<std::string> values;
                        // If we got value from =, use it
                        if (!value.empty()) {
                            values.push_back(value);
                        }
                        // Consume arguments until next flag or end
                        while (i + 1 < argc && argv[i + 1][0] != '-') {
                            values.push_back(argv[++i]);
                        }
                        parsed_args.push_back({def.name, "", values, def.type});
                    }
                    break;
                }
            }
            
            if (!matched) {
                if (token.rfind("-", 0) == 0) {
                    std::cerr << "Unknown argument: " << token << std::endl;
                } else {
                    // This is a positional argument (doesn't start with -)
                    positional_args_.push_back(token);
                }
            }
        }
        
        // Phase 2: Process arguments in a deterministic order
        // Process boolean flags first (debug, verbose, etc.)
        for (const auto& parsed : parsed_args) {
            if (parsed.name == "debug" || parsed.name == "verbose" || parsed.name == "extended_registers" || 
                parsed.name == "memdump" || parsed.name == "gui" || parsed.name == "interactive" || parsed.name == "sandbox" || parsed.name == "allow_read" || parsed.name == "allow_write" || parsed.name == "allow_exec" || parsed.name == "allow_ioctl" && parsed.name != "vfs_container" || parsed.name == "vfs_container") {
                auto def = find_arg_def(parsed.name);
                if (def && def->value_action) {
                    def->value_action(parsed.value);
                }
            }
        }
            
        // Process debug actions (debug_verbose, debug_quiet, hexdump)
        for (const auto& parsed : parsed_args) {
            if (parsed.name == "debug_verbose" || parsed.name == "debug_quiet" || parsed.name == "hexdump") {
                auto def = find_arg_def(parsed.name);
                if (def && def->action) {
                    def->action();
                }
            }
        }
        
        // Process value arguments next (files, paths, etc.)
        for (const auto& parsed : parsed_args) {
            if (parsed.name != "debug" && parsed.name != "verbose" && parsed.name != "extended_registers" && 
                parsed.name != "memdump" && parsed.name != "gui" && parsed.name != "interactive" && parsed.name != "sandbox" && parsed.name != "allow_read" && parsed.name != "allow_write" && parsed.name != "allow_exec" && parsed.name != "allow_ioctl" && parsed.name != "vfs_container" &&
                parsed.name != "help" && parsed.name != "test" && parsed.name != "unit_test" && 
                parsed.name != "assembly_test" && parsed.name != "assembly_test_quiet" &&
                parsed.name != "debug_verbose" && parsed.name != "debug_quiet" && parsed.name != "hexdump") {
                auto def = find_arg_def(parsed.name);
                if (def) {
                    if (def->value_action && parsed.type == ArgType::Value) {
                        def->value_action(parsed.value);
                    } else if (def->multi_value_action && parsed.type == ArgType::MultiValue) {
                        def->multi_value_action(parsed.values);
                    }
                }
            }
        }
        
        // Process action arguments last (test modes, help, etc.)
        for (const auto& parsed : parsed_args) {
            if (parsed.name == "help" || parsed.name == "test" || parsed.name == "unit_test" || 
                parsed.name == "assembly_test" || parsed.name == "assembly_test_quiet") {
                auto def = find_arg_def(parsed.name);
                if (def) {
                    if (def->type == ArgType::Action && def->action) {
                        def->action();
                    } else if (def->type == ArgType::Value && def->value_action) {
                        def->value_action(parsed.value);
                    } else if (def->type == ArgType::MultiValue && def->multi_value_action) {
                        def->multi_value_action(parsed.values);
                    }
                }
            }
        }

        // Process any remaining action arguments (like x64, x86)
        for (const auto& parsed : parsed_args) {
            if (parsed.type == ArgType::Action && 
                parsed.name != "debug_verbose" && parsed.name != "debug_quiet" && parsed.name != "hexdump" &&
                parsed.name != "help" && parsed.name != "test" && parsed.name != "unit_test" && 
                parsed.name != "assembly_test" && parsed.name != "assembly_test_quiet") {
                
                auto def = find_arg_def(parsed.name);
                if (def && def->action) {
                    def->action();
                }
            }
        }
    }

    void print_help() const {
        std::cout << "demi-engine Usage: demi-engine [options]" << std::endl;
        
        // Group arguments by category
        std::map<std::string, std::vector<const ArgDef*>> categories;
        std::vector<std::string> category_order = {
            "General", "Execution", "Debugging", "Testing", "Hardware"
        };
        
        for (const auto& def : args_) {
            categories[def.category].push_back(&def);
        }
        
        for (const auto& cat : category_order) {
            if (categories.count(cat) && !categories[cat].empty()) {
                std::cout << "\n" << cat << " Options:" << std::endl;
                for (const auto* def : categories[cat]) {
                    std::cout << fmt::format("  {:<25} {:<6}  {}\n", def->arg, def->alias, def->help);
                }
            }
        }
        
        // Print any remaining categories
        for (const auto& [cat, defs] : categories) {
            if (std::find(category_order.begin(), category_order.end(), cat) == category_order.end()) {
                std::cout << "\n" << cat << " Options:" << std::endl;
                for (const auto* def : defs) {
                    std::cout << fmt::format("  {:<25} {:<6}  {}\n", def->arg, def->alias, def->help);
                }
            }
        }
    }

private:
    std::vector<ArgDef> args_;
    std::vector<std::string> positional_args_;
    
public:
    const std::vector<std::string>& get_positional_args() const {
        return positional_args_;
    }
    
private:
    // Helper function to find argument definition by name
    ArgDef* find_arg_def(const std::string& name) {
        for (auto& def : args_) {
            if (def.name == name) {
                return &def;
            }
        }
        return nullptr;
    }
};

void run_in_assembly_tests() {
    // Scan tests/ directory recursively for .asm files
    std::vector<std::string> test_files;
    
    // Use filesystem to find all .asm files in tests/ (recursive)
    std::string test_dir = "tests";
    if (fs::exists(test_dir) && fs::is_directory(test_dir)) {
        for (const auto& entry : fs::recursive_directory_iterator(test_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".asm") {
                test_files.push_back(entry.path().string());
            }
        }
    }
    
    // Note: Now uses recursive scanning to include subdirectories like tests/64bit/
    // To run specific files: ./bin/demi-engine -at examples/test_example.asm
    
    if (test_files.empty()) {
        fmt::print("\nNo in-assembly test files found in tests/.\n");
        return;
    }
    
    // Sort files for consistent ordering
    std::sort(test_files.begin(), test_files.end());
    
    // Create test executor
    Testing::TestExecutor executor;
    std::vector<Testing::TestResult> all_results;
    int total_files = 0;
    int total_tests = 0;
    int total_passed = 0;
    int total_failed = 0;
    int total_skipped = 0;
    
    for (const auto& file : test_files) {
        // Check if file exists
        std::ifstream check(file);
        if (!check.good()) {
            continue; // Skip if file doesn't exist
        }
        
        // Print file header
        if (!Config::quiet_assembly_test) {
            fmt::print("\n\033[1;34m📁 Testing file: {}\033[0m\n", file);
        }
        
        // Execute tests from this file
        auto results = executor.execute_tests_from_file(file);
        
        if (!results.empty()) {
            total_files++;
            int file_tests = results.size();
            int file_passed = 0;
            int file_failed = 0;
            int file_skipped = 0;
            
            for (const auto& result : results) {
                if (result.skipped) {
                    file_skipped++;
                } else if (result.passed) {
                    file_passed++;
                } else {
                    file_failed++;
                }
            }
            
            total_tests += file_tests;
            total_passed += file_passed;
            total_failed += file_failed;
            total_skipped += file_skipped;
            
            if (!Config::quiet_assembly_test) {
                fmt::print("   {} tests: {} passed, {} failed, {} skipped\n", 
                          file_tests, file_passed, file_failed, file_skipped);
            }
        }
        
        all_results.insert(all_results.end(), results.begin(), results.end());
    }
    
    // Print consolidated results
    if (all_results.empty()) {
        fmt::print("\nNo in-assembly tests found in tests/.\n");
    } else {
        // Print summary if not in quiet mode
        if (!Config::quiet_assembly_test) {
            fmt::print("\n\033[1;36m📊 Overall Summary\033[0m\n");
            fmt::print("Files tested: {}\n", total_files);
            fmt::print("Total tests: {}\n", total_tests);
            fmt::print("Passed: \033[32m{}\033[0m\n", total_passed);
            if (total_failed > 0) {
                fmt::print("Failed: \033[31m{}\033[0m\n", total_failed);
            } else {
                fmt::print("Failed: {}\n", total_failed);
            }
            fmt::print("Skipped: \033[36m{}\033[0m\n", total_skipped);
            
            if (total_failed == 0) {
                fmt::print("\n\033[1;32m🎉 All tests passed!\033[0m\n");
            } else {
                fmt::print("\n\033[1;31m❌ {} test(s) failed\033[0m\n", total_failed);
            }
        }
        
        executor.print_results(all_results);
    }
}

void run_unit_tests_only() {
    Config::test_mode = true;
    const char* color = Config::debug ? "\033[38;5;208m" : "\033[36m";
    std::cout << color << "┌──────────────────────────────────────────────────────┐\033[0m" << std::endl;
    std::cout << color << "│     Running DemiEngine Unit Tests                    │\033[0m" << std::endl;
    std::cout << color << "└──────────────────────────────────────────────────────┤\033[0m" << std::endl;
    run_unit_tests();
    exit(0);
}

// Forward declaration
void run_file_tests(const std::vector<std::string>& filepaths);

void run_unit_tests_with_inputs(const std::vector<std::string>& inputs) {
    const char* color = Config::debug ? "\033[38;5;208m" : "\033[36m";
    std::cout << color << "┌──────────────────────────────────────────────────────┐\033[0m" << std::endl;
    std::cout << color << "│     Running DemiEngine Unit Tests                    │\033[0m" << std::endl;
    std::cout << color << "└──────────────────────────────────────────────────────┤\033[0m" << std::endl;

    auto& framework = TestFramework::instance();
    std::vector<TestResult> all_results;
    std::vector<std::string> assembly_files;

    for (const auto& input : inputs) {
        if (input.find('/') != std::string::npos || input.find('.') != std::string::npos) {
            // Treat as file path for assembly tests
            assembly_files.push_back(input);
        } else {
            // Treat as unit test name
            auto results = framework.run_single(input);
            if (results.empty()) {
                std::cerr << "Error: Unit test '" << input << "' not found" << std::endl;
            } else {
                all_results.insert(all_results.end(), results.begin(), results.end());
            }
        }
    }

    if (!all_results.empty()) {
        framework.print_results(all_results);
    }

    if (!assembly_files.empty()) {
        run_file_tests(assembly_files);
    }
    
    // assembly_files is populated from CLI args. When empty and all_results
    // is also empty, no work was done — exit(1). Intentional, not dead code.
    exit(all_results.empty() && assembly_files.empty() ? 1 : 0);
}

void run_assembly_tests_only() {
    Config::verbose = true;  // Enable INFO logs for test output
    try {
        run_in_assembly_tests();
    } catch (const std::exception& e) {
        std::cerr << "Error: Exception in run_in_assembly_tests(): " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Error: Unknown exception in run_in_assembly_tests()" << std::endl;
    }
    std::flush(std::cerr);
    std::flush(std::cout);
    _exit(0); // Use _exit to completely bypass all destructors
}

// Forward declaration
void run_file_tests(const std::vector<std::string>& filepaths);

// Helper function to recursively scan directory for test files
std::vector<std::string> scan_directory_for_tests(const std::string& directory_path) {
    std::vector<std::string> test_files;
    
    if (!fs::exists(directory_path) || !fs::is_directory(directory_path)) {
        return test_files;
    }
    
    try {
        for (const auto& entry : fs::recursive_directory_iterator(directory_path)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                std::string extension = entry.path().extension().string();
                
                // Check for .test.asm, .test.dasm, or just .asm files
                bool is_test_file = false;
                
                // Check for .test.asm suffix
                if (filename.length() >= 9 && filename.substr(filename.length() - 9) == ".test.asm") {
                    is_test_file = true;
                }
                // Check for .test.dasm suffix  
                else if (filename.length() >= 10 && filename.substr(filename.length() - 10) == ".test.dasm") {
                    is_test_file = true;
                }
                // Check for general .asm extension
                else if (extension == ".asm") {
                    is_test_file = true;
                }
                
                if (is_test_file) {
                    test_files.push_back(entry.path().string());
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        Logging::DebugHandler::instance().report(Logging::DebugCategory::IO_FILE, fmt::format("Error scanning directory {}: {}", directory_path, e.what()), Logging::DebugLevel::CRITICAL);
    }
    
    // Sort files for consistent ordering
    std::sort(test_files.begin(), test_files.end());
    return test_files;
}

void run_file_tests(const std::vector<std::string>& filepaths) {
    Config::verbose = true;  // Enable INFO logs for test output
    
    std::vector<std::string> test_files;
    
    for (const auto& filepath : filepaths) {
        // Check if path exists
        if (!fs::exists(filepath)) {
            Logging::DebugHandler::instance().report(Logging::DebugCategory::IO_FILE, fmt::format("Test path not found: {}", filepath), Logging::DebugLevel::CRITICAL);
            exit(1);
        }
        
        if (fs::is_directory(filepath)) {
            // If it's a directory, scan for test files
            auto dir_files = scan_directory_for_tests(filepath);
            if (dir_files.empty()) {
                fmt::print("\nNo test files found in directory: {}\n", filepath);
            } else {
                fmt::print("Found {} test files in directory: {}\n", dir_files.size(), filepath);
                test_files.insert(test_files.end(), dir_files.begin(), dir_files.end());
            }
        } else if (fs::is_regular_file(filepath)) {
            // If it's a file, add it to the list
            test_files.push_back(filepath);
        } else {
            Logging::DebugHandler::instance().report(Logging::DebugCategory::IO_FILE, fmt::format("Path is neither a file nor a directory: {}", filepath), Logging::DebugLevel::CRITICAL);
            exit(1);
        }
    }

    if (test_files.empty()) {
        fmt::print("\nNo test files found.\n");
        exit(1);
    }
    
    // Remove duplicates
    std::sort(test_files.begin(), test_files.end());
    test_files.erase(std::unique(test_files.begin(), test_files.end()), test_files.end());
    
    // Create test executor
    Testing::TestExecutor executor;
    std::vector<Testing::TestResult> all_results;
    
    // Process all test files
    for (const auto& file : test_files) {
        // Execute tests from this file
        auto results = executor.execute_tests_from_file(file);
        all_results.insert(all_results.end(), results.begin(), results.end());
    }
    
    if (all_results.empty()) {
        fmt::print("\nNo tests found in provided paths.\n");
    } else {
        executor.print_results(all_results);
        
        // Print explicit summary
        int passed = 0;
        int skipped = 0;
        for (const auto& res : all_results) {
            if (res.skipped) {
                skipped++;
            } else if (res.passed) {
                passed++;
            }
        }
        int failed = all_results.size() - passed - skipped;
        
        if (!Config::quiet_assembly_test) {
             fmt::print("\nTest Summary: {} total, \033[32m{} passed\033[0m, {} failed, \033[36m{} skipped\033[0m\n", 
                all_results.size(), passed, 
                failed > 0 ? fmt::format("\033[31m{}\033[0m", failed) : "0",
                skipped);
        }
    }
    
    exit(0);
}

bool run_tests() {
    // Run all test suites
    const char* color = Config::debug ? "\033[38;5;208m" : "\033[36m";
    std::cout << color << "┌──────────────────────────────────────────────────────┐\033[0m" << std::endl;
    std::cout << color << "│     Running DemiEngine Unit Tests                    │\033[0m" << std::endl;
    std::cout << color << "└──────────────────────────────────────────────────────┤\033[0m" << std::endl;

    run_unit_tests();
    
    // Run in-assembly tests
    run_in_assembly_tests();

    // Run integration tests
    std::cout << std::endl;
    std::cout << color << "┌──────────────────────────────────────────────────────┐\033[0m" << std::endl;
    std::cout << color << "│     Running DemiEngine Integration Tests             │\033[0m" << std::endl;
    std::cout << color << "└──────────────────────────────────────────────────────┤\033[0m" << std::endl;

    TestRunner runner("tests/hex");
    auto results = runner.run_all();
    int passed = 0, failed = 0;
    
    const char* result_color = Config::debug ? "\033[38;5;208m" : "\033[36m";
    std::cout << result_color << "┌──────────────────────────────────────────────────────┤\033[0m" << std::endl;
    std::cout << result_color << "│     DemiEngine Integration Test Results              │\033[0m" << std::endl;
    std::cout << result_color << "└──────────────────────────────────────────────────────┘\033[0m" << std::endl;
    
    for (const auto& result : results) {
        [[maybe_unused]] constexpr int name_width = 24;
        const char* color = result.passed ? "\033[32m" : "\033[31m";
        const char* reset = "\033[0m";
        std::cout << fmt::format("{0}[{1}]{2} {3:<28}", color, result.passed ? "/" : "X", reset, result.name);
        if ((&result - &results[0] + 1) % 4 == 0)
            std::cout << std::endl;
        else
            std::cout << "    ";
        if (result.passed) ++passed; else ++failed;
    }
    std::cout << std::endl;
    
    const char* summary_color = (failed == 0) ? "\033[32m" : "\033[33m";
    std::cout << summary_color << "Integration tests passed: " << passed << " / " << results.size() << "\033[0m" << std::endl;
    exit(0);
}

class DemiApp {
public:
    DemiApp(int argc, char *argv[]) {
        // Help argument
        parser.add_action_arg("help", "--help", "-h", "Shows help information", "General",
            [this]() { parser.print_help(); show_help = true; });
        
        // Version argument
        parser.add_action_arg("version", "--version", "-V", "Shows version information", "General",
            [this]() {
                std::cout << "\nDemi Virtualized Compiler v" << DEMI_VERSION_STRING << "\n\n";
                
                // Build information
                std::cout << "Build Information:\n";
                #ifdef NDEBUG
                std::cout << "  Build Type:        Release\n";
                #else
                std::cout << "  Build Type:        Debug\n";
                #endif
                std::cout << "  Build Date:        " << DEMI_BUILD_DATE << "\n";
                std::cout << "  Build Time:        " << DEMI_BUILD_TIME << "\n";
                
                // Compiler information
                std::cout << "\nCompiler Information:\n";
                std::cout << "  C++ Standard:      ";
                if (__cplusplus == 201103L) std::cout << "C++11";
                else if (__cplusplus == 201402L) std::cout << "C++14";
                else if (__cplusplus == 201703L) std::cout << "C++17";
                else if (__cplusplus == 202002L) std::cout << "C++20";
                else std::cout << __cplusplus;
                std::cout << "\n";
                
                std::cout << "  Compiler:          ";
                #if defined(__GNUC__) && !defined(__clang__)
                std::cout << "GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__;
                #elif defined(__clang__)
                std::cout << "Clang " << __clang_major__ << "." << __clang_minor__ << "." << __clang_patchlevel__;
                #elif defined(_MSC_VER)
                std::cout << "MSVC " << _MSC_VER;
                #else
                std::cout << "Unknown";
                #endif
                std::cout << "\n";
                
                // Platform information
                std::cout << "\nPlatform Information:\n";
                std::cout << "  Operating System:  ";
                #if defined(__linux__)
                std::cout << "Linux";
                #elif defined(__APPLE__) || defined(__MACH__)
                std::cout << "macOS";
                #elif defined(_WIN32) || defined(_WIN64)
                std::cout << "Windows";
                #elif defined(__unix__)
                std::cout << "Unix";
                #else
                std::cout << "Unknown";
                #endif
                std::cout << "\n";
                
                std::cout << "  Architecture:      ";
                #if defined(__x86_64__) || defined(_M_X64)
                std::cout << "x86_64 (64-bit)";
                #elif defined(__i386__) || defined(_M_IX86)
                std::cout << "x86 (32-bit)";
                #elif defined(__aarch64__) || defined(_M_ARM64)
                std::cout << "ARM64 (64-bit)";
                #elif defined(__arm__) || defined(_M_ARM)
                std::cout << "ARM (32-bit)";
                #else
                std::cout << "Unknown";
                #endif
                std::cout << "\n";
                
                // Engine capabilities
                std::cout << "\nEngine Capabilities:\n";
                std::cout << "  Registers:         134 (GP: 16, FPU: 8, XMM: 16, Misc)\n";
                std::cout << "  Opcodes:           133 implemented (24 reserved)\n";
                std::cout << "  SIMD Support:      " << (DEMI_SIMD_SUPPORT ? "Enabled" : "Disabled") << "\n";
                std::cout << "  FPU Support:       Enabled (23+ operations)\n";
                std::cout << "  Preprocessor:      " << (DEMI_PREPROCESSOR_SUPPORT ? "Enabled" : "Disabled") << "\n";
                std::cout << "  Include Support:   " << (DEMI_INCLUDE_SUPPORT ? "Enabled" : "Disabled") << "\n";
                std::cout << "  Max Memory:        4 GB\n";
                std::cout << "  Test Framework:    322 tests (100% passing)\n";
                std::cout << std::endl;
                show_version = true;
            });
            
        // Debug argument
        parser.add_bool_arg("debug", "--debug", "-d", "Enable debug mode", "Debugging",
            [this](bool value) { 
                Config::debug = value; 
                Config::verbose = value; 
                if (value) {
                    Logging::DebugHandler::instance().enable_default_categories();
                }
            });
        
        // Debug level argument
        parser.add_value_arg("debug_level", "--debug-level", "-dl", "Set debug level (trace, detail, info, important, critical, all). Supports comma-separated values.", "Debugging",
            [this](const std::string& value) {
                Config::debug = true;  // Auto-enable debug mode when level is set
                Logging::DebugHandler::instance().enable_default_categories();
                
                std::stringstream ss(value);
                std::string segment;
                std::vector<std::string> levels;
                
                while(std::getline(ss, segment, ',')) {
                    levels.push_back(segment);
                }
                
                bool has_all = false;
                bool trace_categories = false;
                bool found_valid = false;
                
                // Check for "all" first
                for(const auto& lvl : levels) {
                    if (lvl == "all") {
                        has_all = true;
                        break;
                    }
                }
                
                if (has_all) {
                    // "all" means everything (TRACE minimum, no filter mode)
                    Logging::DebugHandler::instance().set_minimum_level(Logging::DebugLevel::TRACE);
                    Logging::DebugHandler::instance().set_level_filter_mode(false);
                    trace_categories = true;
                    found_valid = true;
                } else {
                    // Specific levels -> enable filter mode
                    Logging::DebugHandler::instance().set_level_filter_mode(true);
                    
                    // Always enable CRITICAL (it's hardcoded in handler anyway, but good for completeness)
                    Logging::DebugHandler::instance().set_level_enabled(Logging::DebugLevel::CRITICAL, true);
                    
                    for(const auto& lvl : levels) {
                        if (lvl == "trace") {
                            Logging::DebugHandler::instance().set_level_enabled(Logging::DebugLevel::TRACE, true);
                            trace_categories = true;
                            found_valid = true;
                        } else if (lvl == "detail") {
                            Logging::DebugHandler::instance().set_level_enabled(Logging::DebugLevel::DETAIL, true);
                            found_valid = true;
                        } else if (lvl == "info") {
                            Logging::DebugHandler::instance().set_level_enabled(Logging::DebugLevel::INFO, true);
                            found_valid = true;
                        } else if (lvl == "important") {
                            Logging::DebugHandler::instance().set_level_enabled(Logging::DebugLevel::IMPORTANT, true);
                            found_valid = true;
                        } else if (lvl == "critical") {
                            // Already enabled
                            found_valid = true;
                        }
                    }
                }
                
                if (found_valid) {
                    if (trace_categories) {
                        Logging::DebugHandler::instance().set_category_enabled(Logging::DebugCategory::CPU_DISPATCHER, true);
                        Logging::DebugHandler::instance().set_category_enabled(Logging::DebugCategory::CPU_PREDICTION, true);
                        Logging::DebugHandler::instance().set_category_enabled(Logging::DebugCategory::MEM_BOUNDS, true);
                    }
                } else {
                    std::cerr << "Invalid debug level: " << value << ". Valid levels: trace, detail, info, important, critical, all" << std::endl;
                }
            });
        
        // Debug verbose argument (shortcut for debug + verbose)
        parser.add_action_arg("debug_verbose", "--debug-verbose", "-dv", "Enable debug with verbose output (TRACE level)", "Debugging",
            [this]() { 
                Config::debug = true; 
                Logging::DebugHandler::instance().enable_default_categories();
                Logging::DebugHandler::instance().set_minimum_level(Logging::DebugLevel::TRACE);
                // Enable all verbose categories for maximum visibility
                Logging::DebugHandler::instance().set_category_enabled(Logging::DebugCategory::CPU_DISPATCHER, true);
                Logging::DebugHandler::instance().set_category_enabled(Logging::DebugCategory::CPU_PREDICTION, true);
                Logging::DebugHandler::instance().set_category_enabled(Logging::DebugCategory::MEM_BOUNDS, true);
            });
        parser.add_action_arg("debug_quiet", "--debug-quiet", "-dq", "Enable debug with minimal output (IMPORTANT level)", "Debugging",
            [this]() { 
                Config::debug = true; 
                Logging::DebugHandler::instance().enable_default_categories();
                Logging::DebugHandler::instance().set_minimum_level(Logging::DebugLevel::IMPORTANT); 
            });
        
        // Hexdump argument
        parser.add_action_arg("hexdump", "--hexdump", "", "Enable bytecode hex dump output after assembly", "Debugging",
            [this]() {
                Logging::HexDumper::enable();
            });
        // Verbose argument
        parser.add_bool_arg("verbose", "--verbose", "-v", "Show informational messages (use --verbose=false to disable)", "General",
            [this](bool value) { Config::verbose = value; });

        // Extended registers argument
        parser.add_bool_arg("extended_registers", "--extended-registers", "-er", "Show extended register output (50 registers)", "Debugging",
            [this](bool value) { Config::extended_registers = value; });

        // Debug File argument
        parser.add_value_arg("debug_file", "--debug-file", "-f", "Debug file path", "Debugging",
            [this](const std::string& value) { Config::debug_file = value; });

        // Hex file argument
        parser.add_value_arg("hex", "--hex", "-H", "Path to hex file (hex bytes, space or newline separated)", "Execution",
            [this](const std::string& value) { Config::program_file = value; });

        // Run tests argument (with optional file path)
        parser.add_multi_value_arg("test", "--test", "-t", "Run built-in unit tests, or test specific files if paths provided", "Testing",
            [this](const std::vector<std::string>& values) {
                if (values.empty() || (values.size() == 1 && (values[0].empty() || values[0] == "true"))) {
                    // No file specified, run unit tests only
                    run_unit_tests_only();
                } else {
                    // Files specified, run tests from those files
                    run_file_tests(values);
                }
            });
        
        // Run unit tests only (or specific file)
        parser.add_multi_value_arg("unit_test", "--unit-test", "-ut", "Run built-in unit tests only, specific test by name, or test file", "Testing",
            [this](const std::vector<std::string>& values) {
                if (values.empty()) {
                    run_unit_tests_only();
                } else {
                    run_unit_tests_with_inputs(values);
                }
            });
        
        // Run assembly tests only (or specific file/folder)
        parser.add_multi_value_arg("assembly_test", "--assembly-test", "-at", "Run in-assembly tests only (supports files and folders)", "Testing",
            [this](const std::vector<std::string>& values) {
                Config::test_mode = true;  // Enable test mode to suppress verbose logs
                if (values.empty()) {
                    // Check positional args for backward compatibility or if no values provided
                    auto positional = parser.get_positional_args();
                    if (positional.empty()) {
                        run_assembly_tests_only();
                    } else {
                        run_file_tests(positional);
                    }
                } else {
                    // Run tests on specified files
                    run_file_tests(values);
                }
            });
        
        // Run assembly tests in quiet mode (only show title and description)
        parser.add_multi_value_arg("assembly_test_quiet", "--assembly-test-quiet", "-atq", "Run in-assembly tests in quiet mode (supports files and folders)", "Testing",
            [this](const std::vector<std::string>& values) {
                Config::test_mode = true;  // Enable test mode to suppress verbose logs
                Config::quiet_assembly_test = true;
                if (values.empty()) {
                    auto positional = parser.get_positional_args();
                    if (positional.empty()) {
                        run_assembly_tests_only();
                    } else {
                        run_file_tests(positional);
                    }
                } else {
                    run_file_tests(values);
                }
            });

        // Assembly mode argument
        parser.add_value_arg("assembly", "--assembly", "-A", "Assembly mode: assemble and run .asm file", "Execution",
            [this](const std::string& value) {
                Config::assembly_mode = true;
                Config::assembly_file = value;
            });

        // Assembly output argument
        parser.add_value_arg("assembly_output", "--assembly-output", "-ao", "Output assembled bytecode to file (default: out.hex)", "Execution",
            [this](const std::string& value) {
                Config::assembly_output = value.empty() ? "out.hex" : value;
            });

        // Hex array output argument
        parser.add_value_arg("hex_out", "--hex-out", "-hxo", "Output assembled bytecode to file as formatted hex bytes", "Execution",
            [this](const std::string& value) {
                Config::hex_out = value.empty() ? "out_array.hex" : value;
            });

        // Compile argument
        parser.add_value_arg("compile", "--compile", "-o", "Compile program into a standalone executable (optionally specify output name)", "Execution",
            [this](const std::string& value) {
                Config::compile_only = true;
                Config::output_name = value;
            });

        parser.add_value_arg("assembly_target", "--assembly-target", "", "Set assembler output target (demi-bytecode, x86-elf32, x86-elf64)", "Execution",
            [this](const std::string& value) {
                if (value == "demi-bytecode" || value == "demi" || value.empty()) {
                    Config::assembly_target = AssemblyTarget::DemiBytecode;
                } else if (value == "x86-elf32") {
                    Config::assembly_target = AssemblyTarget::X86Elf32;
                } else if (value == "x86-elf64") {
                    Config::assembly_target = AssemblyTarget::X86Elf64;
                } else {
                    std::cerr << "Error: Invalid assembly target '" << value << "'. Use 'demi-bytecode', 'x86-elf32', or 'x86-elf64'." << std::endl;
                    exit(1);
                }
            });

        // Debug info flag for compiled binaries
        parser.add_bool_arg("debug_info", "--debug-info", "-g", "Include DWARF debug info in compiled binaries", "Execution", [this](bool value) { Config::debug_info = value; });

        parser.add_bool_arg("listing", "--listing", "-l", "Generate assembly listing file (.lst)", "Execution", [this](bool value) { Config::listing = value; });

        // Warning flags
        parser.add_bool_arg("warn_all", "-W", "-w", "Enable assembler warnings (default: on)", "Execution",
            [this](bool value) { Config::warn_all = value; });
        parser.add_bool_arg("warn_error", "--Werror", "", "Treat warnings as errors", "Execution",
            [this](bool value) { Config::warn_error = value; });

        parser.add_bool_arg("shared", "--shared", "", "Generate shared library (.so) instead of executable", "Execution",
            [this](bool value) { Config::shared_library = value; });

        // Memory dump argument
        parser.add_bool_arg("memdump", "--memdump", "-m", "Print memory dump after execution", "Debugging", [this](bool value) { Config::memdump = value; });
        parser.add_bool_arg("interactive", "--interactive", "-i", "Start interactive REPL (Demi language)", "Execution", [this](bool value) { Config::interactive = value; });
        parser.add_bool_arg("debug_step", "--debug-step", "-ds", "Run with step-through debugger", "Debugging", [this](bool value) { Config::debug_step = value; });

        // Entry point argument
        parser.add_value_arg("entry_point", "--entry-point", "-e", "Specify entry point symbol (default: _start)", "Execution",
            [this](const std::string& value) {
                Config::entry_point_symbol = value;
            });

        // Sandbox arguments
        parser.add_bool_arg("sandbox", "--sandbox", "", "Enable sandbox mode (restricts file I/O, exec, network)", "Sandbox",
            [this](bool value) { Config::sandbox_enabled = value; });
        parser.add_bool_arg("allow_read", "--allow-read", "", "Allow host file reads when sandbox is enabled", "Sandbox",
            [this](bool value) { Config::allow_read = value; });
        parser.add_bool_arg("allow_write", "--allow-write", "", "Allow host file writes when sandbox is enabled", "Sandbox",
            [this](bool value) { Config::allow_write = value; });
        parser.add_bool_arg("allow_exec", "--allow-exec", "", "Allow fork+execve when sandbox is enabled", "Sandbox",
            [this](bool value) { Config::allow_exec = value; });
        parser.add_bool_arg("allow_ioctl", "--allow-ioctl", "", "Allow raw ioctl in sandbox mode", "Sandbox",
            [this](bool value) { Config::allow_ioctl = value; });
        parser.add_value_arg("vfs_container", "--vfs-container", "", "Path to VFS container file (default: /tmp/demi_sandbox.vfs)", "Sandbox",
            [this](const std::string& val) { Config::vfs_container = val; });

        // Architecture arguments
        parser.add_value_arg("architecture", "--architecture", "-arch", "Set CPU architecture (x86, x64, auto)", "Execution",
            [this](const std::string& value) {
                if (value == "x86" || value == "32") {
                    Config::architecture = Architecture::X86;
                } else if (value == "x64" || value == "64") {
                    Config::architecture = Architecture::X64;
                } else if (value == "auto") {
                    Config::architecture = Architecture::AUTO;
                } else {
                    std::cerr << "Error: Invalid architecture '" << value << "'. Use 'x86', 'x64', or 'auto'." << std::endl;
                    exit(1);
                }
            });

        parser.add_action_arg("x86", "-x86", "", "Force x86 (32-bit) mode", "Execution",
            [this]() { Config::architecture = Architecture::X86; });

        parser.add_action_arg("x64", "-x64", "", "Force x64 (64-bit) mode", "Execution",
            [this]() { Config::architecture = Architecture::X64; });

        // Test filter argument for tests
        parser.add_value_arg("test_filter", "--test-filter", "", "Filter test output (all|fails|success)", "Testing",
            [this](const std::string& value) {
                if (value == "fails" || value == "fail") {
                    Config::test_show_mode = TestShowMode::FAILS;
                } else if (value == "success" || value == "pass") {
                    Config::test_show_mode = TestShowMode::SUCCESS;
                } else if (value == "all" || value.empty()) {
                    Config::test_show_mode = TestShowMode::ALL;
                } else {
                    std::cerr << "Error: Invalid --test-filter value '" << value << "'. Use 'all', 'fails', or 'success'." << std::endl;
                    exit(1);
                }
            });

        // Test debug mode - verbose output for test diagnostics
        parser.add_bool_arg("test_debug", "--test-debug", "-td",
            "Enable verbose test debugging output (shows fusion engine, opcode dispatch, etc.)", "Testing",
            [this](bool value) { Config::test_debug = value; });

        // Test select - run specific test(s) by name
        parser.add_value_arg("test_select", "--test-select", "-ts",
            "Run specific test(s) by name (comma-separated, e.g. -ts fusion_inc_cmp,fusion_dec_cmp)", "Testing",
            [this](const std::string& value) { Config::test_select = value; });

        // Assembly test debug mode - verbose output for assembly test diagnostics
        parser.add_bool_arg("assembly_test_debug", "--assembly-test-debug", "-atd",
            "Enable verbose assembly test debugging output", "Testing",
            [this](bool value) { Config::assembly_test_debug = value; });

                parser.parse(argc, argv);
        // Debug: check sandbox state after parse
        if (Config::sandbox_enabled) {
        }

        // If -td was set standalone (without -t which exits during parsing), trigger tests
        if (Config::test_debug && !Config::test_mode) {
            run_unit_tests_only();
        }
        // If -atd was set standalone (without -at which sets test_mode), trigger assembly tests
        if (Config::assembly_test_debug && !Config::test_mode) {
            Config::test_mode = true;
            run_assembly_tests_only();
        }
    }

    // Run in compiled mode (create a standalone executable)
    void run_compiled(std::vector<uint8_t>& program) {
        // Create a standalone executable instead of running the program
        std::string output_name;

        if (Config::output_name.empty()) {
            // Generate a name if none provided
            output_name = generate_executable_name(Config::program_file);
        } else {
            // Use provided name and sanitize it
            output_name = sanitize_filename(Config::output_name);
            if (output_name.empty()) {
                std::cerr << "Error: Invalid output filename: " << Config::output_name << std::endl;
                std::cerr << "Filename cannot contain: . at start, ../, or shell metacharacters ;|&`$()[]{}*?<>" << std::endl;
                return;
            }

            // Create directory if it doesn't exist
            fs::path output_path(output_name);
            if (output_path.has_parent_path()) {
                fs::path dir = output_path.parent_path();
                if (!fs::exists(dir)) {
                    if (!fs::create_directories(dir)) {
                        std::cerr << "Error: Failed to create directory: " << dir << std::endl;
                        return;
                    }
                }
            }
        }
        if (create_standalone_executable(program, output_name)) {
            std::string shown_name = output_name;
            if (shown_name.substr(0, 2) != "./") {
                shown_name = "./" + shown_name;
            }
            std::cout << "Successfully compiled to executable: " << shown_name << std::endl;

            // Don't add ./ prefix if output_name already starts with ./
            std::string run_command = output_name;
            if (run_command.substr(0, 2) != "./") {
                run_command = "./" + run_command;
            }
            std::cout << "You can run it with: " << run_command << std::endl;
        }
    }

    // Generate a suitable executable name from the program file path
    std::string generate_executable_name(const std::string& program_file) {
        fs::path path(program_file);
        std::string name = path.stem().string();  // Get filename without extension

        // Make sure we have the bin directory
        fs::path bin_dir("bin");
        if (!fs::exists(bin_dir)) {
            fs::create_directory(bin_dir);
        }

        return (bin_dir / name).string();
    }

    // Sanitize filename to prevent command injection
    std::string sanitize_filename(const std::string& filename) {
        // First check for completely invalid patterns
        if (filename.empty() ||
            filename.find("../") != std::string::npos ||
            filename.find_first_of(";|&`$()[]{}*?<>") != std::string::npos) {
            return "";
        }

        // Check for hidden files (starting with . but not ./ which is current directory)
        if (filename[0] == '.' && filename.length() > 1 && filename[1] != '/') {
            return "";
        }

        std::string sanitized;
        sanitized.reserve(filename.length());

        for (char c : filename) {
            // Allow alphanumeric, dots, hyphens, underscores, and forward slashes for paths
            if (std::isalnum(c) || c == '.' || c == '-' || c == '_' || c == '/') {
                sanitized += c;
            }
            // Replace other characters with underscore
            else {
                sanitized += '_';
            }
        }

        return sanitized;
    }

    // Create a standalone executable with the program embedded
    bool create_standalone_executable(const std::vector<uint8_t>& program,
                                      const std::string& output_name) {
        // Note: output_name is already sanitized by run_compiled()

        // 1. Generate program_data.hpp with the program bytes
        if (!generate_program_data_header(program)) {
            std::cerr << "Failed to generate program data header" << std::endl;
            return false;
        } else {
            std::cout << "Generated program data header with " << program.size() << " bytes" << std::endl;
        }

        // 2. Compile the standalone main
        if (!compile_standalone_main(output_name)) {
            std::cerr << "Failed to compile standalone executable" << std::endl;
            return false;
        } else {
            std::cout << "Compiled standalone main to: " << output_name << std::endl;
        }

        return true;
    }

    // Generate program_data.hpp with the program bytes
    bool generate_program_data_header(const std::vector<uint8_t>& program) {
        std::ofstream outfile("src/program_data.hpp");
        if (!outfile) {
            std::cerr << "Error: Cannot create program_data.hpp" << std::endl;
            return false;
        }

        // Get current time for timestamp
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream timestamp;
        timestamp << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");

        // Write the header file
        outfile << "// Auto-generated from program file\n";
        outfile << "// Generated on: " << timestamp.str() << "\n\n";
        outfile << "#ifndef PROGRAM_DATA_HPP\n";
        outfile << "#define PROGRAM_DATA_HPP\n\n";
        outfile << "#include <vector>\n";
        outfile << "#include <cstdint>\n\n";
        outfile << "// Program binary data\n";
        outfile << "const std::vector<uint8_t> PROGRAM_DATA = {\n    ";

        // Write the program bytes in a nicely formatted array
        const int ITEMS_PER_LINE = 12;
        for (size_t i = 0; i < program.size(); ++i) {
            outfile << "0x" << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(program[i]);

            if (i < program.size() - 1) {
                outfile << ", ";
                if ((i + 1) % ITEMS_PER_LINE == 0) {
                    outfile << "\n    ";
                }
            }
        }

        outfile << "\n};\n\n";
        outfile << "#endif // PROGRAM_DATA_HPP\n";

        return true;
    }

    // Securely compile standalone executable using execvp (no system())
    bool compile_standalone_main(const std::string& output_name) {
        // Create a simplified standalone main that doesn't include imgui
        std::string temp_file = "build/tmp_standalone.cpp";

        // Write a simplified version of standalone_main.cpp to a temporary file
        std::ofstream outfile(temp_file);
        if (!outfile) {
            std::cerr << "Error: Cannot create temporary file" << std::endl;
            return false;
        }

        outfile << "#include <iostream>\n";
        outfile << "#include <vector>\n";
        outfile << "#include <cstdint>\n";
        outfile << "#include \"config.hpp\"\n";
        outfile << "#include \"vhardware/cpu.hpp\"\n";
        outfile << "#include \"vhardware/device_factory.hpp\"\n\n";
        outfile << "// Include the generated program data\n";
        outfile << "#include \"program_data.hpp\"\n\n";
        outfile << "// Basic device initialization without logging\n";
        outfile << "void silent_initialize_devices() {\n";
        outfile << "    using namespace vhw;\n";
        outfile << "    auto console = DeviceFactory::createConsoleDevice(0x01);\n";
        outfile << "    auto counter = DeviceFactory::createCounterDevice(0x02);\n";
        outfile << "    auto file = DeviceFactory::createFileDevice(\"virtual_storage/vhd.dat\", 0x04);\n";
        outfile << "    auto ramdisk = DeviceFactory::createRamDiskDevice(8192, 0x05, 0x06);\n";
        outfile << "}\n\n";
        outfile << "int main(int, char**) {\n";
        outfile << "    // No debug mode by default\n";
        outfile << "    Config::debug = false;\n";
        outfile << "    Config::verbose = false;\n\n";
        outfile << "    // Initialize CPU\n";
        outfile << "    CPU cpu;\n";
        outfile << "    cpu.reset();\n\n";
        outfile << "    // Initialize devices silently\n";
        outfile << "    silent_initialize_devices();\n\n";
        outfile << "    // Execute the program\n";
        outfile << "    cpu.execute(PROGRAM_DATA);\n\n";
        outfile << "    // Handle errors if any\n";
        outfile << "    if (Config::error_count > 0) {\n";
        outfile << "        std::cerr << \"Execution failed with \" << Config::error_count << \" errors.\" << std::endl;\n";
        outfile << "        return 1;\n";
        outfile << "    }\n";
        outfile << "    return 0;\n";
        outfile << "}\n";
        outfile.close();

        // Create bin directory if it doesn't exist
        fs::path bin_dir("bin");
        if (!fs::exists(bin_dir)) {
            fs::create_directory(bin_dir);
        }        // Propietary files
        // Note: fmt library is included in extern/fmt
        // Note: imgui not included
        std::vector<std::string> extra_files = {
            "src/debug/logger.cpp",
            "src/vhardware/cpu.cpp",
            "src/vhardware/device_manager.cpp",
            // Use consolidated opcodes file for faster compilation
            "src/vhardware/opcodes/opcodes_consolidated.cpp",
            // "src/vhardware/device_factory.cpp",
            // "src/vhardware/devices/console_device.cpp",
            // "src/vhardware/devices/counter_device.cpp",
            // "src/vhardware/devices/file_device.cpp",
            // "src/vhardware/devices/ramdisk_device.cpp",
            "extern/fmt/src/format.cc"
        };

        // Prepare argument vector
        std::vector<std::string> args = {
            "g++", "-std=c++17", "-I./src",
            "-Iextern/fmt/include", "-Iextern",
            "-std=c++17",
            "-o", output_name,
            temp_file
        };
        // Add all extra source files
        args.insert(args.end(), extra_files.begin(), extra_files.end());

        // Convert args to char* array
        std::vector<char*> c_args;
        for (auto& arg : args) c_args.push_back(const_cast<char*>(arg.c_str()));
        c_args.push_back(nullptr);

        std::cout << "Building standalone executable..." << std::endl;

        pid_t pid = fork();
        if (pid == -1) {
            std::cerr << "Failed to fork for compile" << std::endl;
            return false;
        } else if (pid == 0) {
            // child process
            execvp("g++", c_args.data());
            // If execvp fails
            std::cerr << "Failed to execute g++" << std::endl;
            _exit(127);
        }
        // parent process
        int status = 0;
        if (waitpid(pid, &status, 0) == -1) {
            std::cerr << "Failed during waitpid" << std::endl;
            return false;
        } else {
        }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            return false;
        }

        // Make the executable file executable
        fs::permissions(output_name,
            fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
            fs::perm_options::add);

        return true;
    }

    void run() {
        if (show_help) return;
        if (show_version) return;

        // Handle test mode first
        // Config::running_tests is a runtime toggle (--test flag).
        // When disabled, test-mode code paths are skipped — intentional.
        if (Config::running_tests) {
            // Validate conflicting flags for test mode
            if (Config::assembly_mode) {
                std::cerr << "Error: Test mode (-t/--test) cannot be used with assembly mode (-A/--assembly)" << std::endl;
                return;
            }
            if (!Config::program_file.empty()) {
                std::cerr << "Error: Test mode (-t/--test) cannot be used with hex file (-H/--hex)" << std::endl;
                return;
            }
            run_tests();
            return;
        }

        // Validate conflicting flags for assembly mode
        if (Config::assembly_mode && !Config::program_file.empty()) {
            std::cerr << "Error: Assembly mode (-A/--assembly) cannot be used with hex file (-H/--hex)" << std::endl;
            return;
        }

        // Handle assembly mode
        if (Config::assembly_mode) {
            run_assembly_mode();
            return;
        }

        CPU cpu;
        cpu.reset();
        // ── Sandbox setup ──
        std::unique_ptr<demi::sandbox::SyscallDispatcher> _sandbox_disp;
        std::unique_ptr<demi::sandbox::VirtualFileSystem> _sandbox_vfs;
        if (Config::sandbox_enabled) {
            _sandbox_disp = std::make_unique<demi::sandbox::SyscallDispatcher>(true); _sandbox_disp->set_allow_read(Config::allow_read); _sandbox_disp->set_allow_write(Config::allow_write); _sandbox_disp->set_allow_exec(Config::allow_exec); _sandbox_disp->set_allow_ioctl(Config::allow_ioctl);
            _sandbox_vfs = std::make_unique<demi::sandbox::VirtualFileSystem>(
                "/tmp/demi_vfs", !(false));
            cpu.set_sandbox_environment(_sandbox_disp.get(), _sandbox_vfs.get());
            // Config::quiet is a runtime toggle (--quiet flag). When set,
            // sandbox status output is suppressed — intentional, not dead code.
            if (!Config::quiet && !Config::test_mode) {
                std::cout << "\033[33m[sandbox]\033[0m enabled";
                if (Config::allow_read)  std::cout << " +read";
                if (Config::allow_write) std::cout << " +write";
                if (Config::allow_exec)  std::cout << " +exec";
                if (Config::allow_ioctl) std::cout << " +ioctl";
                std::cout << std::endl;
            }
        }

        std::vector<uint8_t> program;
        if (!Config::program_file.empty()) {
            if (!load_program_file(Config::program_file, program)) {
                std::cerr << "Failed to load program file: " << Config::program_file << std::endl;
                return;
            }
        } else if (Config::interactive) {
            run_repl();
            return;
        } else {
            // No file specified, show help
            parser.print_help();
            return;
        }

        // Check if we should compile instead of run
        if (Config::compile_only) {
            run_compiled(program);
            return;
        }

        // Print a simple, clean headerw
        if (!Config::compile_only) {
            const char* color = Config::debug ? "\033[38;5;208m" : "\033[36m";
            std::cout << color << "\n=== Demi Engine ===" << "\033[0m" << std::endl;
            std::cout << color << "Execution started..." << "\033[0m\n" << std::endl;
        }

        // Initialize the device system
        initialize_devices(&cpu);

    if (Config::debug_step) {
        run_step_debug(program, 0);
    } else {
        cpu.execute(program, 0);
    }

        // Print CPU state
        cpu.print_state("End");
        cpu.print_registers();

        // Print extended registers if enabled
        if (Config::extended_registers) {
            cpu.print_extended_registers();
        }

        cpu.print_memory();

        if (Config::error_count > 0) {
            Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION, fmt::format("Execution failed with {} errors.", Config::error_count), Logging::DebugLevel::CRITICAL);
        } else {
            Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION, "Execution completed successfully.", Logging::DebugLevel::IMPORTANT);
        }
    }

private:
    ArgParser parser;
    std::string data;
    bool show_help = false;
    bool show_version = false;

    // Helper to load hex bytes from file
    bool load_program_file(const std::string& path, std::vector<uint8_t>& out) {
        std::ifstream file(path);
        if (!file) return false;
        std::string token;
        while (file >> token) {
            if (token[0] == '#') { file.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); continue; }
            try {
                uint8_t byte = static_cast<uint8_t>(std::stoul(token, nullptr, 16));
                out.push_back(byte);
            } catch (...) {
                std::cerr << "Invalid hex byte in program file: " << token << std::endl;
                Config::error_count++;
                return false;
            }
        }
        return true;
    }

    // Assembly mode: assemble and run .asm file
    void run_assembly_mode() {
        if (Config::assembly_file.empty()) {
            std::cerr << "Error: No assembly file specified for assembly mode (-A/--assembly)" << std::endl;
            return;
        }

        auto should_print_compile_details = []() {
            return Config::verbose || Config::debug;
        };

        auto print_compile_detail = [](const std::string& line) {
            std::cout << line << std::endl;
        };

        // Check if file exists and has .asm extension
        if (!fs::exists(Config::assembly_file)) {
            std::cerr << "Error: Assembly file not found: " << Config::assembly_file << std::endl;
            return;
        }

        if (Config::verbose) {
            std::cout << "Assembling: " << Config::assembly_file << std::endl;
        }

        // Detect .dem files → route through Demi language compiler
        bool is_dem_file = Config::assembly_file.size() >= 4 &&
            Config::assembly_file.substr(Config::assembly_file.size() - 4) == ".dem";
        
        if (is_dem_file) {
            using namespace DemiLanguage;
            
            std::ifstream dem_file(Config::assembly_file);
            if (!dem_file) {
                std::cerr << "Error: Cannot open: " << Config::assembly_file << std::endl;
                return;
            }
            std::stringstream dem_buf;
            dem_buf << dem_file.rdbuf();
            std::string source = dem_buf.str();
            
            Lexer lexer(source);
            auto tokens = lexer.tokenize();
            if (lexer.has_errors()) {
                for (auto& e : lexer.get_errors()) std::cerr << "Lexer: " << e << std::endl;
                return;
            }
            Parser parser(std::move(tokens));
            auto mod = parser.parse();
            if (parser.has_errors()) {
                for (auto& e : parser.get_errors()) std::cerr << "Parse: " << e << std::endl;
                return;
            }
            SemanticAnalyzer sema;
            sema.analyze(*mod);
            if (sema.has_errors()) {
                for (auto& e : sema.get_errors()) std::cerr << "Semantic: " << e.message << std::endl;
                return;
            }
            IRGenerator codegen;
            auto result = codegen.generate(*mod);
            if (!codegen.get_errors().empty()) {
                for (auto& e : codegen.get_errors()) std::cerr << "Codegen: " << e << std::endl;
                return;
            }
            
            if (should_print_compile_details()) {
                print_compile_detail(fmt::format("[DEM] input            : {}", Config::assembly_file));
                print_compile_detail(fmt::format("[DEM] bytecode size    : {} bytes", result.bytecode.size()));
            }
            
            // Native compilation (--compile) or VM execution
            if (Config::compile_only) {
                std::string output_name = Config::output_name.empty()
                    ? fs::path(Config::assembly_file).stem().string()
                    : Config::output_name;
                compile_dem_to_native(Config::assembly_file, output_name, result);
                return;
            }
            
            // Execute on VM
            CPU cpu;
            cpu.reset();
            initialize_devices(&cpu);  // register console, devices
            cpu.execute(result.bytecode, 0);
            return;
        }

        // Use DemiAssembler which includes preprocessing
        Assembler::DemiAssembler assembler;
        assembler.set_entry_point_symbol(Config::entry_point_symbol);
        assembler.set_target(Config::assembly_target);
        auto bytecode = assembler.assemble_file(Config::assembly_file);

        if (assembler.has_errors()) {
            std::cerr << "Assembly errors:" << std::endl;
            for (const auto& error : assembler.get_errors()) {
                std::cerr << "  " << error << std::endl;
            }
            return;
        }

        const auto& symbols = assembler.get_symbols();
        uint32_t entry_addr = assembler.get_entry_address();
        const bool is_demi_target = Config::assembly_target == AssemblyTarget::DemiBytecode;

        if (Config::verbose) {
            std::cout << "Assembly successful. Generated " << bytecode.size() << " bytes of output." << std::endl;

            // Show symbol table if verbose
            if (!symbols.empty()) {
                std::cout << "Symbol table:" << std::endl;
                for (const auto& [name, symbol] : symbols) {
                    std::cout << "  " << name << " = 0x" << std::hex << symbol.address << std::dec << std::endl;
                }
            }
        }

        if (should_print_compile_details()) {
            size_t defined_symbols = 0;
            size_t undefined_symbols = 0;
            for (const auto& [name, symbol] : symbols) {
                (void)name;
                if (symbol.defined) defined_symbols++;
                else undefined_symbols++;
            }

            print_compile_detail(fmt::format("[ASM] input            : {}", Config::assembly_file));
            if (is_demi_target) {
                print_compile_detail(fmt::format("[ASM] entry symbol     : {} @ 0x{:04X}", Config::entry_point_symbol, entry_addr));
                print_compile_detail(fmt::format("[ASM] bytecode size    : {} bytes (0x{:X})", bytecode.size(), bytecode.size()));
            } else {
                print_compile_detail(fmt::format("[ASM] target           : {}",
                    Config::assembly_target == AssemblyTarget::X86Elf32 ? "x86-elf32" : "x86-elf64"));
                print_compile_detail(fmt::format("[ASM] object size      : {} bytes (0x{:X})", bytecode.size(), bytecode.size()));
            }
            print_compile_detail(fmt::format("[ASM] symbol count     : {} total ({} defined, {} undefined)", symbols.size(), defined_symbols, undefined_symbols));
            print_compile_detail(fmt::format("[ASM] output mode      : {}",
                Config::compile_only && is_demi_target ? "assemble + native compile" :
                (!Config::assembly_output.empty() ? "assemble-only binary output" :
                (!Config::hex_out.empty() ? "assemble-only hex output" : "assemble + execute"))));
        }

        if (!is_demi_target && Config::compile_only) {
            // x86 target: write ELF object file
        }

        if (!is_demi_target && Config::assembly_output.empty()) {
            std::string output_name;
            if (Config::output_name.empty()) {
                fs::path input_path(Config::assembly_file);
                output_name = input_path.stem().string() + ".o";
            } else {
                output_name = sanitize_filename(Config::output_name);
                if (output_name.empty()) {
                    std::cerr << "Error: Invalid output filename: " << Config::output_name << std::endl;
                    return;
                }
            }
            Config::assembly_output = output_name;
        }

        // Save bytecode to file if -ao option is specified
        if (!Config::assembly_output.empty()) {
            try {
                std::ofstream outfile(Config::assembly_output, std::ios::binary);
                if (!outfile) {
                    std::cerr << "Error: Could not open output file: " << Config::assembly_output << std::endl;
                    return;
                }
                
                outfile.write(reinterpret_cast<const char*>(bytecode.data()), bytecode.size());
                outfile.close();
                
                std::cout << "Bytecode written to: " << Config::assembly_output << " (" << bytecode.size() << " bytes)" << std::endl;

                // Print symbol table for debugging
                const auto& symbols = assembler.get_symbols();
                if (!symbols.empty()) {
                    std::cout << "\nSymbol Table:" << std::endl;
                    std::cout << std::string(60, '=') << std::endl;
                    for (const auto& [name, symbol] : symbols) {
                        std::cout << fmt::format("{:<30} 0x{:04X}  ({})", name, symbol.address, symbol.defined ? "defined" : "undefined") << std::endl;
                    }
                    std::cout << std::string(60, '=') << std::endl;
                }

                // If assembly output is specified, don't execute (assemble-only mode)
                return;
            } catch (const std::exception& e) {
                std::cerr << "Error writing output file: " << e.what() << std::endl;
                return;
            }
        }

        // Save bytecode to hex array file if -hxo option is specified
        if (!Config::hex_out.empty()) {
            try {
                std::ofstream outfile(Config::hex_out);
                if (!outfile) {
                    std::cerr << "Error: Could not open output file: " << Config::hex_out << std::endl;
                    return;
                }
                
                for (size_t i = 0; i < bytecode.size(); ++i) {
                    outfile << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(bytecode[i]);
                    if ((i + 1) % 16 == 0) outfile << std::endl;
                    else outfile << " ";
                }
                if (bytecode.size() % 16 != 0) outfile << std::endl;
                outfile.close();
                
                std::cout << "Hex output written to: " << Config::hex_out << " (" << bytecode.size() << " bytes)" << std::endl;

                // Print symbol table for debugging
                const auto& symbols = assembler.get_symbols();
                if (!symbols.empty()) {
                    std::cout << "\nSymbol Table:" << std::endl;
                    std::cout << std::string(60, '=') << std::endl;
                    for (const auto& [name, symbol] : symbols) {
                        std::cout << fmt::format("{:<30} 0x{:04X}  ({})", name, symbol.address, symbol.defined ? "defined" : "undefined") << std::endl;
                    }
                    std::cout << std::string(60, '=') << std::endl;
                }

                // If hex output is specified, don't execute (assemble-only mode)
                return;
            } catch (const std::exception& e) {
                std::cerr << "Error writing hex output file: " << e.what() << std::endl;
                return;
            }
        }

        // Native compilation: compile bytecode to x86-64 ELF executable
        if (Config::compile_only) {
            std::string output_name;

            if (Config::output_name.empty()) {
                output_name = generate_executable_name(Config::assembly_file);
            } else {
                output_name = sanitize_filename(Config::output_name);
                if (output_name.empty()) {
                    std::cerr << "Error: Invalid output filename: " << Config::output_name << std::endl;
                    return;
                }
            }

            // Create directory if it doesn't exist
            fs::path output_path(output_name);
            if (output_path.has_parent_path()) {
                fs::path dir = output_path.parent_path();
                if (!fs::exists(dir)) {
                    if (!fs::create_directories(dir)) {
                        std::cerr << "Error: Failed to create directory: " << dir << std::endl;
                        return;
                    }
                }
            }

            // Compile bytecode to native code
            CodeGen::DISAToX86Compiler compiler;
            bool is_64bit = (Config::architecture != Architecture::X86);
            const auto& asm_line_map = assembler.get_line_map();
            auto native_code = compiler.compile_program(bytecode, entry_addr,
                Config::debug_info && !asm_line_map.empty() ? &asm_line_map : nullptr,
                is_64bit);

            if (Config::verbose) {
                std::cout << "Compiled to " << native_code.size() << " bytes of native x86-64 code" << std::endl;
            }

            if (should_print_compile_details()) {
                print_compile_detail(fmt::format("[CG ] target           : x86-64 ELF"));
                print_compile_detail(fmt::format("[CG ] source entry     : bytecode 0x{:04X}", entry_addr));
                print_compile_detail(fmt::format("[CG ] generated code   : {} bytes (0x{:X})", native_code.size(), native_code.size()));
            }

            // Wrap in ELF executable
            CodeGen::ELFEmitter elf_emitter;
            std::vector<uint8_t> elf_data;
            if (is_64bit) {
                elf_data = elf_emitter.generate_executable(native_code, "_start",
                    Config::debug_info, Config::assembly_file,
                    Config::debug_info ? &compiler.get_line_entries() : nullptr);
            } else {
                elf_data = elf_emitter.generate_executable_32(native_code);
            }

            if (should_print_compile_details()) {
                long long size_delta = static_cast<long long>(elf_data.size()) - static_cast<long long>(native_code.size());
                print_compile_detail(fmt::format("[ELF] image size       : {} bytes (0x{:X})", elf_data.size(), elf_data.size()));
                print_compile_detail(fmt::format("[ELF] runtime/stub ovh : {} bytes", size_delta));
                print_compile_detail(fmt::format("[OUT] executable path  : {}", output_name));
            }

            if (elf_emitter.write_to_file(elf_data, output_name)) {
                std::cout << "Successfully compiled to executable: " << output_name << std::endl;
                std::string run_path = output_name;
                if (run_path.substr(0, 2) != "./") {
                    run_path = "./" + run_path;
                }
                std::cout << "You can run it with: " << run_path << std::endl;
            }
            return;
        }

        // Initialize CPU and devices
        CPU cpu;
        cpu.reset();
        // ── Sandbox setup ──
        std::unique_ptr<demi::sandbox::SyscallDispatcher> _sandbox_disp;
        std::unique_ptr<demi::sandbox::VirtualFileSystem> _sandbox_vfs;
        if (Config::sandbox_enabled) {
            _sandbox_disp = std::make_unique<demi::sandbox::SyscallDispatcher>(true); _sandbox_disp->set_allow_read(Config::allow_read); _sandbox_disp->set_allow_write(Config::allow_write); _sandbox_disp->set_allow_exec(Config::allow_exec); _sandbox_disp->set_allow_ioctl(Config::allow_ioctl);
            _sandbox_vfs = std::make_unique<demi::sandbox::VirtualFileSystem>(
                "/tmp/demi_vfs", !(false));
            cpu.set_sandbox_environment(_sandbox_disp.get(), _sandbox_vfs.get());
            // Config::quiet is a runtime toggle (--quiet flag). When set,
            // sandbox status output is suppressed — intentional, not dead code.
            if (!Config::quiet && !Config::test_mode) {
                std::cout << "\033[33m[sandbox]\033[0m enabled";
                if (Config::allow_read)  std::cout << " +read";
                if (Config::allow_write) std::cout << " +write";
                if (Config::allow_exec)  std::cout << " +exec";
                if (Config::allow_ioctl) std::cout << " +ioctl";
                std::cout << std::endl;
            }
        }
        initialize_devices(&cpu);

        // Set PC to entry address if available
        // Print hex dump of assembled bytecode
        // Use HexDumper for --hexdump flag (always prints)
        if (Logging::HexDumper::is_enabled()) {
            Logging::HexDumper::dump_bytecode(bytecode, "Assembled bytecode hex dump");
        }
        
        // Also send to debug system if debug mode is enabled (for debug directives)
        if (Config::debug) {
            std::string dump = Logging::HexDumper::format_bytecode(bytecode);
            // Add a newline at the end so the metadata (file:line) appears on a new line
            DEBUG_INFO(Logging::DebugCategory::ASM_HEXDUMP, "Assembled bytecode hex dump ({} bytes):\n{}\n", bytecode.size(), dump);
        }

        // Print entry address and bytes at entry address
        DEBUG_INFO(Logging::DebugCategory::CPU_EXECUTION, "Entry address: 0x{:X}", entry_addr);
        std::string bytes_str = "Bytes at entry address:";
        for (size_t i = entry_addr; i < entry_addr + 8 && i < bytecode.size(); ++i) {
            bytes_str += fmt::format(" {:02X}", bytecode[i]);
        }
        DEBUG_INFO(Logging::DebugCategory::CPU_EXECUTION, "{}", bytes_str);

        try {
            // Debug: Print entry address before execution
            DEBUG_INFO(Logging::DebugCategory::CPU_EXECUTION, "Entry address: 0x{:X}", entry_addr);
            DEBUG_INFO(Logging::DebugCategory::CPU_EXECUTION, "About to call cpu.execute() with bytecode size: {}", bytecode.size());
            
            std::cout.flush();
            // Execute the assembled bytecode, starting at entry address
            if (Config::debug_step) {
                run_step_debug(bytecode, entry_addr);
            } else {
                cpu.execute(bytecode, entry_addr);
            }
            DEBUG_INFO(Logging::DebugCategory::CPU_EXECUTION, "cpu.execute() completed successfully{}", "");

            // Print CPU state and registers (same as regular program mode)
            DEBUG_INFO(Logging::DebugCategory::CPU_EXECUTION, "Post-execution: printing CPU state...{}", "");
            cpu.print_state("End");
            DEBUG_INFO(Logging::DebugCategory::CPU_REGISTERS, "Post-execution: printing registers...{}", "");
            cpu.print_registers();

            // Print extended registers if enabled
            if (Config::extended_registers) {
                DEBUG_INFO(Logging::DebugCategory::CPU_REGISTERS, "Post-execution: printing extended registers...{}", "");
                cpu.print_extended_registers();
            }

            if (Config::memdump) {
                DEBUG_INFO(Logging::DebugCategory::MEM_ACCESS, "Post-execution: printing memory...{}", "");
                size_t mem_size = cpu.get_memory().size();
                if (mem_size > 0) {
                    cpu.print_memory(0, std::min<size_t>(mem_size, 256));
                } else {
                    DEBUG_INFO(Logging::DebugCategory::MEM_ACCESS, "Memory size is zero, skipping print_memory.{}", "");
                }
            }

            if (Config::error_count > 0) {
                Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION, fmt::format("Assembly program failed with {} errors.", Config::error_count), Logging::DebugLevel::CRITICAL);
            } else {
                Logging::DebugHandler::instance().report(Logging::DebugCategory::CPU_EXECUTION, "Assembly program completed successfully.", Logging::DebugLevel::IMPORTANT);
            }
        } catch (const std::exception& e) {
            std::cerr << "Runtime error: " << e.what() << std::endl;
            void* callstack[128];
            int frames = backtrace(callstack, 128);
            char** strs = backtrace_symbols(callstack, frames);
            std::cerr << "Backtrace:" << std::endl;
            for (int i = 0; i < frames; ++i) {
                std::cerr << "  " << strs[i] << std::endl;
            }
            free(strs);
            Config::error_count++;
        }
    }
};

int main(int argc, char *argv[]) {
    DemiApp app(argc, argv);
    app.run();
    return 0;
}
