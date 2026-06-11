; Rotate Instructions Test Suite
; Tests: ROL, ROR, RCL, RCR with immediate and CL register

.test "rol_imm8" {
    .description "Tests ROL with immediate shift count"
    .category "Level 2"
    .tag "rotate"

    LOAD_IMM EAX, 0x80000001   ; bit31=1, bit0=1
    ROL EAX, 1                 ; rotate left → 0x00000003
    .assert_reg EAX, 0x00000003
}

.test "ror_imm8" {
    .description "Tests ROR with immediate shift count"
    .category "Level 2"
    .tag "rotate"

    LOAD_IMM EAX, 0x80000003   ; bit31=1, bit1=1, bit0=1
    ROR EAX, 1                 ; rotate right → 0xC0000001
    .assert_reg EAX, 0xC0000001
}

.test "rcl_through_carry" {
    .description "Tests RCL with carry flag propagation"
    .category "Level 2"
    .tag "rotate"

    LOAD_IMM EAX, 0x80000000   ; bit31=1
    STC                        ; set carry
    RCL EAX, 1                 ; rotate left through carry → 0x00000001, carry set
    .assert_reg EAX, 0x00000001
}

.test "rcr_through_carry" {
    .description "Tests RCR with carry flag propagation"
    .category "Level 2"
    .tag "rotate"

    LOAD_IMM EAX, 0x00000001   ; bit0=1
    CLC                        ; clear carry
    RCR EAX, 1                 ; rotate right through carry → carry set, EAX=0
    .assert_reg EAX, 0x00000000
}
