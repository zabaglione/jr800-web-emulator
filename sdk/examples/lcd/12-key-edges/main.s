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
.extern keys_reset
.extern keys_keypad
.extern keys_ack
.global key_state
.global saved_edges
.global presses
.section .text, code
entry:
    SEI
    LDS #$5FFF
    JSR basic_save
    JSR init
    JSR clear
    LDX #key_state
    JSR keys_reset
    CLR presses
    CLR saved_edges
    CLR saved_edges + 1
    CLR saved_edges + 2
    LDX #title
    LDD #framebuffer + 192 + 36
    JSR text
    LDX #labels
    LDD #framebuffer + 576 + 6
    JSR text
render:
    LDAA saved_edges
    LDX #framebuffer + 576 + 36
    JSR bit
    LDAA saved_edges + 1
    LDX #framebuffer + 576 + 90
    JSR bit
    LDAA saved_edges + 2
    LDX #framebuffer + 576 + 156
    JSR bit
    LDAA presses
    ADDA #48
    LDX #framebuffer + 960 + 90
    JSR glyph
    JSR present
frame_ready:
    JSR basic_check_break
    JSR delay
    LDX #key_state
    JSR keys_keypad
    LDAA key_state
    STAA saved_edges
    LDAA key_state + 1
    STAA saved_edges + 1
    BITA #$20
    BEQ release
    INC presses
    LDAA presses
    CMPA #10
    BNE release
    CLR presses
release:
    LDAA key_state + 2
    STAA saved_edges + 2
    LDX #key_state
    JSR keys_ack
    BRA render
bit:
    ANDA #$20
    BEQ zero
    LDAA #49
    JMP glyph
zero:
    LDAA #48
    JMP glyph
.section .bss, bss
key_state: .space 3
saved_edges: .space 3
presses: .space 1
.section .data, data
title: .byte 75,69,89,32,53,32,69,68,71,69,83,0
labels: .byte 72,69,76,68,32,32,32,32,32,68,79,87,78,32,32,32,32,32,32,85,80,0
