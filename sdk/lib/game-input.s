; SPDX-License-Identifier: MIT
; E-392 keyboard rows. Logical U/D/L/R/SPACE/RETURN = bits 0..5.
; Read only required rows. Aliases are merged before edge detection.
; Free-running HD6301 timer counts E cycles; no interrupt or host clock.
.global input_init
.global input_poll
.global input_take
.global input_gate
.global input_held
.global input_pressed
.global input_event
.global input_ticks
.extern basic_check_break
.section .text, code
input_init:
    CLR input_held
    CLR input_pressed
    CLR input_event
    CLR input_ticks
    CLR input_clock
    CLR input_clock + 1
    LDD $0009
    STD input_last
    LDAA #1
    STAA input_blocked
    RTS
input_gate:
    CLR input_pressed
    CLR input_event
    LDAA #1
    STAA input_blocked
    RTS
input_poll:
    JSR basic_check_break
input_read_clock:
    LDD $0009
    STD input_now
    LDAA $0009
    CMPA input_now
    BNE input_read_clock
    LDD input_now
    SUBD input_last
    ADDD input_clock
    STD input_clock
    LDD input_now
    STD input_last
    LDD input_clock
    SUBD #20000
    BCS input_scan
    STD input_clock
    INC input_ticks
input_scan:
    CLR input_raw
    LDAA $0FFD
    BITA #1
    BNE input_letters
    INC input_raw
input_letters:
    LDAA $0FBF
    BITA #$80
    BNE input_s
    LDAB input_raw
    ORAB #1
    STAB input_raw
input_s:
    BITA #8
    BNE input_ad
    LDAB input_raw
    ORAB #2
    STAB input_raw
input_ad:
    LDAA $0FEF
    BITA #2
    BNE input_d
    LDAB input_raw
    ORAB #4
    STAB input_raw
input_d:
    BITA #16
    BNE input_space
    LDAB input_raw
    ORAB #8
    STAB input_raw
input_space:
    BITA #1
    BNE input_pad
    LDAB input_raw
    ORAB #16
    STAB input_raw
input_pad:
    LDAA $0FFE
    BITA #4
    BNE input_left
    LDAB input_raw
    ORAB #2
    STAB input_raw
input_left:
    BITA #16
    BNE input_right
    LDAB input_raw
    ORAB #4
    STAB input_raw
input_right:
    BITA #64
    BNE input_return
    LDAB input_raw
    ORAB #8
    STAB input_raw
input_return:
    LDAA $0F7F
    BITA #64
    BNE input_merge
    LDAB input_raw
    ORAB #32
    STAB input_raw
input_merge:
    ; Opposite directions neutralize each other.
    LDAA input_raw
    ANDA #3
    CMPA #3
    BNE input_horizontal
    LDAA input_raw
    ANDA #$FC
    STAA input_raw
input_horizontal:
    LDAA input_raw
    ANDA #12
    CMPA #12
    BNE input_edges
    LDAA input_raw
    ANDA #$F3
    STAA input_raw
input_edges:
    LDAA input_held
    COMA
    ANDA input_raw
    ORAA input_pressed
    STAA input_pressed
    LDAA input_raw
    STAA input_held
    TST input_blocked
    BEQ input_poll_done
    CLR input_pressed
    TSTA
    BNE input_poll_done
    CLR input_blocked
input_poll_done:
    RTS
input_take:
    LDAA input_pressed
    STAA input_event
    CLR input_pressed
    ANDA #15
    BEQ input_repeat
    LDAA input_ticks
    ADDA #20
    STAA input_repeat_at
    RTS
input_repeat:
    TST input_blocked
    BNE input_take_done
    LDAA input_held
    ANDA #15
    BEQ input_take_done
    LDAB input_ticks
    SUBB input_repeat_at
    BMI input_take_done
    ORAA input_event
    STAA input_event
    LDAA input_ticks
    ADDA #6
    STAA input_repeat_at
input_take_done:
    RTS
.section .bss, bss
input_held: .space 1
input_pressed: .space 1
input_event: .space 1
input_ticks: .space 1
input_last: .space 2
input_now: .space 2
input_clock: .space 2
input_raw: .space 1
input_blocked: .space 1
input_repeat_at: .space 1
