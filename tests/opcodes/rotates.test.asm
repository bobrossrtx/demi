; Rotate Instructions Test Suite — x86-specific, skipped
; ROL, ROR, RCL, RCR are real x86 ops not available in Demi VM

.test "rol_imm8" {
    .description "Tests ROL (x86-only, skipped)"
    .category "Instructions"
    .tag "rotate"
    .skip "x86-specific instruction (ROL) not in Demi VM"
}

.test "ror_imm8" {
    .description "Tests ROR (x86-only, skipped)"
    .category "Instructions"
    .tag "rotate"
    .skip "x86-specific instruction (ROR) not in Demi VM"
}

.test "rcl_through_carry" {
    .description "Tests RCL (x86-only, skipped)"
    .category "Instructions"
    .tag "rotate"
    .skip "x86-specific instruction (RCL) not in Demi VM"
}

.test "rcr_through_carry" {
    .description "Tests RCR (x86-only, skipped)"
    .category "Instructions"
    .tag "rotate"
    .skip "x86-specific instruction (RCR) not in Demi VM"
}
