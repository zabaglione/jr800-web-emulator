; SPDX-License-Identifier: MIT
; JR-HuBASIC 1.0 context for applications that finish at its command prompt.
; Call basic_save after selecting the application stack, before using $80-$FF.
; BREAK abandons the application's call stack; it never controls power.
.global basic_save
.global basic_check_break
.global basic_exit
.global basic_context

.section .runtime, code
basic_save:
    LDX #$0080
    STX basic_source
    LDX #basic_context
    STX basic_destination
    JMP basic_copy

; A, B and X are unchanged while BREAK is released. Condition codes change.
basic_check_break:
    TST $0F7F
    BMI basic_break_released
    JMP basic_exit
basic_break_released:
    RTS

basic_exit:
    SEI
    LDS #$5FFF
    LDX #basic_context
    STX basic_source
    LDX #$0080
    STX basic_destination
    JSR basic_copy
    ; Restore BASIC display/input and rebuild its program/variable allocation.
    ; Machine-code loading has overwritten the old BASIC page terminators.
    JSR $FEEC
    JSR $F127
    JMP $805E

basic_copy:
    LDAB #128
basic_copy_byte:
    LDX basic_source
    LDAA 0,X
    INX
    STX basic_source
    LDX basic_destination
    STAA 0,X
    INX
    STX basic_destination
    DECB
    BNE basic_copy_byte
    RTS

.section .context, bss
basic_context:
    .space 128
basic_source:
    .space 2
basic_destination:
    .space 2
