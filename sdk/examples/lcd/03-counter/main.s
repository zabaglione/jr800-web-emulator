; SPDX-License-Identifier: MIT
.global entry
.global frame_ready
.extern init
.extern clear
.extern present
.extern text
.extern glyph
.extern delay
.extern framebuffer
.section .text, code
entry:
    SEI
    LDS #$5FFF
    JSR init
    JSR clear
    CLRA
    STAA ones
    STAA tens
    LDX #title
    LDD #framebuffer + 192 + 63
    JSR text
    LDX #caption
    LDD #framebuffer + 960 + 54
    JSR text
render:
    LDAA tens
    ADDA #48
    LDX #framebuffer + 576 + 90
    JSR glyph
    LDAA ones
    ADDA #48
    LDX #framebuffer + 576 + 96
    JSR glyph
    JSR present
frame_ready:
    JSR delay
    INC ones
    LDAA ones
    CMPA #10
    BNE render
    CLRA
    STAA ones
    INC tens
    LDAA tens
    CMPA #10
    BNE render
    CLRA
    STAA tens
    BRA render
.section .bss, bss
.global ones
.global tens
ones:
    .space 1
tens:
    .space 1
.section .data, data
title:
    .byte 68, 69, 67, 73, 77, 65, 76, 32, 48, 48, 45, 57, 57, 0
caption:
    .byte 67, 65, 82, 82, 89, 32, 65, 78, 68, 32, 76, 79, 79, 80, 0
