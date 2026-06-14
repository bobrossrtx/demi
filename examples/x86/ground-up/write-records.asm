; Programming from the Ground Up — Chapter 6
; write-records.s: Write structured records to file
;
; Expected Output:
;   Creates "records.dat" with 3 structured records (name + address + age).
;   No stdout output; run read-records to verify.
;   ls -l records.dat  →  252 bytes

.section .data
filename:
    .string "records.dat"

record1_name:
    .string "Fredrick"
    .zero 32
record1_addr:
    .string "123 Main St, Springfield, IL 62701"
    .zero 5
record1_age:
    .dd 34

record2_name:
    .string "Marilyn"
    .zero 32
record2_addr:
    .string "456 Oak Ave, Chicago, IL 60601     "
    .zero 5
record2_age:
    .dd 29

record3_name:
    .string "Derrick"
    .zero 33
record3_addr:
    .string "789 Pine Rd, Peoria, IL 61602      "
    .zero 5
record3_age:
    .dd 52

.section .text
.global _start
_start:
    ; sys_open(filename, O_CREAT|O_WRONLY|O_TRUNC, 0644)
    MOV EAX, 5
    MOV EBX, filename
    MOV ECX, 577       ; O_CREAT(64) | O_WRONLY(1) | O_TRUNC(512)
    MOV EDX, 420       ; 0644 octal
    INT 0x80
    MOV ESI, EAX

    ; Write record 1
    MOV EAX, 4
    MOV EBX, ESI
    MOV ECX, record1_name
    MOV EDX, 84
    INT 0x80

    ; Write record 2
    MOV EAX, 4
    MOV EBX, ESI
    MOV ECX, record2_name
    MOV EDX, 84
    INT 0x80

    ; Write record 3
    MOV EAX, 4
    MOV EBX, ESI
    MOV ECX, record3_name
    MOV EDX, 84
    INT 0x80

    ; Close
    MOV EAX, 6
    MOV EBX, ESI
    INT 0x80

    ; Exit
    MOV EAX, 1
    MOV EBX, 0
    INT 0x80
