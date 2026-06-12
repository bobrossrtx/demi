# =============================================================================
# DEMI Engine - Production Makefile
# =============================================================================
# Fast, efficient, production-ready build system
# Features: parallel builds, dependency tracking, incremental compilation,
# multiple build configurations, comprehensive testing
# =============================================================================

# Compiler configuration
CXX := g++
SHELL := /bin/bash

# CPU core detection for parallel builds
NPROCS := $(shell nproc 2>/dev/null || echo 1)
MAKEFLAGS += -j$(NPROCS)

# Directory structure
SRC_DIR := src
INCLUDE_DIR := include
BUILD_DIR := build
BIN_DIR := bin
EXTERN_DIR := extern
DOC_DIR := docs
TEST_DIR := tests

# Build configuration
BUILD_TYPE ?= debug
CONFIG_FILE := .build_config
PLATFORM ?= linux

EXE_EXT :=
STATIC_EXT := .a
SHARED_EXT := .so
ifeq ($(PLATFORM),windows)
    EXE_EXT := .exe
    SHARED_EXT := .dll
    # Change compiler to mingw for windows cross-compilation if requested
    CXX := x86_64-w64-mingw32-g++
endif

# =============================================================================
# Build Configuration Profiles
# =============================================================================

# Debug build (default)
ifeq ($(BUILD_TYPE),debug)
CXXFLAGS := -Wall -Wextra -Wpedantic -std=c++17 -g3 -O0 -MMD -MP
CXXFLAGS += -DDEMI_DEBUG_BUILD -DDEMI_DEBUG_MODE
CXXFLAGS += -DVM_DEBUG_BOUNDS -DVM_DEBUG_LOGGING -DVM_FULL_VALIDATION
LDFLAGS := -g
BUILD_SUFFIX := -debug
endif

# Release build
ifeq ($(BUILD_TYPE),release)
CXXFLAGS := -Wall -Wextra -std=c++17 -O3 -DNDEBUG -MMD -MP -flto
CXXFLAGS += -march=native -mtune=native -fomit-frame-pointer
CXXFLAGS += -DVM_RELEASE -DVM_NO_BOUNDS_CHECK -DVM_NO_DEBUG_LOG
CXXFLAGS += -DDEMI_RELEASE_BUILD
LDFLAGS := -O3 -flto -s
BUILD_SUFFIX := -release
endif

# Profile build (optimized with debug symbols)
ifeq ($(BUILD_TYPE),profile)
CXXFLAGS := -Wall -Wextra -std=c++17 -O2 -g -MMD -MP
CXXFLAGS += -DNDEBUG -fno-omit-frame-pointer -DDEMI_PROFILE_MODE
CXXFLAGS += -DVM_PROFILE -DVM_LIMITED_BOUNDS_CHECK
LDFLAGS := -O2 -g
BUILD_SUFFIX := -profile
endif

# Sanitizer build (for testing)
ifeq ($(BUILD_TYPE),sanitize)
CXXFLAGS := -Wall -Wextra -std=c++17 -O1 -g -MMD -MP
CXXFLAGS += -fsanitize=address,undefined,leak -fno-omit-frame-pointer
LDFLAGS := -fsanitize=address,undefined,leak
BUILD_SUFFIX := -sanitize
endif

# Common linker flags
LDFLAGS += -lstdc++fs -pthread

ifneq ($(PLATFORM),windows)
    CXXFLAGS += -fPIC
else
    # LDFLAGS += -static-libgcc -static-libstdc++  # Disabled for DLL build
endif

# =============================================================================
# External Dependencies
# =============================================================================

# FMT library
FMT_DIR := $(EXTERN_DIR)/fmt
FMT_SRCS := $(FMT_DIR)/src/format.cc
FMT_OBJS := $(patsubst $(FMT_DIR)/src/%.cc,$(BUILD_DIR)/fmt/%.o,$(FMT_SRCS))
FMT_DEPS := $(FMT_OBJS:.o=.d)
CXXFLAGS += -I$(FMT_DIR)/include
CXXFLAGS += -I$(INCLUDE_DIR)

# =============================================================================
# Source File Discovery and Object Generation
# =============================================================================

# Find all source files (cached for performance)
MAIN_SRCS := $(shell find $(SRC_DIR) -name '*.cpp' -not -name 'test_assembler.cpp' -not -name 'test_level2.cpp')
TEST_SRCS := $(shell find $(SRC_DIR)/test -name 'test_assembler.cpp' -type f 2>/dev/null)
ALL_SRCS := $(MAIN_SRCS) $(TEST_SRCS)

# Generate object file paths
MAIN_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(MAIN_SRCS))
TEST_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(TEST_SRCS))
ALL_OBJS := $(MAIN_OBJS) $(TEST_OBJS)

# Dependency files
ALL_DEPS := $(ALL_OBJS:.o=.d) $(FMT_DEPS)

# =============================================================================
# Target Definitions
# =============================================================================

# Main executable
TARGET := $(BIN_DIR)/demi-engine$(BUILD_SUFFIX)$(EXE_EXT)
MAIN_TARGET_OBJS := $(filter-out $(BUILD_DIR)/test/test_assembler.o,$(MAIN_OBJS))

# Test targets
TEST_TARGET := $(BIN_DIR)/test_runner$(BUILD_SUFFIX)$(EXE_EXT)
ASSEMBLER_TEST_TARGET := $(BIN_DIR)/test_assembler$(BUILD_SUFFIX)$(EXE_EXT)

# Library objects (everything except main.o and test_assembler.o)
# LIB_OBJS := $(filter-out $(BUILD_DIR)/main.o $(BUILD_DIR)/test/test_assembler.o,$(MAIN_OBJS))
# Library objects (everything except main.o and any test files)
LIB_OBJS := $(filter-out $(BUILD_DIR)/main.o $(BUILD_DIR)/test/%,$(MAIN_OBJS))

# Library targets (shared and static)
LIB_STATIC := $(BIN_DIR)/libdemi$(BUILD_SUFFIX)$(STATIC_EXT)
LIB_SHARED := $(BIN_DIR)/libdemi$(BUILD_SUFFIX)$(SHARED_EXT)

# =============================================================================
# Default Target and Build Goals
# =============================================================================

.DEFAULT_GOAL := all
.PHONY: all clean build install test unit-test integration-test benchmark
.PHONY: debug release profile sanitize prereqs help version info
.PHONY: test-all test-assembler force-rebuild check-deps lib-all
.PHONY: benchmark-binaries docs lint format
.PHONY: debug-symbols debug-dependencies debug-linking debug-undefined
.PHONY: debug-undefined-detailed debug-problematic-objects debug-minimal-link
.PHONY: debug-analysis debug-clean debug-recommendations
.PHONY: codacy-install codacy-login codacy-add codacy-issues codacy-critical
.PHONY: codacy-findings codacy-tools codacy-reanalyze codacy-local codacy-info
.PHONY: sonar sonar-docker sonar-check-deps sonar-info
.PHONY: coderabbit-info
.PHONY: promote promote-check

# =============================================================================
# Main Build Targets
# =============================================================================

# Default build (all configurations)
all: $(TARGET)

# Quick build shorthand
build: all

# Main executable
$(TARGET): $(MAIN_TARGET_OBJS) $(FMT_OBJS) | $(BIN_DIR)
	@echo "🔗 Linking $(notdir $@)..."
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
        @echo "✅ Build complete: $@"

# Library targets
lib-all: $(LIB_STATIC) $(LIB_SHARED)
	@mkdir -p /usr/local/include/demi 2>/dev/null || true
	@cp $(INCLUDE_DIR)/demi/engine.hpp $(INCLUDE_DIR)/demi/engine_c_api.h /usr/local/include/demi/ 2>/dev/null || true
	@echo "✅ Libraries built and headers copied"

$(LIB_STATIC): $(LIB_OBJS) $(FMT_OBJS) | $(BIN_DIR)
	@echo "📦 Creating static library..."
	@ar rcs $@ $^
	@echo "✅ Static library built: $@"

$(LIB_SHARED): $(LIB_OBJS) $(FMT_OBJS) | $(BIN_DIR)
	@echo "🔗 Linking shared library..."
	@$(CXX) -shared $(CXXFLAGS) -DDEMI_BUILD_SHARED -o $@ $^ $(LDFLAGS)
	@echo "✅ Shared library built: $@"

# Test runner
$(TEST_TARGET): $(LIB_OBJS) $(TEST_OBJS) $(FMT_OBJS) | $(BIN_DIR)
	@echo "🔗 Linking test runner..."
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✅ Test runner built: $@"

# Assembler test (lightweight)
$(ASSEMBLER_TEST_TARGET): $(filter $(BUILD_DIR)/assembler/%,$(LIB_OBJS)) \
                          $(filter $(BUILD_DIR)/debug/%,$(LIB_OBJS)) \
                          $(filter $(BUILD_DIR)/test/test_assembler%,$(TEST_OBJS)) \
                          $(FMT_OBJS) | $(BIN_DIR)
	@echo "🔗 Linking assembler test..."
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✅ Assembler test built: $@"

# =============================================================================
# Compilation Rules
# =============================================================================

# Source file compilation
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "🔨 Compiling $(notdir $<)..."
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# FMT library compilation
$(BUILD_DIR)/fmt/%.o: $(FMT_DIR)/src/%.cc
	@mkdir -p $(dir $@)
	@echo "🔨 Compiling FMT $(notdir $<)..."
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# =============================================================================
# Directory Creation
# =============================================================================

$(BUILD_DIR) $(BIN_DIR):
	@mkdir -p $@

# =============================================================================
# Build Configuration Management
# =============================================================================

# Configuration targets
debug:
	@$(MAKE) BUILD_TYPE=debug all
	@echo "BUILD_TYPE=debug" > $(CONFIG_FILE)

release:
	@$(MAKE) BUILD_TYPE=release all
	@echo "BUILD_TYPE=release" > $(CONFIG_FILE)

profile:
	@$(MAKE) BUILD_TYPE=profile all
	@echo "BUILD_TYPE=profile" > $(CONFIG_FILE)

sanitize:
	@$(MAKE) BUILD_TYPE=sanitize all
	@echo "BUILD_TYPE=sanitize" > $(CONFIG_FILE)

# =============================================================================
# Testing Targets
# =============================================================================

# Run all tests
test-all: unit-test integration-test

# Unit tests
unit-test: $(TARGET)
	@echo "🧪 Running unit tests..."
	@./$(TARGET) -t

# Integration tests with debug output
integration-test: $(TARGET)
	@echo "🔬 Running integration tests..."
	@./$(TARGET) -t -d

# Assembler-only tests
test-assembler: $(ASSEMBLER_TEST_TARGET)
	@echo "🧪 Running assembler tests..."
	@./$(ASSEMBLER_TEST_TARGET)

# Simple test alias
test: unit-test

# =============================================================================
# Development and Maintenance Targets
# =============================================================================

# Clean build artifacts
clean:
	@echo "🧹 Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@rm -f $(CONFIG_FILE)
	@echo "✅ Clean complete"

# Force rebuild of specific components
force-rebuild:
	@echo "🔄 Force rebuilding opcodes..."
	@rm -f $(BUILD_DIR)/engine/opcodes/*.o $(BUILD_DIR)/engine/opcodes/*.d
	@$(MAKE) all

# Install system prerequisites
prereqs:
	@echo "📦 Checking system prerequisites..."
	@if ! dpkg -s libglfw3-dev libglew-dev libgl1-mesa-dev xorg-dev >/dev/null 2>&1; then \
		echo "🔧 Installing required system libraries..."; \
		sudo apt-get -qq update && sudo apt-get -qq install -y \
			libglfw3-dev libglew-dev libgl1-mesa-dev xorg-dev \
			build-essential pkg-config; \
	else \
		echo "✅ All required system libraries are installed"; \
	fi
	@if [ ! -d $(FMT_DIR) ]; then \
		echo "📥 Cloning FMT library..."; \
		git clone --depth=1 --branch=10.1.1 https://github.com/fmtlib/fmt.git $(FMT_DIR); \
	else \
		echo "✅ FMT library present"; \
	fi
	@if [ ! -d $(IMGUI_DIR) ]; then \
		echo "📥 Cloning ImGui library..."; \
		git clone --depth=1 --branch=v1.89.9 https://github.com/ocornut/imgui.git $(IMGUI_DIR); \
	else \
		echo "✅ ImGui library present"; \
	fi

# Dependency check
check-deps:
	@echo "🔍 Checking dependencies..."
	@echo "Source files: $(words $(ALL_SRCS))"
	@echo "Object files: $(words $(ALL_OBJS))"
	@echo "FMT objects: $(words $(FMT_OBJS))"
	@echo "ImGui objects: $(words $(IMGUI_OBJS))"
	@echo "Build type: $(BUILD_TYPE)"

# =============================================================================
# Performance and Benchmarking
# =============================================================================

# Benchmark build configurations
BENCHMARK_BUILD_DIR := build-benchmark
BENCHMARK_THREADED_TARGET := $(BIN_DIR)/demi-engine-threaded
BENCHMARK_FALLBACK_TARGET := $(BIN_DIR)/demi-engine-fallback

# Optimized benchmark objects
BENCHMARK_THREADED_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BENCHMARK_BUILD_DIR)/threaded/%.o,$(MAIN_SRCS))
BENCHMARK_FALLBACK_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BENCHMARK_BUILD_DIR)/fallback/%.o,$(MAIN_SRCS))

# Benchmark FMT objects
BENCHMARK_THREADED_FMT := $(patsubst $(FMT_DIR)/src/%.cc,$(BENCHMARK_BUILD_DIR)/threaded/fmt/%.o,$(FMT_SRCS))
BENCHMARK_FALLBACK_FMT := $(patsubst $(FMT_DIR)/src/%.cc,$(BENCHMARK_BUILD_DIR)/fallback/fmt/%.o,$(FMT_SRCS))

# Threaded benchmark build (maximum optimization)
$(BENCHMARK_BUILD_DIR)/threaded/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "⚡ Compiling threaded benchmark $(notdir $<)..."
	@$(CXX) -Wall -Wextra -std=c++17 -O3 -DNDEBUG -flto -march=native \
		-I$(FMT_DIR)/include -MMD -MP -c $< -o $@

$(BENCHMARK_BUILD_DIR)/threaded/fmt/%.o: $(FMT_DIR)/src/%.cc
	@mkdir -p $(dir $@)
	@echo "⚡ Compiling threaded FMT $(notdir $<)..."
	@$(CXX) -Wall -Wextra -std=c++17 -O3 -DNDEBUG -flto -march=native \
		-I$(FMT_DIR)/include -MMD -MP -c $< -o $@

$(BENCHMARK_THREADED_TARGET): $(BENCHMARK_THREADED_OBJS) $(BENCHMARK_THREADED_FMT) | $(BIN_DIR)
	@echo "🚀 Linking threaded benchmark..."
	@$(CXX) -O3 -DNDEBUG -flto -march=native -o $@ $^ -lstdc++fs -pthread
	@echo "✅ Threaded benchmark built: $@"

# Fallback benchmark build (with sanitizer)
$(BENCHMARK_BUILD_DIR)/fallback/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "🔍 Compiling fallback benchmark $(notdir $<)..."
	@$(CXX) -Wall -Wextra -std=c++17 -O2 -g -fsanitize=address \
		-I$(FMT_DIR)/include -MMD -MP -c $< -o $@

$(BENCHMARK_BUILD_DIR)/fallback/fmt/%.o: $(FMT_DIR)/src/%.cc
	@mkdir -p $(dir $@)
	@echo "🔍 Compiling fallback FMT $(notdir $<)..."
	@$(CXX) -Wall -Wextra -std=c++17 -O2 -g -fsanitize=address \
		-I$(FMT_DIR)/include -MMD -MP -c $< -o $@

$(BENCHMARK_FALLBACK_TARGET): $(BENCHMARK_FALLBACK_OBJS) $(BENCHMARK_FALLBACK_FMT) | $(BIN_DIR)
	@echo "🔍 Linking fallback benchmark..."
	@$(CXX) -O2 -g -fsanitize=address -o $@ $^ -lstdc++fs -pthread -fsanitize=address
	@echo "✅ Fallback benchmark built: $@"

# Build both benchmark targets
benchmark-binaries: $(BENCHMARK_THREADED_TARGET) $(BENCHMARK_FALLBACK_TARGET)
	@echo "📊 Benchmark binaries ready:"
	@echo "  🚀 $(BENCHMARK_THREADED_TARGET) (optimized)"
	@echo "  🔍 $(BENCHMARK_FALLBACK_TARGET) (sanitized)"

# Run comprehensive benchmark suite
benchmark: benchmark-binaries
	@echo "🏁 DEMI Engine Performance Benchmark"
	@echo "═══════════════════════════════════════"
	@if [ ! -d benchmarks ]; then \
		echo "⚠️  No benchmarks/ directory found - creating basic test..."; \
		mkdir -p benchmarks; \
		echo "# Basic test case" > benchmarks/basic.asm; \
		echo "MOV R1, 42" >> benchmarks/basic.asm; \
		echo "ADD R1, 8" >> benchmarks/basic.asm; \
		echo "HALT" >> benchmarks/basic.asm; \
	fi
	@BENCHMARK_FILES=$$(find benchmarks -name "*.asm" | sort); \
	if [ -z "$$BENCHMARK_FILES" ]; then \
		echo "❌ No .asm files found in benchmarks/"; \
		exit 1; \
	fi; \
	echo "📁 Found benchmark files:"; \
	for file in $$BENCHMARK_FILES; do echo "  📄 $$file"; done; \
	echo; \
	for file in $$BENCHMARK_FILES; do \
		echo "🧪 Testing $$file"; \
		echo "────────────────────────────────────────"; \
		echo "🚀 Threaded Dispatcher:"; \
		for i in 1 2 3; do \
			printf "  Run $$i: "; \
			if timeout 30 ./$(BENCHMARK_THREADED_TARGET) -A "$$file" >/dev/null 2>&1; then \
				echo "✅ Success"; \
			else \
				echo "❌ Failed"; \
			fi; \
		done; \
		echo "🔍 Fallback Dispatcher:"; \
		for i in 1 2 3; do \
			printf "  Run $$i: "; \
			start=$$(date +%s.%N); \
			if timeout 30 ./$(BENCHMARK_FALLBACK_TARGET) -A "$$file" >/dev/null 2>&1; then \
				end=$$(date +%s.%N); \
				runtime=$$(echo "$$end - $$start" | bc -l 2>/dev/null || echo "0.0000"); \
				printf "✅ %.4fs\n" $$runtime; \
			else \
				echo "❌ Failed"; \
			fi; \
		done; \
		echo; \
	done; \
	echo "🏁 Benchmark complete!"

# =============================================================================
# Development Quality Tools
# =============================================================================

# Code formatting (if clang-format is available)
format:
	@if command -v clang-format >/dev/null 2>&1; then \
		echo "🎨 Formatting source code..."; \
		find $(SRC_DIR) -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i; \
		echo "✅ Code formatting complete"; \
	else \
		echo "⚠️  clang-format not available"; \
	fi

# Static analysis (if available)
lint:
	@if command -v cppcheck >/dev/null 2>&1; then \
		echo "🔍 Running static analysis..."; \
		cppcheck --enable=warning,style,performance,portability \
			--std=c++17 --quiet $(SRC_DIR)/; \
	else \
		echo "⚠️  cppcheck not available"; \
	fi

# Project information
info:
	@echo "📋 DEMI Engine Build Information"
	@echo "═══════════════════════════════════"
	@echo "🏗️  Build type: $(BUILD_TYPE)"
	@echo "🔧 Compiler: $(CXX)"
	@echo "⚙️  Flags: $(CXXFLAGS)"
	@echo "🔗 Linker flags: $(LDFLAGS)"
	@echo "📁 Source files: $(words $(ALL_SRCS))"
	@echo "🎯 Main target: $(TARGET)"
	@echo "🧪 Test target: $(TEST_TARGET)"
	@echo "🚀 CPU cores: $(NPROCS)"
	@echo "📦 External libs: FMT"

# Version information (if VERSION file exists)
version:
	@if [ -f VERSION ]; then \
		echo "📋 DEMI Engine v$$(cat VERSION)"; \
	else \
		echo "📋 DEMI Engine (development)"; \
	fi
	@if [ -f $(CONFIG_FILE) ]; then \
		echo "🏗️  Last build: $$(cat $(CONFIG_FILE))"; \
	fi

# Help target
help:
	@{ \
		echo "🛠️  DEMI Engine Makefile Commands"; \
		echo "════════════════════════════════════"; \
		echo ""; \
		echo "🏗️  Build Commands:"; \
		echo "  make, make all    - Build main target (current: $(BUILD_TYPE))"; \
		echo "  make debug        - Build debug version"; \
		echo "  make release      - Build optimized release"; \
		echo "  make profile      - Build with profiling symbols"; \
		echo "  make sanitize     - Build with sanitizers"; \
		echo ""; \
		echo "🧪 Test Commands:"; \
		echo "  make test         - Run unit tests"; \
		echo "  make test-all     - Run all tests"; \
		echo "  make unit-test    - Run unit tests only"; \
		echo "  make integration-test - Run integration tests"; \
		echo "  make test-assembler   - Run assembler tests"; \
		echo ""; \
		echo "📊 Performance:"; \
		echo "  make benchmark    - Run performance benchmarks"; \
		echo "  make benchmark-binaries - Build benchmark targets"; \
		echo ""; \
		echo "🛠️  Maintenance:"; \
		echo "  make clean        - Remove build artifacts"; \
		echo "  make prereqs      - Install system dependencies"; \
		echo "  make force-rebuild - Force rebuild opcodes"; \
		echo "  make format       - Format source code"; \
		echo "  make lint         - Static analysis"; \
		echo "  make codacy-install  - Install Codacy Cloud CLI"; \
		echo "  make codacy-info     - Codacy setup instructions"; \
		echo "  make sonar-info      - SonarCloud setup guide"; \
		echo "  make promote        - Growth checklist & action plan"; \
		echo "  make promote-check  - Community health audit"; \
		echo ""; \
		echo "🔍 Debugging:"; \
		echo "  make debug-symbols         - Analyze symbol tables"; \
		echo "  make debug-dependencies    - Check library dependencies"; \
		echo "  make debug-linking         - Verbose linking with map"; \
		echo "  make debug-undefined       - Find undefined symbols"; \
		echo "  make debug-undefined-detailed - Detailed undefined analysis"; \
		echo "  make debug-problematic-objects - Find problematic object files"; \
		echo "  make debug-minimal-link    - Test minimal linking"; \
		echo "  make debug-analysis        - Comprehensive debug analysis"; \
		echo "  make debug-recommendations - Get actionable fix recommendations"; \
		echo "  make debug-clean           - Clean debug files"; \
		echo ""; \
		echo "ℹ️  Information:"; \
		echo "  make info         - Build configuration info"; \
		echo "  make version      - Version information"; \
		echo "  make help         - This help message"; \
	} 2>/dev/null || true

# =============================================================================
# Debugging and Analysis Targets
# =============================================================================

# Examine symbol tables in object files
debug-symbols:
	@echo "🔍 Analyzing symbol tables..."
	@if [ -d $(BUILD_DIR) ] && [ $$(find $(BUILD_DIR) -name "*.o" | wc -l) -gt 0 ]; then \
		echo "📄 Generating symbols.txt..."; \
		nm -C $(BUILD_DIR)/**/*.o > symbols.txt 2>/dev/null || \
		find $(BUILD_DIR) -name "*.o" -exec nm -C {} \; > symbols.txt; \
		echo "✅ Symbol analysis saved to symbols.txt"; \
		echo "📊 Symbol summary:"; \
		echo "  🔤 Total symbols: $$(wc -l < symbols.txt)"; \
		echo "  🔧 Defined symbols: $$(grep -c ' [TtDdRrBb] ' symbols.txt || echo 0)"; \
		echo "  ❓ Undefined symbols: $$(grep -c ' U ' symbols.txt || echo 0)"; \
	else \
		echo "❌ No object files found. Build first with 'make all'"; \
		exit 1; \
	fi

# Check shared library dependencies
debug-dependencies:
	@echo "🔗 Analyzing library dependencies..."
	@if [ -f $(TARGET) ]; then \
		echo "📋 Dependencies for $(TARGET):"; \
		ldd $(TARGET) || echo "⚠️  ldd failed - static binary or not built yet"; \
		echo ""; \
		echo "🔍 Dynamic symbol analysis:"; \
		readelf -d $(TARGET) 2>/dev/null | grep NEEDED || echo "  No dynamic dependencies"; \
	else \
		echo "❌ Target $(TARGET) not found. Build first with 'make all'"; \
		exit 1; \
	fi

# Verbose linking with map file generation
debug-linking:
	@echo "🔗 Debug linking with verbose output..."
	@echo "🗺️  Generating link map..."
	@$(MAKE) clean >/dev/null 2>&1 || true
	@$(CXX) $(CXXFLAGS) -Wl,--verbose -Wl,-Map=link.map -o $(TARGET) $(MAIN_TARGET_OBJS) $(FMT_OBJS) $(LDFLAGS) 2>&1 | \
		grep -E "(attempt to open|succeeded|failed|undefined reference)" || true
	@if [ -f link.map ]; then \
		echo "✅ Link map generated: link.map"; \
		echo "📊 Link summary:"; \
		echo "  📄 Sections: $$(grep -c '^\.' link.map || echo 0)"; \
		echo "  🏗️  Objects linked: $$(grep -c '\.o)' link.map || echo 0)"; \
		echo "  📚 Libraries: $$(grep -c '\.so' link.map || echo 0)"; \
	else \
		echo "⚠️  Link map not generated"; \
	fi

# Analyze undefined symbols specifically
debug-undefined:
	@echo "❓ Analyzing undefined symbols..."
	@if [ -d $(BUILD_DIR) ] && [ $$(find $(BUILD_DIR) -name "*.o" | wc -l) -gt 0 ]; then \
		echo "🔍 Searching for undefined symbols in object files..."; \
		UNDEFINED=$$(find $(BUILD_DIR) -name "*.o" -exec nm -u {} \; 2>/dev/null | grep -v "^$$" | sort | uniq); \
		if [ -n "$$UNDEFINED" ]; then \
			echo "📄 Undefined symbols found:"; \
			echo "$$UNDEFINED" | head -20; \
			if [ $$(echo "$$UNDEFINED" | wc -l) -gt 20 ]; then \
				echo "  ... and $$(echo "$$UNDEFINED" | wc -l | awk '{print $$1-20}') more"; \
			fi; \
			echo "$$UNDEFINED" > undefined_symbols.txt; \
			echo "✅ Full list saved to undefined_symbols.txt"; \
		else \
			echo "✅ No undefined symbols found"; \
		fi; \
	else \
		echo "❌ No object files found. Build first with 'make all'"; \
		exit 1; \
	fi

# Enhanced undefined symbol analysis with demangling and categorization
debug-undefined-detailed:
	@echo "🔍 Detailed undefined symbol analysis..."
	@if [ -d $(BUILD_DIR) ] && [ $$(find $(BUILD_DIR) -name "*.o" | wc -l) -gt 0 ]; then \
		echo "📊 Generating detailed symbol analysis..."; \
		find $(BUILD_DIR) -name "*.o" -exec nm -u {} + 2>/dev/null | \
			grep -v "^$$" | sort | uniq > undefined_symbols_raw.txt; \
		echo "🔤 Demangling C++ symbols..."; \
		grep "_Z" undefined_symbols_raw.txt | c++filt > undefined_symbols_demangled.txt; \
		echo "📈 Symbol categories:"; \
		echo "  🏗️  Total undefined: $$(wc -l < undefined_symbols_raw.txt)"; \
		echo "  🔧 C++ mangled: $$(grep -c "_Z" undefined_symbols_raw.txt)"; \
		echo "  📚 Standard library: $$(grep -c "__" undefined_symbols_raw.txt)"; \
		echo "  🎯 Handle functions: $$(grep -c "handle_" undefined_symbols_demangled.txt || echo 0)"; \
		echo "  ⚙️  System symbols: $$(grep -c "_GLOBAL_OFFSET_TABLE_\|_Unwind_Resume" undefined_symbols_raw.txt)"; \
		if [ -s undefined_symbols_demangled.txt ]; then \
			echo "🔍 Sample demangled symbols:"; \
			head -10 undefined_symbols_demangled.txt | sed 's/^/    /'; \
		fi; \
		echo "📄 Files generated:"; \
		echo "  📋 undefined_symbols_raw.txt - Raw symbol names"; \
		echo "  🔤 undefined_symbols_demangled.txt - Human-readable names"; \
	else \
		echo "❌ No object files found. Build first with 'make all'"; \
		exit 1; \
	fi

# Comprehensive debugging analysis
debug-analysis: debug-symbols debug-dependencies debug-undefined
	@echo "📋 Comprehensive Debug Analysis Complete"
	@echo "═══════════════════════════════════════"
	@echo "📄 Generated files:"
	@[ -f symbols.txt ] && echo "  🔤 symbols.txt - All symbol tables" || true
	@[ -f undefined_symbols.txt ] && echo "  ❓ undefined_symbols.txt - Undefined symbols" || true
	@[ -f link.map ] && echo "  🗺️  link.map - Linker map file" || true
	@echo "🔧 Use these files to debug linking and symbol issues"

# Find problematic object files that reference undefined symbols
debug-problematic-objects:
	@echo "🔍 Finding problematic object files..."
	@if [ -d $(BUILD_DIR) ] && [ $$(find $(BUILD_DIR) -name "*.o" | wc -l) -gt 0 ]; then \
		echo "📊 Analyzing object files for undefined references..."; \
		for obj in $$(find $(BUILD_DIR) -name "*.o"); do \
			UNDEF_COUNT=$$(nm -u "$$obj" 2>/dev/null | grep -v "^$$" | grep -c "handle_"); \
			if [ "$$UNDEF_COUNT" -gt 0 ] 2>/dev/null; then \
				echo "⚠️  $$obj: $$UNDEF_COUNT undefined handle_ symbols"; \
				nm -u "$$obj" 2>/dev/null | grep "handle_" | head -3 | c++filt | sed 's/^/    /'; \
				if [ "$$UNDEF_COUNT" -gt 3 ] 2>/dev/null; then \
					echo "    ... and $$((UNDEF_COUNT - 3)) more"; \
				fi; \
			fi; \
		done; \
		echo "🔍 Objects with most undefined symbols:"; \
		for obj in $$(find $(BUILD_DIR) -name "*.o"); do \
			UNDEF_COUNT=$$(nm -u "$$obj" 2>/dev/null | grep -v "^$$" | wc -l); \
			echo "$$UNDEF_COUNT $$obj"; \
		done | sort -nr | head -5 | while read count file; do \
			echo "  📄 $$(basename $$file): $$count undefined symbols"; \
		done; \
	else \
		echo "❌ No object files found. Build first with 'make all'"; \
		exit 1; \
	fi

# Test linking without specific problematic files
debug-minimal-link:
	@echo "🔗 Testing minimal linking..."
	@echo "🧪 Attempting to link core components only..."
	@CORE_OBJS="$(BUILD_DIR)/main.o $(BUILD_DIR)/engine/cpu.o $(BUILD_DIR)/engine/opcodes/opcodes_consolidated.o"; \
	AVAILABLE_OBJS=""; \
	for obj in $$CORE_OBJS; do \
		if [ -f "$$obj" ]; then \
			AVAILABLE_OBJS="$$AVAILABLE_OBJS $$obj"; \
		else \
			echo "⚠️  Missing core object: $$obj"; \
		fi; \
	done; \
	if [ -n "$$AVAILABLE_OBJS" ]; then \
		echo "🔗 Linking with: $$AVAILABLE_OBJS"; \
		$(CXX) $(CXXFLAGS) -o test_minimal $$AVAILABLE_OBJS $(FMT_OBJS) $(LDFLAGS) 2>&1 || \
		echo "❌ Minimal linking failed - check error messages above"; \
		if [ -f test_minimal ]; then \
			echo "✅ Minimal executable created: test_minimal"; \
			rm -f test_minimal; \
		fi; \
	else \
		echo "❌ No core objects available for testing"; \
	fi

# Clean debug files
debug-clean:
	@echo "🧹 Cleaning debug analysis files..."
	@rm -f symbols.txt undefined_symbols.txt undefined_symbols_raw.txt
	@rm -f undefined_symbols_demangled.txt link.map test_minimal
	@echo "✅ Debug files cleaned"

# Detailed analysis of linking issues with actionable recommendations
debug-recommendations:
	@echo "📋 DEMI Engine Linking Analysis & Recommendations"
	@echo "═══════════════════════════════════════════════"
	@if [ ! -f undefined_symbols_demangled.txt ]; then \
		echo "⚠️  Running detailed analysis first..."; \
		$(MAKE) debug-undefined-detailed >/dev/null 2>&1; \
	fi
	@echo "🔍 Build Health Check:"
	@echo ""
	@if [ -f $(TARGET) ]; then \
		echo "✅ EXECUTABLE BUILD: Success"; \
		echo "   📄 Target: $(TARGET)"; \
		echo "   📊 Size: $$(stat -c%s $(TARGET) 2>/dev/null || echo 'unknown') bytes"; \
		echo "   🧪 Basic test: $$(timeout 5s $(TARGET) --version >/dev/null 2>&1 && echo 'PASS' || echo 'FAIL')"; \
	else \
		echo "❌ EXECUTABLE BUILD: Failed"; \
		echo "   🎯 Target missing: $(TARGET)"; \
		echo "   🔧 Run 'make all' to rebuild"; \
	fi
	@echo ""
	@echo "📊 Symbol Analysis Summary:"
	@echo "   � Individual object undefined symbols: $$(grep -c 'handle_.*(' undefined_symbols_demangled.txt) (NORMAL)"
	@echo "   🔗 These resolve during final linking phase"
	@echo "   � Only investigate if build/execution fails"
	@echo ""
	@echo "🛠️  DEBUGGING WORKFLOW:"
	@echo ""
	@echo "  🎯 1. For Build Failures:"
	@echo "     make debug-linking        # Verbose linker output"
	@echo "     make debug-minimal-link   # Test minimal core components"
	@echo ""
	@echo "  🎯 2. For Runtime Issues:"
	@echo "     ./$(TARGET) --debug --verbose  # Run with debug output"
	@echo "     valgrind ./$(TARGET) [args]    # Memory debugging"
	@echo ""
	@echo "  🎯 3. For Performance Analysis:"
	@echo "     make benchmark             # Run performance tests"
	@echo "     perf record ./$(TARGET) [args]  # Profile execution"
	@echo ""
	@echo "✨ BUILD STATUS: $$([ -f $(TARGET) ] && echo "HEALTHY ✅" || echo "BROKEN ❌")"

# =============================================================================
# Dependency Management
# =============================================================================

# Include dependency files (suppress errors if they don't exist)
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),help)
ifneq ($(MAKECMDGOALS),prereqs)
-include $(ALL_DEPS)
endif
endif
endif

# =============================================================================
# Codacy Code Quality Integration
# =============================================================================
# Codacy Cloud CLI: npm install -g @codacy/codacy-cloud-cli
# API token from: https://app.codacy.com → Account → API Tokens
# Set via: export CODACY_API_TOKEN=cc_...
# Or pass per-command: CODACY_API_TOKEN=cc_... make codacy-issues

CODACY_API_TOKEN ?=
CODACY_ORG := bobrossrtx
CODACY_REPO := demi

# Install the Codacy Cloud CLI (npm)
codacy-install:
	@echo "📦 Installing Codacy Cloud CLI..."
	@if command -v codacy >/dev/null 2>&1; then \
		echo "✅ Codacy CLI already installed: $$(codacy --version 2>&1 || true)"; \
	elif command -v npm >/dev/null 2>&1; then \
		npm install -g @codacy/codacy-cloud-cli; \
		echo "✅ Installed. Run 'codacy login' to authenticate."; \
	else \
		echo "❌ npm not found. Install Node.js first: https://nodejs.org"; \
		exit 1; \
	fi

# Authenticate with Codacy (interactive or via env var)
codacy-login:
	@if [ -n "$(CODACY_API_TOKEN)" ]; then \
		echo "✅ Using CODACY_API_TOKEN from environment"; \
	elif [ -f ~/.codacy/credentials ]; then \
		echo "✅ Found stored credentials"; \
	else \
		echo "🔐 Run: codacy login"; \
		echo "   Or set: export CODACY_API_TOKEN=cc_..."; \
	fi

# Add this repo to Codacy (one-time setup)
codacy-add:
	@echo "🔗 Adding repository to Codacy..."
	@if [ -z "$(CODACY_API_TOKEN)" ]; then \
		echo "❌ Set CODACY_API_TOKEN first: export CODACY_API_TOKEN=cc_..."; \
		exit 1; \
	fi
	codacy repositories gh $(CODACY_ORG) $(CODACY_REPO) --add

# View all issues grouped by severity
codacy-issues:
	@if [ -z "$(CODACY_API_TOKEN)" ]; then \
		echo "❌ Set CODACY_API_TOKEN: export CODACY_API_TOKEN=cc_..."; \
		exit 1; \
	fi
	codacy issues gh $(CODACY_ORG) $(CODACY_REPO)

# View critical/high issues only
codacy-critical:
	@if [ -z "$(CODACY_API_TOKEN)" ]; then \
		echo "❌ Set CODACY_API_TOKEN: export CODACY_API_TOKEN=cc_..."; \
		exit 1; \
	fi
	codacy issues gh $(CODACY_ORG) $(CODACY_REPO) --severities Critical,High

# View security findings
codacy-findings:
	@if [ -z "$(CODACY_API_TOKEN)" ]; then \
		echo "❌ Set CODACY_API_TOKEN: export CODACY_API_TOKEN=cc_..."; \
		exit 1; \
	fi
	codacy findings gh $(CODACY_ORG) $(CODACY_REPO)

# List configured tools
codacy-tools:
	@if [ -z "$(CODACY_API_TOKEN)" ]; then \
		echo "❌ Set CODACY_API_TOKEN: export CODACY_API_TOKEN=cc_..."; \
		exit 1; \
	fi
	codacy tools gh $(CODACY_ORG) $(CODACY_REPO)

# Trigger a reanalysis of the latest commit
codacy-reanalyze:
	@if [ -z "$(CODACY_API_TOKEN)" ]; then \
		echo "❌ Set CODACY_API_TOKEN: export CODACY_API_TOKEN=cc_..."; \
		exit 1; \
	fi
	codacy repositories gh $(CODACY_ORG) $(CODACY_REPO) --reanalyze
	@echo "✅ Reanalysis triggered"

# Run Cppcheck locally (faster feedback without waiting for Codacy servers)
codacy-local:
	@echo "🔍 Running Cppcheck locally..."
	@if command -v cppcheck >/dev/null 2>&1; then \
		cppcheck --enable=all --std=c++17 --quiet \
			-I include -I extern/fmt/include \
			--suppress=missingIncludeSystem \
			src/ 2>&1 || true; \
		echo "✅ Local cppcheck complete"; \
	else \
		echo "❌ cppcheck not installed. Run: sudo apt install cppcheck"; \
		echo "   Or use: make codacy-reanalyze (runs on Codacy servers)"; \
	fi

# Show Codacy setup guide
codacy-info:
	@echo "📋 Codacy Integration for Demi Engine"
	@echo "════════════════════════════════════════"
	@echo ""
	@echo "🔧 One-time setup:"
	@echo "  1. Sign up at https://www.codacy.com"
	@echo "  2. Add repo: https://app.codacy.com → Add repository → bobrossrtx/demi"
	@echo "  3. Get API token: Account → API Tokens"
	@echo "  4. make codacy-install           # Install CLI"
	@echo "  5. export CODACY_API_TOKEN=cc_... # Set token"
	@echo "  6. make codacy-add               # Link repo via CLI"
	@echo ""
	@echo "📄 Configuration: .codacy.yml"
	@echo "   - Cppcheck engine configured for C++"
	@echo "   - Duplication detection tuned (minTokenMatch: 60)"
	@echo "   - build/, bin/, extern/ excluded"
	@echo ""
	@echo "🎯 Make Targets:"
	@echo "   make codacy-install     - Install Codacy Cloud CLI"
	@echo "   make codacy-add         - Link repo to Codacy"
	@echo "   make codacy-issues      - List all issues"
	@echo "   make codacy-critical    - Critical/high issues only"
	@echo "   make codacy-findings    - Security findings"
	@echo "   make codacy-tools       - List enabled tools"
	@echo "   make codacy-reanalyze   - Trigger reanalysis"
	@echo "   make codacy-local       - Run cppcheck locally"
	@echo "   make codacy-info        - This help"
	@echo ""
	@echo "🔗 Repo: https://github.com/bobrossrtx/demi"

# =============================================================================
# SonarCloud / SonarQube Integration
# =============================================================================
# Config: sonar-project.properties
# Prerequisites: sonar-scanner CLI installed
#   npm install -g sonarqube-scanner
# Or use Docker: make sonar-docker

SONAR_TOKEN ?=
SONAR_HOST ?= https://sonarcloud.io
SONAR_ORG ?= bobrossrtx

# Run SonarScanner (requires sonar-scanner CLI + SONAR_TOKEN)
sonar:
	@echo "🔍 SonarCloud Analysis - Demi Engine"
	@echo "═══════════════════════════════════════"
	@if ! command -v sonar-scanner >/dev/null 2>&1; then \
		echo "❌ sonar-scanner not found. Install it:"; \
		echo "   npm install -g sonarqube-scanner"; \
		echo "   Or use Docker: make sonar-docker"; \
		exit 1; \
	fi
	@if [ -z "$(SONAR_TOKEN)" ]; then \
		echo "❌ SONAR_TOKEN not set."; \
		echo "   export SONAR_TOKEN=your_sonarcloud_token"; \
		echo "   Get it from: https://sonarcloud.io/account/security"; \
		exit 1; \
	fi
	@if [ ! -f compile_commands.json ]; then \
		echo "📦 Generating compile_commands.json..."; \
		$(MAKE) compile_commands.json; \
	fi
	@echo "🔧 Project: $(SONAR_ORG)/demi"
	@echo "📊 Running analysis..."
	sonar-scanner \
		-Dsonar.host.url=$(SONAR_HOST) \
		-Dsonar.token=$(SONAR_TOKEN) \
		-Dsonar.projectKey=$(SONAR_ORG)_demi \
		-Dsonar.organization=$(SONAR_ORG)
	@echo "✅ SonarCloud analysis complete"
	@echo "📊 View results: https://sonarcloud.io/dashboard?id=$(SONAR_ORG)_demi"

# Run SonarScanner via Docker
sonar-docker:
	@echo "🐳 SonarCloud via Docker"
	@echo "═══════════════════════════════════════"
	@if ! command -v docker >/dev/null 2>&1; then \
		echo "❌ Docker not installed."; \
		exit 1; \
	fi
	@if [ -z "$(SONAR_TOKEN)" ]; then \
		echo "❌ SONAR_TOKEN not set."; \
		exit 1; \
	fi
	@if [ ! -f compile_commands.json ]; then \
		$(MAKE) compile_commands.json; \
	fi
	docker run --rm \
		-v "$(CURDIR):/usr/src" \
		-e SONAR_TOKEN="$(SONAR_TOKEN)" \
		sonarsource/sonar-scanner-cli:latest \
		-Dsonar.host.url=$(SONAR_HOST) \
		-Dsonar.projectKey=$(SONAR_ORG)_demi

# Run OWASP dependency check (optional — finds known CVEs in dependencies)
sonar-check-deps:
	@echo "🔒 OWASP Dependency Check..."
	@if command -v dependency-check.sh >/dev/null 2>&1; then \
		dependency-check.sh --project "Demi Engine" --scan . --out .; \
	else \
		echo "⚠️  dependency-check not installed."; \
		echo "   Download: https://owasp.org/dependency-check"; \
	fi

# Sonar setup guide
sonar-info:
	@echo "📋 SonarCloud Integration for Demi Engine"
	@echo "════════════════════════════════════════"
	@echo ""
	@echo "🔧 One-time setup:"
	@echo "  1. Sign up at https://sonarcloud.io"
	@echo "  2. Create project for bobrossrtx/demi"
	@echo "  3. Get token: https://sonarcloud.io/account/security"
	@echo "  4. export SONAR_TOKEN=squ_...\""
	@echo "  5. npm install -g sonarqube-scanner"
	@echo "  6. make sonar"
	@echo ""
	@echo "📄 Configuration: sonar-project.properties"
	@echo "   - C++17, compile_commands.json-based analysis"
	@echo "   - extern/, build/, bin/ excluded"
	@echo ""
	@echo "🎯 Make Targets:"
	@echo "   make sonar             - Run SonarCloud analysis"
	@echo "   make sonar-docker      - Run via Docker"
	@echo "   make sonar-info        - This help"
	@echo ""
	@echo "🔗 Dashboard: https://sonarcloud.io/dashboard?id=bobrossrtx_demi"

# =============================================================================
# CodeRabbit AI Code Review Integration
# =============================================================================
# Config: .coderabbit.yaml
# CodeRabbit is an AI-powered PR review bot. Install at:
#   https://github.com/apps/coderabbitai
# Once installed, it auto-reviews every PR based on .coderabbit.yaml.

# Show CodeRabbit setup guide
coderabbit-info:
	@echo "📋 CodeRabbit Integration for Demi Engine"
	@echo "═════════════════════════════════════════"
	@echo ""
	@echo "🔧 One-time setup:"
	@echo "  1. Install GitHub App: https://github.com/apps/coderabbitai"
	@echo "  2. Grant access to bobrossrtx/demi"
	@echo "  3. Done — CodeRabbit auto-reviews every PR"
	@echo ""
	@echo "📄 Configuration: .coderabbit.yaml"
	@echo "   - Assertive review profile for thorough feedback"
	@echo "   - Clang static analysis enabled for C/C++"
	@echo "   - checkmake enabled for Makefile linting"
	@echo "   - Path-specific instructions for engine/assembler/test code"
	@echo "   - Automatic docstring and unit test generation"
	@echo ""
	@echo "🔗 Dashboard: https://app.coderabbit.ai"

# =============================================================================
# Community Promotion & Growth
# =============================================================================

# Promotion checklist — actionable steps to grow the project
promote:
	@echo "🚀 Demi Promotion & Growth Checklist"
	@echo "═══════════════════════════════════════"
	@echo ""
	@echo "📋 PHASE 1 — Project Polish (this week)"
	@echo "  [ ] Enable GitHub Discussions: Settings → Features → Discussions"
	@echo "  [ ] Add repo topics: compiler, virtual-machine, c-plus-plus-17, assembler, programming-language"
	@echo "  [ ] Pin 'good first issue' and 'help wanted' issues"
	@echo "  [ ] Add demo GIF to README"
	@echo "  [ ] Create social preview image (1200x630px)"
	@echo ""
	@echo "📣 PHASE 2 — Social Launch"
	@echo "  [ ] Hacker News: 'Show HN: Demi — a customizable VM and compiler in C++17'"
	@echo "  [ ] Reddit: r/programming, r/cpp, r/compilers, r/ProgrammingLanguages"
	@echo "  [ ] Twitter/X: Thread showing architecture + demo"
	@echo "  [ ] Lobsters: tags: compilers, performance"
	@echo "  [ ] Dev.to: 'Building a Virtual Machine from Scratch in C++17'"
	@echo ""
	@echo "📝 PHASE 3 — Content (ongoing)"
	@echo "  [ ] Blog: 'Why I'm Building a Custom VM'"
	@echo "  [ ] Blog: 'The Demi Instruction Set — Design Decisions'"
	@echo "  [ ] Video: 5-min demo (assembly → execution → debugging)"
	@echo "  [ ] Tutorial: 'Writing Your First Demi Assembly Program'"
	@echo "  [ ] Comparison: 'Demi vs QBE vs Cranelift'"
	@echo ""
	@echo "👥 PHASE 4 — Community (ongoing)"
	@echo "  [ ] Respond to every issue/PR within 48 hours"
	@echo "  [ ] Thank first-time contributors publicly"
	@echo "  [ ] Add all-contributors bot"
	@echo "  [ ] Hacktoberfest participation"
	@echo "  [ ] Reach out to CS students interested in compilers/VMs"
	@echo ""
	@echo "🌱 PHASE 5 — Sustained Growth"
	@echo "  [ ] Monthly 'What's New in Demi' updates"
	@echo "  [ ] Conference talks: CppCon, FOSDEM, meetups"
	@echo "  [ ] GitHub Sponsors setup"
	@echo "  [ ] WebAssembly playground for instant demos"
	@echo "  [ ] Dedicated website/landing page"
	@echo ""
	@echo "📄 Templates: docs/SOCIAL_TEMPLATES.md"
	@echo "📄 Full plan: docs/PROMOTION_PLAN.md"
	@echo ""
	@echo "🔗 Repo: https://github.com/bobrossrtx/demi"

# Quick health check for community readiness
promote-check:
	@echo "🏥 Community Health Check"
	@echo "═══════════════════════════════════════"
	@echo ""
	@echo "📄 Config files:"
	@for f in README.md CONTRIBUTING.md CODE_OF_CONDUCT.md SECURITY.md \
	         .github/pull_request_template.md .codacy.yml .coderabbit.yaml \
	         sonar-project.properties; do \
		if [ -f "$$f" ]; then echo "  ✅ $$f"; else echo "  ❌ $$f MISSING"; fi; \
	done
	@echo ""
	@echo "📋 Issue templates:"
	@for f in .github/ISSUE_TEMPLATE/bug-report.md \
	         .github/ISSUE_TEMPLATE/feature_request.md \
	         .github/ISSUE_TEMPLATE/question.md \
	         .github/ISSUE_TEMPLATE/config.yml; do \
		if [ -f "$$f" ]; then echo "  ✅ $$f"; else echo "  ❌ $$f MISSING"; fi; \
	done
	@echo ""
	@echo "⚙️  CI workflows:"
	@for f in .github/workflows/build.yml .github/workflows/flawfinder.yml; do \
		if [ -f "$$f" ]; then echo "  ✅ $$f"; else echo "  ❌ $$f MISSING"; fi; \
	done
	@echo ""
	@echo "🔗 GitHub Setup Checklist:"
	@echo "  [ ] Discussions enabled"
	@echo "  [ ] Topics added"
	@echo "  [ ] Good first issues tagged"
	@echo "  [ ] Project board created"
	@echo "  [ ] Social preview image uploaded"
	@echo ""
	@echo "📂 All promotion docs: docs/PROMOTION_PLAN.md"

# =============================================================================
# Special Targets and Rules
# =============================================================================

# Install to system (optional)
install: $(TARGET)
	@echo "📦 Installing DEMI Engine..."
	@sudo install -D $(TARGET) /usr/local/bin/demi-engine
	@echo "✅ Installed to /usr/local/bin/demi-engine"

# Generate compile_commands.json for clangd IntelliSense
compile_commands.json: Makefile generate_compile_commands.py
	@python3 generate_compile_commands.py

# Phony targets
.PHONY: clangd-setup compile_commands.json
clangd-setup: compile_commands.json

# Prevent make from deleting intermediate files
.PRECIOUS: $(BUILD_DIR)/%.o $(BUILD_DIR)/%.d

# Silent rules for cleaner output
ifndef VERBOSE
.SILENT:
endif