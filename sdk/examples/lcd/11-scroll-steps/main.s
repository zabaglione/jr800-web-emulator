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
.extern scroll_left
.extern scroll_right
.extern keys_reset
.extern keys_keypad
.extern keys_ack
.global key_state
.global amount
.section .text, code
entry:
    SEI
    LDS #$5FFF
    JSR basic_save
    JSR init
    JSR clear
    LDX #key_state
    JSR keys_reset
    LDAA #1
    STAA amount
    LDX #title
    LDD #framebuffer + 192 + 24
    JSR text
    LDX #caption
    LDD #framebuffer + 1152 + 12
    JSR text
    LDX #framebuffer + 576
    CLRB
pattern:
    TBA
    ANDA #8
    BEQ blank
    LDAA #$FF
blank:
    STAA 0,X
    INX
    INCB
    CMPB #192
    BNE pattern
render:
    LDAA amount
    ADDA #48
    LDX #framebuffer + 960 + 90
    JSR glyph
    JSR present
frame_ready:
    JSR basic_check_break
    JSR delay
    LDX #key_state
    JSR keys_keypad
    LDAA key_state + 1
    BITA #$20
    BEQ move
    LDAA amount
    EORA #9
    STAA amount
move:
    LDAA key_state
    BITA #$10
    BNE left
    BITA #$40
    BEQ done
    LDX #framebuffer + 576
    LDAA amount
    JSR scroll_right
    BRA done
left:
    LDX #framebuffer + 576
    LDAA amount
    JSR scroll_left
done:
    LDX #key_state
    JSR keys_ack
    BRA render
.section .bss, bss
key_state: .space 3
amount: .space 1
.section .data, data
title: .byte 83,67,82,79,76,76,32,49,32,79,82,32,56,0
caption: .byte 52,32,76,69,70,84,32,53,32,83,84,69,80,32,54,32,82,73,71,72,84,0
