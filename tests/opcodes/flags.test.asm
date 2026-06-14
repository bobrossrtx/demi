; Flag Test Suite — x86-specific instructions skipped
; CLC, STC, CMC, ADC, CBW, CWD are real x86 ops not available in Demi VM

.test "flag_ops_basic" {
    .description "Tests basic flag manipulation (x86-only, skipped)"
    .category "Flags"
    .tag "flags"
    .skip "x86-specific instructions (CLC/ADC/STC/CMC) not in Demi VM"
}

.test "cbw_cwd_sign_extend" {
    .description "Tests CBW and CWD sign extension (x86-only, skipped)"
    .category "Flags"
    .tag "flags"
    .skip "x86-specific instructions (CBW/CWD) not in Demi VM"
}
