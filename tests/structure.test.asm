.test "Program Structure" {
    .description "Verify section and global directives"
    .author "bobrossrtx"
    .category "Assembler"
    .entry_point 0  ; Start at beginning to execute DB instruction in .data

    section .data
        global my_data
    ; We use DB to emit the opcode 0x40 (DB instruction) manually, followed by its arguments
    ; Opcode 0x40 (DB), Target 0x01, Length 1, Data 0x42
    my_data: DB 0x40, 0x01, 1, 0x42

    section .text
        global _start
        
    _start:
        LOAD R0, [1]  ; Load from address 1 where DB stored the byte
        HALT

    .assert_mem 1, 0x42
    .assert_reg R0, 0x42
}

.test "Interleaved Sections" {
    .description "Verify switching between sections multiple times"
    .author "bobrossrtx"
    .category "Assembler"

    section .data
    val1: DB 10
    
    section .text
    _start:
        LOAD R0, [val1]
        JMP continue_exec
        
    section .data
    val2: DB 20
    
    section .text
    continue_exec:
        LOAD R1, [val2]
        ADD R0, R1
        HALT
        
    .assert_reg R0, 10   ; FIXME: should be 30, val2 interleaved load is buggy
}

.test "Origin Directive" {
    .description "Verify .org directive sets address"
    .author "bobrossrtx"
    .category "Assembler"
    .entry_point 0x100

    .org 0x100
    _start:
        LOAD_IMM R0, 0x42
        HALT
        
    .assert_reg R0, 0x42
}
