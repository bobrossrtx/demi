; ==========================================
; Number Guess Game (x64 64-bit)
; ==========================================
; A small interactive game using sys_read/sys_write.
; The player has three tries to guess the hidden digit.

.data
    intro: DB "Guess the hidden digit (0-9). You get 3 tries.", 10
    intro_len EQU 46

    prompt: DB "Enter your guess: "
    prompt_len EQU 18

    too_low: DB "Too low!", 10
    too_low_len EQU 9

    too_high: DB "Too high!", 10
    too_high_len EQU 10

    invalid: DB "Please enter a single digit from 0 to 9.", 10
    invalid_len EQU 41

    win_msg: DB "Correct! You win.", 10
    win_msg_len EQU 18

    lose_msg: DB "Out of tries. The answer was 7.", 10
    lose_msg_len EQU 33

    buffer: RESB 16

.text
_start:
    ; Show the game rules.
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, intro
    LOAD_IMM RDX, intro_len
    INT 0x80

    ; The hidden answer is '7'. Give the player three attempts.
    LOAD_IMM R8, 3

game_loop:
    ; Print the input prompt.
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, prompt
    LOAD_IMM RDX, prompt_len
    INT 0x80

    ; Read up to 16 bytes from stdin.
    LOAD_IMM RAX, 3
    LOAD_IMM RBX, 0
    LOAD_IMM RCX, buffer
    LOAD_IMM RDX, 16
    INT 0x80

    ; Stop cleanly if stdin closes.
    CMP RAX, 0
    JLE exit_program

    ; Inspect the first typed character.
    LOAD_IMM RSI, buffer
    LOADR R11, RSI

    ; Reject anything outside '0'..'9' without consuming a try.
    CMP R11, 48            ; '0'
    JL invalid_input
    CMP R11, 57            ; '9'
    JG invalid_input

    ; Compare against the hidden answer.
    CMP R11, 55            ; '7'
    JE player_wins
    JL guess_too_low
    JMP guess_too_high

invalid_input:
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, invalid
    LOAD_IMM RDX, invalid_len
    INT 0x80
    JMP game_loop

guess_too_low:
    DEC R8
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, too_low
    LOAD_IMM RDX, too_low_len
    INT 0x80
    CMP R8, 0
    JG game_loop
    JMP player_loses

guess_too_high:
    DEC R8
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, too_high
    LOAD_IMM RDX, too_high_len
    INT 0x80
    CMP R8, 0
    JG game_loop
    JMP player_loses

player_wins:
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, win_msg
    LOAD_IMM RDX, win_msg_len
    INT 0x80
    JMP exit_program

player_loses:
    LOAD_IMM RAX, 4
    LOAD_IMM RBX, 1
    LOAD_IMM RCX, lose_msg
    LOAD_IMM RDX, lose_msg_len
    INT 0x80

exit_program:
    LOAD_IMM RAX, 1
    LOAD_IMM RBX, 0
    INT 0x80
    HALT