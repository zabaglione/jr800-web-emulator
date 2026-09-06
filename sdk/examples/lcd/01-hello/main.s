; SPDX-License-Identifier: MIT
.global entry
.global frame_ready
.extern init
.extern basic_save
.extern basic_check_break
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
    JSR basic_save
    JSR init
    JSR clear
    LDX #title
    LDD #framebuffer + 192 + 60
    JSR text
    LDX #greeting
    LDD #framebuffer + 576 + 60
    JSR text
    LDX #size
    LDD #framebuffer + 960 + 57
    JSR text
    JSR present
frame_ready:
    JSR basic_check_break
    BRA frame_ready
.section .data, data
title:
    .byte 72, 69, 76, 76, 79, 32, 74, 82, 45, 56, 48, 48, 0
greeting:
    .byte 77, 65, 67, 72, 73, 78, 69, 32, 67, 79, 68, 69, 0
size:
    .byte 49, 57, 50, 32, 88, 32, 54, 52, 32, 68, 79, 84, 83, 0
