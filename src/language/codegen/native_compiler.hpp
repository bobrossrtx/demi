#pragma once
#include <vector>
#include <cstdint>
#include <string>

namespace DemiLanguage {

struct CodegenResult;

// Compile a .dem source file to native x86-64 ELF executable.
// Returns true on success, false on error (errors printed to stderr).
bool compile_dem_to_native(const std::string& source_path,
                           const std::string& output_path,
                           const CodegenResult& result);

} // namespace DemiLanguage
