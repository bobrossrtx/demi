# Changelog

All notable changes to DemiEngine will be documented in this file.

## [Unreleased] — DASM x86/x64 Cross-Assembler

### ✨ Multi-Target Assembler Backend
- Target-neutral assembler IR with lowering pass and pluggable backends
- CLI: `--assembly-target demi-bytecode|x86-elf32|x86-elf64`
- Demi bytecode path preserved unchanged (284 unit tests)

### x86-32 Backend (23 instructions)
- Data: MOV (reg/imm/mem/symbol/indexed)
- Stack: PUSH, POP
- Arithmetic: ADD, SUB, INC, DEC, NEG
- Logic: AND, OR, XOR, NOT
- Compare: CMP, TEST
- Shift: SHL, SHR (imm8 and ECX/CL)
- Address: LEA (base+disp, indexed, symbol)
- Control: CALL, JMP, Jcc(12), RET (register-indirect CALL/JMP)
- System: INT, NOP
- Full operand coverage: reg-reg, reg-imm, [base+disp], [base+idx*scale+disp], [symbol], mem-reg, mem-imm

### x86-64 Backend
- Full instruction parity with x86-32, REX.W prefix throughout
- Memory operations with REX.W for 64-bit operand size

### ELF Output
- ELF32 relocatable objects: .text, .data, .rodata, .bss, .rel.*, .symtab, .strtab
- ELF64 relocatable objects with RELA relocations
- R_386_32, R_386_PC32, R_X86_64_64, R_X86_64_PC32, R_X86_64_32

### ABI-Aware Lowering
- `.function` directive: auto prologue (PUSH EBP; MOV EBP,ESP) and epilogue (LEAVE)
- Verified end-to-end: sum_to_n assembles → links → runs correctly

### Preprocessor
- `.rept`/`.endr` — block repetition
- `.include`, `.define`, `.undef`, `.ifdef`/`.ifndef`/`.elif`/`.else`/`.endif`

### New Data Directives
- `.dq` (quad-word), `.asciz`, `.zero`, `.align`

### Parser Fixes
- Negative displacement sign handling in `[EBP-4]` memory operands
- `.function` symbol merging (global + label → single GLOBAL defined symbol)
- String directives no longer create spurious relocations

### Tests
- 18 built-in assembler tests, 284 unit tests, 0 warnings
- hello world assembles → links → runs correctly

## [1.0.0] - 2025-12-13

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2025-12-13

### ✨ Features

- Add roadmap for planned opcodes and operations (c2b0663) - Owen Boreham
- Refactor CPU instruction handling: add MOV, JMP, LOAD, STORE, PUSH, POP, and CMP opcodes with improved state management (afd3ec5) - Owen Boreham
- Enhance I/O operations with new opcodes and string handling (be975cb) - Owen Boreham
- Update compilation command to include additional source files and headers (67ac1d3) - Owen Boreham
- Enhance logging functionality with new log level and force bypass option feat(cpu): Improve memory dump output with forced logging and better formatting options (6aa17cd) - Owen Boreham
- Add support for pending VirtComp messages and improve logging output (3e33d79) - Owen Boreham
- Enhance argument parser with pending message logging and debug options (b3a45f0) - Owen Boreham
- Enhance argument parsing to support output name specification and sanitize filenames (398ec66) - Owen Boreham
- add port overflow checks and logging for read/write operations (4ef64e0) - Bobrossrtx
- Add LEA and SWAP opcode implementations and corresponding header files (a4416c0) - Bobrossrtx
- Add test program for LEA and SWAP opcodes to validate functionality (e5b7c97) - Bobrossrtx
- integrate modern C++ unit testing framework (a4e6c93) - Bobrossrtx
- add private memory bank and project knowledge system (574bc78) - Bobrossrtx
- enhance merge script with branch argument support (c5da7ac) - Bobrossrtx
- Add JC and JNC conditional jump instructions (972c474) - Bobrossrtx
- Integrate JC/JNC instructions and fix carry flag handling (51bd018) - Bobrossrtx
- Add JC and JNC conditional jump instructions (2626d40) - Bobrossrtx
- Integrate JC/JNC instructions and fix carry flag handling (468f3d0) - Bobrossrtx
- implement overflow flag support with JO/JNO instructions (4cd0c34) - Bobrossrtx
- implement overflow flag support with JO/JNO instructions (b0796c1) - Bobrossrtx
- implement extended register architecture with 50 x64-style registers (084893b) - Bobrossrtx
- integrate extended registers into CPU class with dual-mode support (342b045) - Bobrossrtx
- implement extended register operations and mode switching opcodes (1e2a830) - Bobrossrtx
- add UI enhancement and professional output formatting (bfa21bf) - Bobrossrtx
- update testing framework for extended register architecture (e351b42) - Bobrossrtx
- add compilation codegen infrastructure with AST generation and optimization techniques (5a87f11) - Bobrossrtx
- Implement complete assembly language support (46638da) - Bobrossrtx
- Add comprehensive assembly language examples (bbec417) - Bobrossrtx
- Expand VirtComp register architecture from 50 to 134 registers (90b9ef4) - Bobrossrtx
- Update CPU class to support 134-register architecture (b6215eb) - Bobrossrtx
- Add comprehensive SIMD/FPU/AVX/MMX opcode definitions (a974654) - Bobrossrtx
- Add comparative jump opcodes (JG, JL, JGE, JLE) (ce587f6) - Bobrossrtx
- Implement comparative jump instruction handlers (6ed8575) - Bobrossrtx
- Implement DB directive with hybrid format detection (eae6d61) - Owen Boreham
- Add ADDEX instruction for extended register operations (08f4385) - Owen Boreham
- add test directive parsing and CPU exception handling (3f04b55) - Owen Boreham
- Implement comprehensive FPU operations (0e47007) - Owen Boreham
- Implement multi-provider AI system for auto growth scripts (dd342c1) - Owen Boreham
- Complete automated community growth system with AI integration (ca2a8b9) - Owen Boreham
- implement MOD instruction and fix assembly test issues (3cad9b4) - Owen Boreham
- implement FINIT instruction and fix register synchronization (322e76d) - Owen Boreham
- implement FLD, FST, and FSTP floating point instructions (9de18d3) - Owen Boreham
- implement FPU arithmetic instructions (FADD, FSUB, FMUL, FDIV) (0cfd1cc) - Owen Boreham
- implement FPU integer conversion instructions (FILD, FIST, FISTP) (1df7099) - Owen Boreham
- implement FPU math functions (FABS, FCHS, FSQRT) (20233ac) - Owen Boreham
- implement FPU transcendental functions (FSIN, FCOS, FTAN) (e5f24ed) - Owen Boreham
- implement final FPU opcodes (FCOMPP, FUCOMPP, FCLEX, FSTCW, FLDCW, FSTSW) (a24005d) - Owen Boreham
- add comprehensive test suite for future implementations (18f508a) - Owen Boreham
- add test filtering and enhanced summary (b46f614) - Owen Boreham
- implement instruction fusion optimization system (f57111a) - Owen Boreham
- Implement comprehensive preprocessor system (9e7e367) - Owen Boreham
- Add SIMD vector operation foundation (fda150a) - Owen Boreham
- Add comprehensive error handling and debug system (e102428) - Owen Boreham
- Add 64-bit operations and branch prediction (c9cb9e2) - Owen Boreham
- Add optimized opcode dispatchers (7a376b3) - Owen Boreham
- Add cleaned test files and 64-bit test suite (b225cab) - Owen Boreham
- Enhance assembler with advanced features (3e7fcf2) - Owen Boreham
- Enhance test framework and CPU integration (249b5d6) - Owen Boreham
- Update test files with enhanced error handling (ea5f350) - Owen Boreham
- Enhance build system for production (321b05d) - Owen Boreham
- Add skipped test count to test summaries (edfadb0) - Owen Boreham
- Add test metadata directives and structure tests (50358a3) - Owen Boreham
- add interrupt controller and syscall infrastructure (3dbe3d5) - Owen Boreham
- add hexdumper utility for bytecode visualization (08c4d13) - Owen Boreham
- add mode-aware execution with x86/x64 register support (a7531e6) - Owen Boreham
- add MODE32/MODE64 directive support (1473066) - Owen Boreham
- improve debug handler with mode-aware output (617844c) - Owen Boreham
- standardize test syntax to .test and add octal number support (19f6cee) - Owen Boreham
- enhance MOV instruction with multiple operand types (f37315f) - Owen Boreham
- Add --version flag support (e0b0ffb) - Owen Boreham

### ⚡ Performance Improvements

- Enhance CPU functionality with additional instructions and debugging support (a2af336) - Owen Boreham
- Enhance CPU engine with SIMD and opcode improvements (939aa2f) - Owen Boreham

### 🐛 Bug Fixes

- reformat code for clarity and enhance datetime buffer safety (0005dbc) - Bobrossrtx
- Correctly include Logger in file_device.hpp for proper logging functionality (81234ac) - Bobrossrtx
- correct jump addresses in simple_overflow_test.hex (a9976cd) - Bobrossrtx
- correct jump addresses in simple_overflow_test.hex (3a30492) - Bobrossrtx
- resolve merge conflict in simple_overflow_test.hex (bcda728) - Bobrossrtx
- resolve merge conflict in simple_overflow_test.hex (61cf67a) - Bobrossrtx
- remove obsolete simple_overflow_test.hex file (9daef5e) - Bobrossrtx
- correct typo in header comment and clean up whitespace in output section (0c7ada8) - Bobrossrtx
- Add missing implementations for extended opcodes (679cc01) - Bobrossrtx
- Resolve integration test failures and enhance examples (c7c16d3) - Owen Boreham
- Fix argument parsing and include directive processing (d6cd95e) - Owen Boreham
- update LOAD_IMM to 6-byte format and fix FPU handlers (0886f42) - Owen Boreham
- Resolve all compiler warnings for clean build (89196e0) - Owen Boreham
- Multiple critical bug fixes for x86 calculator and assembler (0d1347f) - Owen Boreham

### 🧪 Tests

- Add test cases for basic arithmetic and logical operations (f16048f) - Owen Boreham
- Add test hex files for CPU functionality (6ab57e0) - Owen Boreham
- Enhance CPU class functionality and update test hex file (e9fec62) - Bobrossrtx
- Add pushf_popf.hex to validate PUSH_FLAG and POP_FLAG instructions (5bfa7c3) - Bobrossrtx
- improve test data and error expectations (42e8179) - Bobrossrtx
- Add comprehensive carry flag and conditional jump tests (9f7d885) - Bobrossrtx
- Add debugging tools and test programs for development (92a2945) - Bobrossrtx
- Add comprehensive carry flag and conditional jump tests (ce13e74) - Bobrossrtx
- Add debugging tools and test programs for development (d00a5b6) - Bobrossrtx
- Add comprehensive assembler test suite (8d99469) - Bobrossrtx
- Add comprehensive SIMD/FPU register architecture unit tests (31e6660) - Bobrossrtx
- implement in-assembly test framework with metadata support (75e8e23) - Owen Boreham
- Add FPU test suite for floating-point operations (2673c9d) - Owen Boreham
- Enhance instruction fusion test suite (9b23c62) - Owen Boreham
- Add include system and shared library tests (533fdec) - Owen Boreham
- Add comprehensive core instruction test suites (8f16873) - Owen Boreham
- Add advanced algorithm and specialized test suites (4dadd8d) - Owen Boreham
- Add test framework feature and error handling suites (ff93eca) - Owen Boreham
- Add comprehensive preprocessor and macro testing suites (2ae4634) - Owen Boreham
- Add specialized instruction and benchmark test suites (a9988be) - Owen Boreham
- Enhance test framework with advanced features (9222713) - Owen Boreham
- add mode-awareness tests and update test framework (7ac11c4) - Owen Boreham

### 📚 Documentation

- Add contributing guidelines for VirtComp project (3277cd5) - Owen Boreham
- Replace roadmap.txt with markdown version for improved clarity and structure (a2e07a3) - Bobrossrtx
- Update README for improved structure and clarity (ef1179f) - Bobrossrtx
- Update roadmap to clarify status of Flags & Status Operations (864bab1) - Bobrossrtx
- restructure roadmap and add comprehensive test framework design (dd9c692) - Bobrossrtx
- Update project roadmap with completed milestones (14595f6) - Bobrossrtx
- Update memory bank with critical development knowledge (ac4e998) - Bobrossrtx
- Fix critical opcode errors in QUICK_REFERENCE.md (23a9833) - Bobrossrtx
- Fix opcode and format errors in TROUBLESHOOTING.md (1b8aba8) - Bobrossrtx
- Fix opcode errors in CONTRIBUTING.md test examples (18d5a1f) - Bobrossrtx
- Major overhaul of docs/usage/README.md instruction accuracy (08afadb) - Bobrossrtx
- Fix opcode constants in API_REFERENCE.md (2cdc0c9) - Bobrossrtx
- Complete rewrite of main README.md instruction set (b7bef14) - Bobrossrtx
- Document VirtComp merge workflow and final system patterns (21e5898) - Bobrossrtx
- Document VirtComp merge workflow in memory bank (e2d43d0) - Bobrossrtx
- Update project roadmap with completed milestones (0cc6565) - Bobrossrtx
- Fix critical opcode errors in QUICK_REFERENCE.md (9f27cba) - Bobrossrtx
- Fix opcode and format errors in TROUBLESHOOTING.md (f57b808) - Bobrossrtx
- Fix opcode errors in CONTRIBUTING.md test examples (df2a748) - Bobrossrtx
- Major overhaul of docs/usage/README.md instruction accuracy (b2a2d66) - Bobrossrtx
- Fix opcode constants in API_REFERENCE.md (68475c1) - Bobrossrtx
- Complete rewrite of main README.md instruction set (567db1d) - Bobrossrtx
- update documentation for overflow flag implementation (0f6f78e) - Bobrossrtx
- update progress and decisions for overflow flag implementation (58540f2) - Bobrossrtx
- update documentation for overflow flag implementation (08ae01a) - Bobrossrtx
- update roadmap and project documentation for Phase 2.5 completion (c579d19) - Bobrossrtx
- finalize roadmap for Phase 2 assembly language development (5823b0f) - Bobrossrtx
- Update roadmap (a069c49) - Bobrossrtx
- Update project vision and roadmap details for DemiEngine (8a565dc) - Bobrossrtx
- Update documentation for 100% test coverage achievement (e36218f) - Owen Boreham
- correct opcode implementation claims (63 implemented, 88 planned) (d13f0c1) - Owen Boreham
- update roadmap priorities - assembly expansion before frontend (fbdb963) - Owen Boreham
- reorganize documentation into logical structure (56c4f66) - Owen Boreham
- remove duplicate files after reorganization (b633641) - Owen Boreham
- add comprehensive future features roadmap (c3b129e) - Owen Boreham
- add comprehensive website documentation datasets (JSON) (b7c2e73) - Owen Boreham
- Update project configuration and documentation (f371531) - Owen Boreham
- Comprehensive documentation update and reorganization (74a1af4) - Owen Boreham
- Update roadmap to reflect production-ready backend status (ffdb5d8) - Owen Boreham
- reorganize documentation structure and verify against codebase (94105fe) - Owen Boreham
- Update documentation structure and roadmap (00c7ecc) - Owen Boreham
- update documentation for mode-aware execution (4c5efb2) - Owen Boreham
- update README, roadmap, and remove run_tests.sh (9af33c6) - Owen Boreham
- update changelog, examples, and project documentation (795f490) - Owen Boreham
- regenerate changelog with latest commits (ca7b687) - Owen Boreham
- Update test coverage to 322 tests (145 unit + 177 assembly) (3591b4f) - Owen Boreham
- Update changelog with recent commits (da67f36) - Owen Boreham

### ♻️ Code Refactoring

- Update argument parser and improve process execution handling in compilation argument (7b32e74) - Owen Boreham
- updated printf statements to cout - Fix to flawfinder scan #22-26 (0a43341) - Bobrossrtx
- Mark unused variables with [[maybe_unused]] in gui.cpp and main.cpp (59a630c) - Bobrossrtx
- remove generated program_data.hpp from repository (aee170d) - Bobrossrtx
- Reorganize test files from tests/ to tests/hex/ structure (9427c81) - Bobrossrtx
- Improve code readability and structure (137d391) - Bobrossrtx
- Update default test directory and improve log formatting (76b599d) - Bobrossrtx
- Improve logging during device initialization and update test runner path (7dc4220) - Bobrossrtx
- Remove GUI system and simplify build (e98cb19) - Owen Boreham
- Improve device management and logging (f3f2677) - Owen Boreham
- update SIMD tests to use actual opcodes (0cff333) - Owen Boreham
- Eliminate compilation warnings (400+ → 0) (c7c929f) - Owen Boreham
- Reorganize examples and remove debug tests (1d1384c) - Owen Boreham
- reorganize examples into x86 and x64 directories (da207be) - Owen Boreham
- remove hex file input support, improve help output (ffcc717) - Owen Boreham
- improve CPU architecture and test infrastructure (a94ab8b) - Owen Boreham

### 💅 Code Style

- Update test result header formatting for consistency (db561d1) - Bobrossrtx
- Update settings.json with additional file associations and spell check words (8ed7c62) - Bobrossrtx
- Apply Trunk formatting and add development tooling (70078c1) - Owen Boreham
- Apply Trunk formatting to build system files (bdd651e) - Owen Boreham
- Apply Trunk formatting to configuration files (c864864) - Owen Boreham
- Apply Trunk formatting to governance documents (ffdbfaf) - Owen Boreham
- Apply Trunk formatting to documentation and examples (2fdf2d2) - Owen Boreham

### 🔧 Build System

- Enhance Makefile structure and add dependency tracking (147476c) - Bobrossrtx
- enhance test infrastructure with error handling (524c4ec) - Bobrossrtx

### 👷 Continuous Integration

- disable autogrowth workflow until properly configured (0c7ec34) - Owen Boreham

### 🔨 Chores

- Refactor CPU header: Clean up formatting and maintain consistency & update terminal output with box-creation unicode (a981e8d) - Bobrossrtx
- Cleanup: Remove unnecessary blank lines in GUI rendering code (6874d2e) - Bobrossrtx
- Cleanup: Remove fixed version of advanced I/O test with corrupt bytes (19d69dd) - Owen Boreham
- Remove obsolete hex test files for helloworld_memory and helloworld_with_loop to clean up the test suite. (9ee10fc) - Bobrossrtx
- update development environment configuration (947bc7f) - Bobrossrtx
- exclude external dependencies from repository (169eb0b) - Bobrossrtx
- Update VS Code settings and virtual storage data (48a8f87) - Bobrossrtx
- Clean up temporary debug files (d7a3d65) - Bobrossrtx
- Update VS Code settings and virtual storage data (19f945d) - Bobrossrtx
- Remove obsolete debug files and scripts (4a9d563) - Bobrossrtx
- Add additional debugging files to .gitignore (1ac473a) - Bobrossrtx
- remove obsolete development scripts (c264f28) - Bobrossrtx
- add memory bank and development files to .gitignore (e6b3bee) - Bobrossrtx
- improve .gitignore with debug file patterns and fix test file (f44eb32) - Bobrossrtx
- Update .gitignore for improved file management (e6f39ee) - Bobrossrtx
- Add project pitch to .gitignore (c511423) - Bobrossrtx
- Update .gitignore to exclude development tools and documentation (ac68b61) - Bobrossrtx
- update documentation links and development environment (2cf4aff) - Owen Boreham
- remove obsolete hex test files (9ed13dc) - Owen Boreham
- Clean up legacy test files and add comprehensive documentation (dc9d4d4) - Owen Boreham
- Repository cleanup and maintenance (6d87ce0) - Owen Boreham
- remove automated community growth scripts (c5c0ceb) - Owen Boreham
- Update .gitignore with development artifacts (63daa09) - Owen Boreham

### 🔄 Other Changes

- Initial commit (21f99bc) - Owen Boreham
- Update .gitignore to include additional debug files and build directories (312b1e9) - Owen Boreham
- Add initial implementation of the VirtComp project (294edb7) - Owen Boreham
- Add initial configuration files for VSCode (5c24476) - Owen Boreham
- Add README.md with project overview, structure, and usage instructions (de19b43) - Owen Boreham
- Update README.md to clarify program writing guidelines and assembly language status (3486a05) - Owen Boreham
- Add current and future instruction set documentation to README.md (4ab11a1) - Owen Boreham
- Create build.yml (dcedbbc) - Owen Boreham
- Update build.yml (4dcbcb7) - Owen Boreham
- Create flawfinder.yml (22c18ab) - Owen Boreham
- Add launch and tasks configuration files for C++ development (4f0b00d) - Owen Boreham
- Add debugging flags to C++ compilation in Makefile (c9c37ee) - Owen Boreham
- Fix instruction sequence in push_pop.hex for correct R0 and R1 operations (8e08dda) - Owen Boreham
- Add ImGui integration for VirtComp VM debug GUI and update Makefile (6d291a4) - Owen Boreham
- Add prerequisites installation to Makefile and update README instructions (87364c8) - Owen Boreham
- Add Fira Code font files for improved typography (f925bee) - Bobrossrtx
- Update VSCode configuration files: refine include paths, adjust C standard, and enhance file associations (078e7d9) - Bobrossrtx
- Refactor ImGui window settings: adjust positions, sizes, and docking configurations for improved layout (ffb70a0) - Bobrossrtx
- Enhance Makefile: add fmt library prerequisites and improve installation messages (493a0f7) - Bobrossrtx
- Update test hex file: add header comment and correct data format (f15e122) - Bobrossrtx
- Update roadmap: format opcode list for improved readability (9998d15) - Bobrossrtx
- Update build.yml (0612def) - Owen Boreham
- Refactor argument parser: Improve code structure and enhance readability (9ae77eb) - Bobrossrtx
- Add virtual device management and I/O device implementations (da46b6e) - Bobrossrtx
- Update .gitignore: Add cache file patterns to ignore list (1073fce) - Bobrossrtx
- Add configuration provider to C++ IntelliSense settings (64ef19f) - Bobrossrtx
- Add compile mode and program data generation functionality (f5dda94) - Bobrossrtx
- Add comprehensive usage documentation for VirtComp and update roadmap (c5216b9) - Owen Boreham
- Potential fix for code scanning alert no. 28: Workflow does not contain permissions (841f1ec) - Owen Boreham
- Merge pull request #1 from bobrossrtx/alert-autofix-28 (9ec3213) - Owen Boreham
- security: Fix critical buffer overflow and file security vulnerabilities (6fd793a) - Bobrossrtx
- security: Fix critical TOCTOU race condition in serial port device (CWE-362) (27f5393) - Bobrossrtx
- security: Complete buffer boundary fixes for file device operations (054e129) - Bobrossrtx
- Refactor(Opcodes/CPU): Implement opcode handlers for various operations (72dcc08) - Bobrossrtx
- Remove obsolete opcode handlers for OR, OUT, OUTB, OUTL, OUTSTR, OUTW, POP, POP_ARG, POP_FLAG, PUSH, PUSH_ARG, PUSH_FLAG, RET, SHL, SHR, STORE, SUB, and XOR instructions to streamline the codebase and improve maintainability. (16e9075) - Bobrossrtx
- Remove obsolete jmp, jz, and sub opcode files to streamline the codebase (f507951) - Owen Boreham
- Update README.md (e98a72b) - Owen Boreham
- Merge branch 'main' into private-memory-bank (606a275) - Bobrossrtx
- Implement LEA and SWAP opcodes with comprehensive testing (d9d2e23) - Bobrossrtx
- Update roadmap to reflect current progress and test results (3e24b07) - Bobrossrtx
- 🎉 Phase 1 Complete: Achieve 100% Test Coverage (53/53 tests) (92785b2) - Bobrossrtx
- Merge Assembly Update (c322ad2) - Bobrossrtx
- Phase 3A: Complete codebase documentation overhaul (20f75d7) - Bobrossrtx
- Update register implementation notes and workspace changes (54940bc) - Bobrossrtx
- Pre-rename backup: Complete VirtComp codebase before DemiEngine transition (1a5dacf) - Bobrossrtx
- Step-by-step rename: Core VirtComp → DemiEngine transformation complete (861175a) - Bobrossrtx
- Complete VirtComp → DemiEngine rename transformation (85be170) - Bobrossrtx
- Update build status to use correct repo url (ad07388) - Owen Boreham
- Fix LOAD_IMM64 instruction: properly decode 64-bit immediates and advance PC (c982ca0) - Owen Boreham
- Fix ADD instruction to handle 8-bit overflow and flags (6b1c339) - Owen Boreham
- Fix assembly test metadata printing and completion message placement (3e226f0) - Owen Boreham
- Exclude examples from default assembly test runs (67f2611) - Owen Boreham
- Integrate 20 comprehensive features into development roadmap (5e254ce) - Owen Boreham
- Fix GitHub Actions workflow for Gemini AI and Twitter-only mode (107f966) - Owen Boreham
- Remove OpenAI dependency from daily post generator (18a8225) - Owen Boreham
- Fix formatting and update test results in README (6dcab54) - Owen Boreham
- Add packed byte manipulation instructions (3e653e1) - Owen Boreham
- Add LOADR indirect addressing instruction (3eb1189) - Owen Boreham
- Add example programs and include demonstrations (9e4adf5) - Owen Boreham
- Enhance core assembler with preprocessor integration (970836a) - Owen Boreham
- Add comprehensive version management system (f6c21a2) - Owen Boreham
- Add version management documentation (cd8df4a) - Owen Boreham
- Update issue #3 with implementation notes (97bf729) - Owen Boreham
- Add Contributor Covenant Code of Conduct (f7dbc02) - Owen Boreham
- Revise and expand security policy documentation (8b082a0) - Owen Boreham
- Update issue templates (4020a6e) - Owen Boreham
- Add issue template configuration for security reports (1c4d8f2) - Owen Boreham
- Refactor Makefile to production-ready build system (67bbbce) - Owen Boreham


