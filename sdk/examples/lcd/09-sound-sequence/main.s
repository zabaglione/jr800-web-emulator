; SPDX-License-Identifier: MIT
.global entry
.global frame_ready
.extern basic_save
.extern basic_check_break
.extern init
.extern clear
.extern present
.extern text
.extern glyph
.extern delay
.extern framebuffer
.extern sound_play
.extern sound_port
.global phrase
.section .text, code
entry:
    SEI
    LDS #$5FFF
    JSR basic_save
    JSR init
    JSR clear
    CLR phrase
    LDX #title
    LDD #framebuffer + 192 + 36
    JSR text
render:
    LDX #framebuffer + 576
    LDAB #192
    CLRA
erase:
    STAA 0,X
    INX
    DECB
    BNE erase
    LDX #se_label
    TST phrase
    BEQ label
    LDX #melody_label
label:
    LDD #framebuffer + 576 + 48
    JSR text
    JSR present
frame_ready:
    JSR basic_check_break
    ; Standalone demo baseline, as in library sample 08; not a saved latch.
    LDAA #$EF
    STAA sound_port
    LDAA #$10
    STAA $0000
    LDX #effect
    TST phrase
    BEQ play
    LDX #melody
play:
    JSR sound_play
    LDAB #8
pause:
    PSHB
    JSR basic_check_break
    JSR delay
    PULB
    DECB
    BNE pause
    LDAA phrase
    EORA #1
    STAA phrase
    BRA render
.section .bss, bss
phrase: .space 1
.section .data, data
title: .byte 83,79,85,78,68,32,83,69,81,85,69,78,67,69,0
se_label: .byte 83,72,79,82,84,32,69,70,70,69,67,84,0
melody_label: .byte 83,73,78,71,76,69,32,77,69,76,79,68,89,0
effect: .word 344,24,172,32,0,0
melody: .word 344,80,0,8000,306,90,273,100,229,120,0,0
