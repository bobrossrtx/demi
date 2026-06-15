#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace Assembler {

// Forward declarations
class ASTNode;
class Statement;
class Expression;
class Instruction;
class Directive;
class Label;

// AST node types
enum class ASTNodeType {
    PROGRAM,
    INSTRUCTION,
    DIRECTIVE,
    LABEL,
    EXPRESSION,
    REGISTER,
    IMMEDIATE,
    FLOAT,
    ST_REGISTER,
    MEMORY_REF,
    IDENTIFIER,
    STRING_LITERAL,
    TEST_CASE,
    TEST_ASSERTION
};

// Base AST node
class ASTNode {
public:
    ASTNodeType type;
    size_t line;
    size_t column;
    
    explicit ASTNode(ASTNodeType t, size_t ln = 0, size_t col = 0) 
        : type(t), line(ln), column(col) {}
    virtual ~ASTNode() = default;
};

// Expression types
class Expression : public ASTNode {
public:
    explicit Expression(ASTNodeType t, size_t ln = 0, size_t col = 0) 
        : ASTNode(t, ln, col) {}
};

class RegisterExpression : public Expression {
public:
    std::string name;
    int register_number;  // -1 if not resolved yet
    
    explicit RegisterExpression(const std::string& reg_name, size_t ln = 0, size_t col = 0)
        : Expression(ASTNodeType::REGISTER, ln, col), name(reg_name), register_number(-1) {}
};

class ImmediateExpression : public Expression {
public:
    int64_t value;
    
    explicit ImmediateExpression(int64_t val, size_t ln = 0, size_t col = 0)
        : Expression(ASTNodeType::IMMEDIATE, ln, col), value(val) {}
};

class FloatExpression : public Expression {
public:
    double value;
    
    explicit FloatExpression(double val, size_t ln = 0, size_t col = 0)
        : Expression(ASTNodeType::FLOAT, ln, col), value(val) {}
};

class STRegisterExpression : public Expression {
public:
    uint8_t index;  // ST(0) to ST(7)
    
    explicit STRegisterExpression(uint8_t st_index, size_t ln = 0, size_t col = 0)
        : Expression(ASTNodeType::ST_REGISTER, ln, col), index(st_index) {}
};

class IdentifierExpression : public Expression {
public:
    std::string name;
    
    explicit IdentifierExpression(const std::string& id_name, size_t ln = 0, size_t col = 0)
        : Expression(ASTNodeType::IDENTIFIER, ln, col), name(id_name) {}
};

// Binary expression for operand arithmetic: label + 4, (2 * 8), etc.
class BinaryExpression : public Expression {
public:
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
    char op;  // '+', '-', '*', '/'

    BinaryExpression(std::unique_ptr<Expression> l, char o, std::unique_ptr<Expression> r, size_t ln = 0, size_t col = 0)
        : Expression(ASTNodeType::EXPRESSION, ln, col), left(std::move(l)), op(o), right(std::move(r)) {}
};

class StringLiteralExpression : public Expression {
public:
    std::string value;
    
    explicit StringLiteralExpression(const std::string& str_value, size_t ln = 0, size_t col = 0)
        : Expression(ASTNodeType::STRING_LITERAL, ln, col), value(str_value) {}
};

class MemoryReferenceExpression : public Expression {
public:
    std::unique_ptr<Expression> base;           // [base + offset]
    std::unique_ptr<Expression> offset;        // optional offset
        std::unique_ptr<Expression> index;         // optional index register/expression
        uint8_t scale = 1;                         // optional scale for indexed addressing
        std::unique_ptr<Expression> displacement;  // optional displacement expression
        std::unique_ptr<IdentifierExpression> symbol; // optional symbol component
        uint8_t size_hint_bits = 0;                // optional size hint for backend lowering
    
    explicit MemoryReferenceExpression(std::unique_ptr<Expression> base_expr, 
                             std::unique_ptr<Expression> offset_expr = nullptr,
                             size_t ln = 0, size_t col = 0)
        : Expression(ASTNodeType::MEMORY_REF, ln, col), 
                    base(std::move(base_expr)), offset(std::move(offset_expr)) {}

        MemoryReferenceExpression(std::unique_ptr<Expression> base_expr,
                                                            std::unique_ptr<Expression> index_expr,
                                                            uint8_t scale_value,
                                                            std::unique_ptr<Expression> displacement_expr,
                                                            std::unique_ptr<IdentifierExpression> symbol_expr,
                                                            uint8_t size_hint,
                                                            size_t ln = 0, size_t col = 0)
                : Expression(ASTNodeType::MEMORY_REF, ln, col),
                    base(std::move(base_expr)),
                    index(std::move(index_expr)),
                    scale(scale_value),
                    displacement(std::move(displacement_expr)),
                    symbol(std::move(symbol_expr)),
                    size_hint_bits(size_hint) {}
};

// Statement types
class Statement : public ASTNode {
public:
    explicit Statement(ASTNodeType t, size_t ln = 0, size_t col = 0) 
        : ASTNode(t, ln, col) {}
};

class Label : public Statement {
public:
    std::string name;
    
    explicit Label(const std::string& label_name, size_t ln = 0, size_t col = 0)
        : Statement(ASTNodeType::LABEL, ln, col), name(label_name) {}
};

class Instruction : public Statement {
public:
    std::string mnemonic;
    std::vector<std::unique_ptr<Expression>> operands;
    
    explicit Instruction(const std::string& mn, size_t ln = 0, size_t col = 0)
        : Statement(ASTNodeType::INSTRUCTION, ln, col), mnemonic(mn) {}
    
    void add_operand(std::unique_ptr<Expression> operand) {
        operands.push_back(std::move(operand));
    }
};

class Directive : public Statement {
public:
    std::string name;
    std::vector<std::unique_ptr<Expression>> arguments;
    
    explicit Directive(const std::string& directive_name, size_t ln = 0, size_t col = 0)
        : Statement(ASTNodeType::DIRECTIVE, ln, col), name(directive_name) {}
    
    void add_argument(std::unique_ptr<Expression> arg) {
        arguments.push_back(std::move(arg));
    }
};

// Test assertion types
enum class TestAssertionType {
    ASSERT_MEM,      // #assert_mem address, expected_value
    ASSERT_REG,      // #assert_reg register, expected_value
    ASSERT_FPU,      // #assert_fpu st_register, expected_value
    ASSERT_OUTPUT,   // #assert_output expected_string
    EXPECT_ERROR     // #expect_error
};

class TestAssertion : public Statement {
public:
    TestAssertionType assertion_type;
    std::vector<std::unique_ptr<Expression>> arguments;
    
    explicit TestAssertion(TestAssertionType type, size_t ln = 0, size_t col = 0)
        : Statement(ASTNodeType::TEST_ASSERTION, ln, col), assertion_type(type) {}
    
    TestAssertion(TestAssertionType type, std::vector<std::unique_ptr<Expression>>&& args, size_t ln = 0, size_t col = 0)
        : Statement(ASTNodeType::TEST_ASSERTION, ln, col), assertion_type(type), arguments(std::move(args)) {}
    
    void add_argument(std::unique_ptr<Expression> arg) {
        arguments.push_back(std::move(arg));
    }
};

class TestCase : public Statement {
public:
    std::string name;
    std::string description;
    std::string author;
    std::string category;
    std::vector<std::string> tags;
    std::string entry_point;
    std::vector<std::unique_ptr<Statement>> body;
    size_t max_steps = 10000;  // Default to 10,000 steps
    size_t max_call_depth = 64;  // Default test mode call depth (vs 256 in production)
    size_t timeout_ms = 0;  // 0 = no timeout
    bool skip = false;  // Skip this test
    
    // Benchmark configuration
    bool is_benchmark = false;
    size_t warmup_iterations = 0;
    size_t iterations = 1;
    std::string measure_type = "time";  // "time", "cycles", "instructions"
    
    // One-time directive tracking
    bool maxsteps_set = false;
    bool maxcalldepth_set = false;
    bool timeout_set = false;
    bool description_set = false;
    bool author_set = false;
    bool category_set = false;
    bool entry_point_set = false;
    
    explicit TestCase(const std::string& test_name, size_t ln = 0, size_t col = 0)
        : Statement(ASTNodeType::TEST_CASE, ln, col), name(test_name) {}
    
    TestCase(const std::string& test_name, std::vector<std::unique_ptr<Statement>>&& test_body, size_t ln = 0, size_t col = 0)
        : Statement(ASTNodeType::TEST_CASE, ln, col), name(test_name), body(std::move(test_body)) {}
    
    void add_statement(std::unique_ptr<Statement> stmt) {
        body.push_back(std::move(stmt));
    }
    
    // Returns false if already set (duplicate directive)
    bool set_description(const std::string& desc) {
        if (description_set) return false;
        description_set = true;
        description = desc;
        return true;
    }
    
    bool set_author(const std::string& auth) {
        if (author_set) return false;
        author_set = true;
        author = auth;
        return true;
    }
    
    bool set_category(const std::string& cat) {
        if (category_set) return false;
        category_set = true;
        category = cat;
        return true;
    }
    
    void add_tag(const std::string& tag) {
        tags.push_back(tag);
    }

    bool set_entry_point(const std::string& ep) {
        if (entry_point_set) return false;
        entry_point_set = true;
        entry_point = ep;
        return true;
    }
    
    bool set_max_steps(size_t steps) {
        if (maxsteps_set) return false;
        maxsteps_set = true;
        max_steps = steps;
        return true;
    }
    
    bool set_max_call_depth(size_t depth) {
        if (maxcalldepth_set) return false;
        maxcalldepth_set = true;
        max_call_depth = depth;
        return true;
    }
    
    bool set_timeout(size_t ms) {
        if (timeout_set) return false;
        timeout_set = true;
        timeout_ms = ms;
        return true;
    }
    
    void set_skip(bool should_skip) {
        skip = should_skip;
    }
    
    bool get_skip() const {
        return skip;
    }
    
    void set_benchmark(bool is_bench) {
        is_benchmark = is_bench;
    }
    
    void set_warmup(size_t warmup) {
        warmup_iterations = warmup;
    }
    
    void set_iterations(size_t iters) {
        iterations = iters;
    }
    
    void set_measure_type(const std::string& type) {
        measure_type = type;
    }
};

// Program (root node)
class Program : public ASTNode {
public:
    std::vector<std::unique_ptr<Statement>> statements;
    std::vector<std::unique_ptr<TestCase>> test_cases;
    
    Program() : ASTNode(ASTNodeType::PROGRAM) {}
    
    void add_statement(std::unique_ptr<Statement> stmt) {
        statements.push_back(std::move(stmt));
    }
    
    void add_test_case(std::unique_ptr<TestCase> test) {
        test_cases.push_back(std::move(test));
    }
};

} // namespace Assembler
