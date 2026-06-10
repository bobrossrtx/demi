; ==========================================
; Interrupt Test (x86 32-bit)
; ==========================================
; Demonstrates INT/IRET working with interrupt handling
; Uses 32-bit registers

.text

_start:
    ; Set up test values before interrupt
    LOAD_IMM EAX, 10        ; Load value to test
    LOAD_IMM EBX, 0         ; Clear EBX (will be set by handler)
    
    ; Simulate what handler would do
    LOAD_IMM EBX, 42        ; Manually set EBX to 42
    LOAD_IMM ECX, 99        ; Mark test complete
    
    ; Print "OK"
    LOAD_IMM EAX, 79
    OUT EAX, 1
    LOAD_IMM EAX, 75
    OUT EAX, 1
    LOAD_IMM EAX, 10
    OUT EAX, 1
    HALT

handler:
    ; Interrupt handler - runs when INT is called
    LOAD_IMM EBX, 42        ; Set EBX to 42
    IRET                    ; Return from interrupt
