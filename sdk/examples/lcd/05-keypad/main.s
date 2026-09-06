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
    LDAA #92
    STAA position
    LDX #title
    LDD #framebuffer + 192 + 60
    JSR text
    LDX #caption
    LDD #framebuffer + 1152 + 48
    JSR text
render:
    LDX #framebuffer + 768
    CLRA
    LDAB #192
erase_column:
    STAA 0,X
    INX
    DECB
    BNE erase_column
    LDX #framebuffer + 768
    LDAB position
    ABX
    LDAA #$3C
    STAA 0,X
    STAA 7,X
    LDAA #$7E
    STAA 1,X
    STAA 6,X
    LDAA #$DB
    STAA 2,X
    STAA 5,X
    LDAA #$FF
    STAA 3,X
    STAA 4,X
    JSR present
frame_ready:
    JSR basic_check_break
    JSR delay
    ; E-381 owner-observed keypad selection: 4=$EF, 6=$BF, idle=$FF.
    LDAA $0FFE
    CMPA #$EF
    BEQ move_left
    CMPA #$BF
    BNE render
    LDAA position
    CMPA #184
    BEQ render
    ADDA #4
    STAA position
    BRA render
move_left:
    LDAA position
    BEQ render
    SUBA #4
    STAA position
    BRA render
.section .bss, bss
.global position
position:
    .space 1
.section .data, data
title:
    .byte 75, 69, 89, 80, 65, 68, 32, 67, 85, 82, 83, 79, 82, 0
caption:
    .byte 52, 32, 76, 69, 70, 84, 32, 32, 32, 54, 32, 82, 73, 71, 72, 84, 0
