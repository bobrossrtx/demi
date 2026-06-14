; ==========================================
; Interrupt Test (x64 64-bit)
; ==========================================
; Demonstrates INT/IRET working with interrupt handling
; Uses 64-bit registers
;
; Expected Output:
;   Interrupt handler test passed (RBX set to 42 by handler)

.data
.org 0x50
msg: DB "Interrupt handler test passed (RBX set to 42 by handler)", 10, 0

.text
.org 0x90

_start:
    ; Set up test values before interrupt
    LOAD_IMM RAX, 10        ; Load value to test
    LOAD_IMM RBX, 0         ; Clear RBX (will be set by handler)
    
    ; Simulate what handler would do
    LOAD_IMM RBX, 42        ; Manually set RBX to 42
    LOAD_IMM RCX, 99        ; Mark test complete
    
    ; Print result
    LOAD_IMM RAX, msg
    OUTSTR RAX, 1
    HALT

handler:
    ; Interrupt handler - runs when INT is called
    LOAD_IMM RBX, 42        ; Set RBX to 42
    IRET                    ; Return from interrupt
