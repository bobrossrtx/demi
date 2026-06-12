; Programming from the Ground Up — Chapter 3
; maximum.s: Find the maximum value in an array

.section .data
data_items:
    .dd 3, 67, 34, 222, 45, 75, 54, 34, 44, 33, 22, 11, 66, 0

.section .text
.global _start
_start:
    MOV ESI, data_items       ; address of first element
    MOV EBX, [ESI]            ; max = first item
    ADD ESI, 4                ; advance to next

loop_start:
    MOV EAX, [ESI]            ; load current item
    CMP EAX, 0                ; check for terminator
    JE loop_exit

    CMP EAX, EBX              ; compare with max
    JLE skip_update
    MOV EBX, EAX              ; update max

skip_update:
    ADD ESI, 4                ; next element (4 bytes per dword)
    JMP loop_start

loop_exit:
    MOV EAX, 1                ; sys_exit
    INT 0x80                  ; exit(max in EBX)
