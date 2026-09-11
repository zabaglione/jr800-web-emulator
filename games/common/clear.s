; SPDX-License-Identifier: MIT
; Retain the completed playfield. This never advances gameplay or redraws
; unchanged pixels, and reuses the short title cue's two working bytes.
.global clear_active
.global note_index
.section .text, code
clear_begin:
    LDAA #1
    STAA clear_active
    CLR note_index
    LDAA #6
    STAA note_blocks
    LDAA input_ticks
    STAA clear_clock
    JMP input_gate
clear_update:
    LDAA input_ticks
    SUBA clear_clock
    CMPA note_blocks
    BCS clear_idle
    LDAA input_ticks
    STAA clear_clock
    INC note_index
    LDAA #8
    STAA note_blocks
    LDAB note_index
    CMPB #7
    BCS clear_idle
    LDAA #20
    STAA note_blocks
    CMPB #9
    BCS clear_idle
    CLR clear_active
    LDX #result_win
    JMP result_draw
clear_idle:
    RTS
; Two periods per slice; the caller polls keys before another slice.
; The final twenty ticks are silent, so the cadence has room to settle.
clear_audio:
    TST clear_active
    BEQ clear_idle
    LDAB note_index
    BEQ clear_idle
    CMPB #8
    BCC clear_idle
    DECB
    ASLB
    LDX #clear_notes
    ABX
    LDX 0,X
    LDD #2
    JMP sound_tone
.section .bss, bss
clear_active: .space 1
clear_clock: .space 1
