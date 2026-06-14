; Programming from the Ground Up — Chapter 6
; read-records.s: Read structured records, print name + age
;
; Expected Output:
;   Reads "records.dat" (created by write-records) and prints each record:
;     Fredrick - age 34
;     Marilyn - age 29
;     Derrick - age 52

.section .bss
buffer:     .resb 84

.section .data
filename:
    .string "records.dat"
age_label:
    .string " - age "
newline:
    .string 10

.section .text
.global _start
_start:
    ; sys_open(filename, O_RDONLY, 0)
    MOV EAX, 5
    MOV EBX, filename
    MOV ECX, 0
    MOV EDX, 0
    INT 0x80
    MOV ESI, EAX

read_loop:
    ; sys_read(fd, buffer, 84)
    MOV EAX, 3
    MOV EBX, ESI
    MOV ECX, buffer
    MOV EDX, 84
    INT 0x80
    CMP EAX, 84
    JNE done

    ; Find name length (scan for null, max 40)
    MOV EDI, 0
name_find:
    CMP EDI, 40
    JGE name_print
    MOV EAX, [buffer+EDI]
    AND EAX, 255
    CMP EAX, 0
    JE name_print
    INC EDI
    CMP EDI, 40
    JL name_find

name_print:
    MOV EAX, 4
    MOV EBX, 1
    MOV ECX, buffer
    MOV EDX, EDI
    INT 0x80

    ; Print " - age "
    MOV EAX, 4
    MOV EBX, 1
    MOV ECX, age_label
    MOV EDX, 7
    INT 0x80

    ; Print age (dword at buffer+80)
    MOV EAX, [buffer+80]
    MOV ECX, 10
    MOV EDI, 0

age_to_ascii:
    MOV EDX, 0
    DIV ECX
    PUSH EDX
    INC EDI
    CMP EAX, 0
    JNE age_to_ascii

age_print:
    CMP EDI, 0
    JE age_done
    POP EAX
    ADD EAX, 48
    MOV [buffer], AL
    PUSH EDI
    MOV EAX, 4
    MOV EBX, 1
    MOV ECX, buffer
    MOV EDX, 1
    INT 0x80
    POP EDI
    DEC EDI
    JMP age_print

age_done:
    MOV EAX, 4
    MOV EBX, 1
    MOV ECX, newline
    MOV EDX, 1
    INT 0x80
    JMP read_loop

done:
    MOV EAX, 6
    MOV EBX, ESI
    INT 0x80
    MOV EAX, 1
    MOV EBX, 0
    INT 0x80
