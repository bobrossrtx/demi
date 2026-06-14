# Test demonstrating new test directives
# This file shows .maxcalldepth, .timeout, .skip, and benchmark directives

.test "max_call_depth_test" {
    .description "Tests custom call depth limit (32 instead of default 64)"
    .author "bobrossrtx"
    .category "Test Framework"
    .tag "maxcalldepth"
    .tag "call-stack"
    .maxcalldepth 32
    
    # Recursive function that calls itself 31 times (should succeed)
    LOAD_IMM EAX, 31         # Counter
    LOAD_IMM EBX, 0          # Recursion depth tracker
    
recurse_loop:
    CMP EAX, EBX
    JZ end
    INC EBX
    CALL recurse
    JMP recurse_loop
    
recurse:
    RET
    
end:
    HALT
}

.test "timeout_test" {
    .description "Tests timeout directive (currently informational)"
    .author "bobrossrtx"
    .category "Test Framework"
    .tag "timeout"
    .timeout 1000
    
    # Simple test that completes quickly
    LOAD_IMM EAX, 42
    HALT
    
    .assert_reg EAX, 42
}

.test "skipped_test" {
    .description "This test is skipped"
    .author "bobrossrtx"
    .category "Test Framework"
    .tag "skip"
    .skip
    
    # This code won't execute
    LOAD_IMM EAX, 999
    HALT
}

.test "benchmark_example" {
    .description "Benchmark example (infrastructure ready, timing not yet implemented)"
    .author "bobrossrtx"
    .category "Benchmarks"
    .tag "benchmark"
    .tag "arithmetic"
    .benchmark
    .warmup 5
    .iterations 100
    .measure "time"
    
    # Simple arithmetic loop
    LOAD_IMM EAX, 0
    LOAD_IMM EBX, 100
    
bench_loop:
    CMP EAX, EBX
    JZ bench_end
    INC EAX
    JMP bench_loop
    
bench_end:
    HALT
    
    .assert_reg EAX, 100
}

.test "combined_metadata" {
    .description "Test with all metadata directives"
    .author "bobrossrtx"
    .category "Test Framework"
    .tag "comprehensive"
    .tag "metadata"
    .maxsteps 500
    .maxcalldepth 16
    
    LOAD_IMM EAX, 123
    LOAD_IMM EBX, 200  # Fixed: within 0-255 range
    ADD EAX, EBX
    HALT
    
    .assert_reg EAX, 67  # 123 + 200 = 323, but overflows in 8-bit to 67 (323 % 256)
}
