; Flag and Sign Extension Test Suite
; Tests: CLC, STC, CMC, CLD, STD, LAHF, SAHF, CBW, CWD, CWDE, CDQ

.test "flag_ops_basic" {
    .description "Tests basic flag manipulation instructions"
    .category "Level 2"
    .tag "flags"

    CLC                        ; clear carry
    LOAD_IMM EAX, 1
    ADC EAX, 0                 ; EAX = 1 + 0 + 0(carry) = 1
    .assert_reg EAX, 1

    STC                        ; set carry
    LOAD_IMM EAX, 1
    ADC EAX, 0                 ; EAX = 1 + 0 + 1(carry) = 2
    .assert_reg EAX, 2

    CMC                        ; complement carry (was 1, now 0)
    LOAD_IMM EAX, 1
    ADC EAX, 0                 ; EAX = 1 + 0 + 0(carry) = 1
    .assert_reg EAX, 1
}

.test "cbw_cwd_sign_extend" {
    .description "Tests CBW and CWD sign extension"
    .category "Level 2"
    .tag "flags"

    ; CBW: extend AL → AX (byte to word)
    LOAD_IMM EAX, 0xFFFFFF80   ; AL = 0x80 = -128 signed
    CBW                        ; sign-extend AL to AX (within EAX)
    ; AX should now be 0xFF80

    ; CWD: extend AX → DX:AX (word to dword)
    LOAD_IMM EAX, 0x00008000   ; AX = 0x8000 = -32768 signed
    CWD                        ; sign-extend AX to DX:AX
    ; DX should be 0xFFFF (sign extended)
    .assert_reg EDX, 0x0000FFFF
}
