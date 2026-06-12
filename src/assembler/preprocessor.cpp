#include "preprocessor.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <filesystem>
#include <iostream>

namespace Assembler {

Preprocessor::Preprocessor() : current_line(0) {
    // Add some predefined macros
    define_macro("__DEMI_ENGINE__", "1");
    define_macro("__DEMI_VERSION__", "1.0");
}

std::string Preprocessor::preprocess(const std::string& source, const std::string& base_path) {
    errors.clear();
    
    // Only clear included_files for top-level calls (when it's empty)
    bool is_top_level = included_files.empty();
    if (is_top_level) {
        included_files.clear();
    }
    
    // Clear conditional stack for top-level calls only
    if (is_top_level) {
        while (!conditional_stack.empty()) {
            conditional_stack.pop();
        }
    }
    
    std::istringstream stream(source);
    std::string line;
    std::string result;
    current_line = 0;
    
    while (std::getline(stream, line)) {
        current_line++;
        std::string trimmed_line = trim(line);
        
        // Handle .rept / .endr blocks
        if (trimmed_line.substr(0, 5) == ".rept") {
            if (!should_include_line()) {
                // Skip lines until .endr
                while (std::getline(stream, line)) {
                    current_line++;
                    if (trim(line) == ".endr") break;
                }
                continue;
            }
            std::string count_str = trim(trimmed_line.substr(5));
            int count = 0;
            try { count = std::stoi(count_str); }
            catch (...) { add_error("Invalid .rept count: " + count_str); continue; }
            if (count < 0) { add_error("Negative .rept count"); continue; }
            
            // Collect body lines until .endr
            std::vector<std::string> body;
            int endr_line = 0;
            while (std::getline(stream, line)) {
                current_line++;
                std::string tl = trim(line);
                if (tl == ".endr") { endr_line = current_line; break; }
                body.push_back(line);
            }
            if (endr_line == 0) {
                add_error(".rept without matching .endr at line " + std::to_string(current_line - (int)body.size()));
                continue;
            }
            
            // Output body count times
            for (int i = 0; i < count; i++) {
                for (const auto& body_line : body) {
                    std::string processed = process_line(body_line, base_path);
                    if (!processed.empty()) {
                        result += processed + "\n";
                    }
                }
            }
            continue;
        }
        
        if (trimmed_line == ".endr") {
            add_error("Stray .endr without .rept at line " + std::to_string(current_line));
            continue;
        }
        
        // Handle .macro / .endm blocks
        if (trimmed_line.substr(0, 6) == ".macro") {
            if (!should_include_line()) {
                while (std::getline(stream, line)) {
                    current_line++;
                    if (trim(line) == ".endm") break;
                }
                continue;
            }
            std::string rest = trim(trimmed_line.substr(6));
            std::istringstream rs(rest);
            std::string macro_name;
            rs >> macro_name;
            if (macro_name.empty()) {
                add_error(".macro without name");
                continue;
            }
            std::vector<std::string> params;
            std::string param;
            while (rs >> param) {
                if (!param.empty() && param.back() == ',') param.pop_back();
                if (!param.empty()) params.push_back(param);
            }
            std::vector<std::string> body;
            int endm_line = 0;
            while (std::getline(stream, line)) {
                current_line++;
                if (trim(line) == ".endm") { endm_line = current_line; break; }
                body.push_back(line);
            }
            if (endm_line == 0) {
                add_error(".macro without matching .endm");
                continue;
            }
            std::string body_str;
            for (size_t i = 0; i < body.size(); i++) {
                if (i > 0) body_str += "\n";
                body_str += body[i];
            }
            macros[macro_name] = MacroDefinition(macro_name, params, body_str);
            continue;
        }
        
        if (trimmed_line == ".endm") {
            add_error("Stray .endm without .macro at line " + std::to_string(current_line));
            continue;
        }
        
        // Check for macro invocation: first word matches a function macro
        std::string first_word;
        { std::istringstream ls(trimmed_line); ls >> first_word; }
        if (!first_word.empty() && first_word[0] != '.') {
            auto it = macros.find(first_word);
            if (it != macros.end() && it->second.is_function_macro) {
                std::string rest_of_line = trim(trimmed_line.substr(first_word.length()));
                std::vector<std::string> args;
                if (!rest_of_line.empty()) {
                    std::istringstream as(rest_of_line);
                    std::string arg;
                    while (std::getline(as, arg, ',')) args.push_back(trim(arg));
                }
                std::string expanded = expand_function_macro(it->second, args);
                if (!expanded.empty()) {
                    // Re-preprocess to handle nested macros and .equ
                    result += preprocess(expanded, base_path);
                }
                continue;
            }
        }
        
        std::string processed_line = process_line(line, base_path);
        if (!processed_line.empty()) {
            result += processed_line + "\n";
        }
    }
    
    // Check for unclosed conditionals only at top level
    if (is_top_level && !conditional_stack.empty()) {
        add_error("Unclosed conditional directive(s) at end of file");
    }
    
    return result;
}

void Preprocessor::define_macro(const std::string& name, const std::string& value) {
    macros[name] = MacroDefinition(name, value);
}

bool Preprocessor::is_defined(const std::string& name) const {
    return macros.find(name) != macros.end();
}

std::string Preprocessor::process_line(const std::string& line, const std::string& base_path) {
    std::string trimmed = trim(line);
    
    // Handle preprocessor directives
    if (trimmed.empty() || trimmed[0] == '#') {
        // Comments and empty lines - pass through if conditions allow
        return should_include_line() ? line : "";
    }
    
    if (trimmed.substr(0, 8) == ".include") {
        if (should_include_line()) {
            std::string filename = trimmed.substr(8);
            filename = trim(filename);
            if (filename.length() >= 2 && filename[0] == '"' && filename[filename.length()-1] == '"') {
                filename = filename.substr(1, filename.length() - 2);
            }
            std::string output;
            handle_include(filename, base_path, output);
            return output;
        }
        return "";
    } else if (trimmed.substr(0, 7) == ".define") {
        if (should_include_line()) {
            std::string definition = trimmed.substr(7);
            handle_define(trim(definition));
        }
        return "";
    } else if (trimmed.substr(0, 6) == ".undef") {
        if (should_include_line()) {
            std::string name = trimmed.substr(6);
            handle_undef(trim(name));
        }
        return "";
    } else if (trimmed.substr(0, 6) == ".ifdef") {
        std::string name = trimmed.substr(6);
        bool condition = handle_ifdef(trim(name));
        ConditionalState state;
        state.is_true = condition;
        state.has_matched = condition;
        state.in_else = false;
        conditional_stack.push(state);
        return "";
    } else if (trimmed.substr(0, 7) == ".ifndef") {
        std::string name = trimmed.substr(7);
        bool condition = handle_ifndef(trim(name));
        ConditionalState state;
        state.is_true = condition;
        state.has_matched = condition;
        state.in_else = false;
        conditional_stack.push(state);
        return "";
    } else if (trimmed.substr(0, 5) == ".elif") {
        if (conditional_stack.empty()) {
            add_error(".elif without matching .ifdef/.ifndef");
            return "";
        }
        std::string condition_str = trimmed.substr(5);
        handle_elif(trim(condition_str));
        return "";
    } else if (trimmed.substr(0, 5) == ".else") {
        handle_else();
        return "";
    } else if (trimmed.substr(0, 6) == ".endif") {
        handle_endif();
        return "";
    }
    
    // Regular line - expand macros and return if conditions allow
    if (should_include_line()) {
        return expand_macros(line);
    }
    
    return "";
}

std::string Preprocessor::expand_macros(const std::string& text) {
    std::string result = text;
    
    for (const auto& [name, macro] : macros) {
        if (macro.is_function_macro) {
            auto& pattern = macro_regex_cache_[name];
            if (pattern == nullptr) {
                pattern = std::make_unique<std::regex>(name + R"(\s*\(([^)]*)\))");
            }
            std::smatch match;
            
            while (std::regex_search(result, match, *pattern)) {
                std::string args_str = match[1].str();
                std::vector<std::string> args = parse_macro_args(args_str);
                std::string expanded = expand_function_macro(macro, args);
                result.replace(match.position(), match.length(), expanded);
            }
        } else {
            // Simple macro - replace all occurrences
            size_t pos = 0;
            while ((pos = result.find(name, pos)) != std::string::npos) {
                // Check that it's a complete word (not part of another identifier)
                bool is_word_start = (pos == 0 || (!std::isalnum(result[pos-1]) && result[pos-1] != '_'));
                bool is_word_end = (pos + name.length() >= result.length() || 
                                  (!std::isalnum(result[pos + name.length()]) && result[pos + name.length()] != '_'));
                
                if (is_word_start && is_word_end) {
                    std::string expanded_body = macro.body;
                    // Convert \n escape sequences to actual newlines
                    size_t newline_pos = 0;
                    while ((newline_pos = expanded_body.find("\\n", newline_pos)) != std::string::npos) {
                        expanded_body.replace(newline_pos, 2, "\n");
                        newline_pos += 1;
                    }
                    result.replace(pos, name.length(), expanded_body);
                    pos += expanded_body.length();
                } else {
                    pos++;
                }
            }
        }
    }
    
    return result;
}

std::string Preprocessor::expand_function_macro(const MacroDefinition& macro, const std::vector<std::string>& args) {
    if (args.size() != macro.parameters.size()) {
        add_error("Macro " + macro.name + " expects " + std::to_string(macro.parameters.size()) + 
                 " arguments, got " + std::to_string(args.size()));
        return "";
    }
    
    std::string result = macro.body;
    
    // Replace parameter references with arguments
    for (size_t i = 0; i < macro.parameters.size(); ++i) {
        const std::string& param = macro.parameters[i];
        const std::string& arg = args[i];
        
        size_t pos = 0;
        while ((pos = result.find(param, pos)) != std::string::npos) {
            // Check that it's a complete word
            bool is_word_start = (pos == 0 || (!std::isalnum(result[pos-1]) && result[pos-1] != '_'));
            bool is_word_end = (pos + param.length() >= result.length() || 
                              (!std::isalnum(result[pos + param.length()]) && result[pos + param.length()] != '_'));
            
            if (is_word_start && is_word_end) {
                result.replace(pos, param.length(), arg);
                pos += arg.length();
            } else {
                pos++;
            }
        }
    }
    
    // Convert \n escape sequences to actual newlines
    size_t pos = 0;
    while ((pos = result.find("\\n", pos)) != std::string::npos) {
        result.replace(pos, 2, "\n");
        pos += 1;
    }
    
    return result;
}

void Preprocessor::handle_include(const std::string& filename, const std::string& base_path, std::string& output) {
    // Resolve path
    std::filesystem::path include_path = filename;
    if (include_path.is_relative()) {
        if (!base_path.empty()) {
            include_path = std::filesystem::path(base_path) / include_path;
        }
    }
    
    // Canonicalize to prevent path traversal (../ sequences)
    std::error_code ec;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(include_path, ec);
    if (ec) {
        add_error("Invalid include path: " + include_path.string());
        return;
    }
    
    // Verify canonical path stays within base directory
    if (!base_path.empty()) {
        std::filesystem::path canonical_base = std::filesystem::weakly_canonical(base_path, ec);
        if (!ec) {
            auto [base_end, path_it] = std::mismatch(
                canonical_base.begin(), canonical_base.end(),
                canonical.begin(), canonical.end());
            if (base_end != canonical_base.end()) {
                add_error("Path traversal in include: " + filename + " resolves outside base directory");
                return;
            }
        }
    }
    
    std::string full_path = canonical.string();
    
    // Check for circular includes
    if (std::find(included_files.begin(), included_files.end(), full_path) != included_files.end()) {
        add_error("Circular include detected: " + full_path);
        return;
    }
    
    // Read and preprocess the included file
    std::string content = read_file(full_path);
    if (content.empty() && has_errors()) {
        return; // Error already added in read_file
    }
    
    // Track inclusion to prevent circular includes
    included_files.push_back(full_path);
    
    // Recursively preprocess the included content
    std::string old_file = current_file;
    size_t old_line = current_line;
    current_file = full_path;
    
    std::string preprocessed = preprocess(content, std::filesystem::path(full_path).parent_path().string());
    output = preprocessed;
    
    current_file = old_file;
    current_line = old_line;
    
    // Remove from tracking
    included_files.pop_back();
}

void Preprocessor::handle_define(const std::string& definition) {
    // Parse macro definition
    std::istringstream stream(definition);
    std::string name;
    stream >> name;
    
    if (name.empty()) {
        add_error("Empty macro name in .define directive");
        return;
    }
    
    // Check for function macro
    if (name.find('(') != std::string::npos) {
        // Function macro: NAME(param1, param2) body
        size_t paren_pos = name.find('(');
        std::string macro_name = name.substr(0, paren_pos);
        
        size_t close_paren = definition.find(')', paren_pos);
        if (close_paren == std::string::npos) {
            add_error("Unclosed parameter list in macro definition");
            return;
        }
        
        std::string params_str = definition.substr(paren_pos + 1, close_paren - paren_pos - 1);
        std::vector<std::string> parameters = parse_macro_args(params_str);
        
        // Get body after the closing paren
        std::string body = definition.substr(close_paren + 1);
        body = trim(body);
        
        macros[macro_name] = MacroDefinition(macro_name, parameters, body);
    } else {
        // Simple macro: NAME body
        std::string body = definition.substr(name.length());
        body = trim(body);
        
        macros[name] = MacroDefinition(name, body);
    }
}

void Preprocessor::handle_undef(const std::string& name) {
    macros.erase(name);
}

bool Preprocessor::handle_ifdef(const std::string& name) {
    return is_defined(name);
}

bool Preprocessor::handle_ifndef(const std::string& name) {
    return !is_defined(name);
}

void Preprocessor::handle_elif(const std::string& condition) {
    if (conditional_stack.empty()) {
        add_error(".elif without matching .ifdef/.ifndef");
        return;
    }
    
    ConditionalState& state = conditional_stack.top();
    if (state.in_else) {
        add_error(".elif after .else");
        return;
    }
    
    bool cond_result = evaluate_condition(condition);
    state.is_true = !state.has_matched && cond_result;
    if (state.is_true) {
        state.has_matched = true;
    }
}

void Preprocessor::handle_else() {
    if (conditional_stack.empty()) {
        add_error(".else without matching .ifdef/.ifndef");
        return;
    }
    
    ConditionalState& state = conditional_stack.top();
    if (state.in_else) {
        add_error("Multiple .else blocks in same conditional");
        return;
    }
    
    state.in_else = true;
    state.is_true = !state.has_matched;
}

void Preprocessor::handle_endif() {
    if (conditional_stack.empty()) {
        add_error(".endif without matching .ifdef/.ifndef");
        return;
    }
    
    conditional_stack.pop();
}

bool Preprocessor::evaluate_condition(const std::string& condition) {
    std::string trimmed = trim(condition);
    
    // Simple condition evaluation - just check if macro is defined
    if (trimmed.length() >= 10 && trimmed.substr(0, 8) == "defined(" && trimmed.back() == ')') {
        std::string macro_name = trimmed.substr(8, trimmed.length() - 9);
        macro_name = trim(macro_name);
        return is_defined(macro_name);
    }
    
    // Direct macro name check
    return is_defined(trimmed);
}

std::string Preprocessor::read_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        add_error("Cannot open file: " + filepath);
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::vector<std::string> Preprocessor::parse_macro_args(const std::string& args_str) {
    std::vector<std::string> args;
    if (args_str.empty()) {
        return args;
    }
    
    std::stringstream stream(args_str);
    std::string arg;
    
    while (std::getline(stream, arg, ',')) {
        args.push_back(trim(arg));
    }
    
    return args;
}

std::string Preprocessor::trim(const std::string& str) {
    const char* ws = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(ws);
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(ws);
    return str.substr(start, end - start + 1);
}

void Preprocessor::add_error(const std::string& message) {
    std::string error_msg = message;
    if (!current_file.empty()) {
        error_msg = current_file + ":" + std::to_string(current_line) + ": " + message;
    } else if (current_line > 0) {
        error_msg = "Line " + std::to_string(current_line) + ": " + message;
    }
    errors.push_back(error_msg);
}

bool Preprocessor::should_include_line() {
    if (conditional_stack.empty()) {
        return true;
    }
    return conditional_stack.top().is_true;
}

} // namespace Assembler