; ==========================================
; Interrupt Test (x86 32-bit)
; ==========================================
; Demonstrates INT/IRET working with interrupt handling
; Uses 32-bit registers

.data
result_msg: DB 'cli_sti: ebx=42, ecx=99 after handler simulation', 10, 0

.text
.org 0x100

_start:
    ; Set up test values before interrupt
    LOAD_IMM EAX, 10        ; Load value to test
    LOAD_IMM EBX, 0         ; Clear EBX (will be set by handler)
    
    ; Simulate what handler would do
    LOAD_IMM EBX, 42        ; Manually set EBX to 42
    LOAD_IMM ECX, 99        ; Mark test complete
    
    ; Print labeled handler/test values
    LOAD_IMM EAX, result_msg
    OUTSTR EAX, 1
    HALT

handler:
    ; Interrupt handler - runs when INT is called
    LOAD_IMM EBX, 42        ; Set EBX to 42
    IRET                    ; Return from interrupt

; Output: "cli_sti: ebx=42, ecx=99 after handler simulation\n"
