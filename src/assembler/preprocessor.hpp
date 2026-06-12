#pragma once
#include "token.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <stack>
#include <filesystem>
#include <memory>
#include <regex>

namespace Assembler {

struct MacroDefinition {
    std::string name;
    std::vector<std::string> parameters;
    std::string body;
    bool is_function_macro;  // true if macro has parameters
    
    MacroDefinition() : is_function_macro(false) {}
    MacroDefinition(const std::string& n, const std::string& b) 
        : name(n), body(b), is_function_macro(false) {}
    MacroDefinition(const std::string& n, const std::vector<std::string>& params, const std::string& b)
        : name(n), parameters(params), body(b), is_function_macro(!params.empty()) {}
};

struct ConditionalState {
    bool is_true;      // Current condition evaluation
    bool has_matched;  // Has any branch matched yet
    bool in_else;      // Currently in else block
};

class Preprocessor {
public:
    Preprocessor();
    
    /**
     * Preprocess the given source text, handling all preprocessor directives
     * @param source The source code to preprocess
     * @param base_path The base directory for relative includes
     * @return Preprocessed source code with directives resolved
     */
    std::string preprocess(const std::string& source, const std::string& base_path = "");
    
    /**
     * Add a predefined macro (e.g., from command line)
     */
    void define_macro(const std::string& name, const std::string& value = "");
    
    /**
     * Check if a macro is defined
     */
    bool is_defined(const std::string& name) const;
    
    /**
     * Get preprocessor errors
     */
    const std::vector<std::string>& get_errors() const { return errors; }
    bool has_errors() const { return !errors.empty(); }
    
    bool evaluate_expression(const std::string& expr);
    
private:
    std::unordered_map<std::string, MacroDefinition> macros;
    std::unordered_map<std::string, std::unique_ptr<std::regex>> macro_regex_cache_;
    std::stack<ConditionalState> conditional_stack;
    std::vector<std::string> included_files;
    std::vector<std::string> errors;
    std::string current_file;
    size_t current_line;
    std::string next_macro_expansion_;  // queued macro expansions
    
    // Preprocessing methods
    std::string process_line(const std::string& line, const std::string& base_path);
    std::string expand_macros(const std::string& text);
    std::string expand_function_macro(const MacroDefinition& macro, const std::vector<std::string>& args);
    
    // Directive handlers
    void handle_include(const std::string& filename, const std::string& base_path, std::string& output);
    void handle_define(const std::string& definition);
    void handle_undef(const std::string& name);
    bool handle_ifdef(const std::string& name);
    bool handle_ifndef(const std::string& name);
    void handle_elif(const std::string& condition);
    void handle_else();
    void handle_endif();
    
    // Utility methods
    int64_t resolve_value(const std::string& s) const;
    std::string read_file(const std::string& filepath);
    std::vector<std::string> parse_macro_args(const std::string& args_str);
    std::string trim(const std::string& str);
    void add_error(const std::string& message);
    bool should_include_line();  // Based on current conditional state
};

} // namespace Assembler