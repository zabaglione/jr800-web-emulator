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
    CLRA
    STAA position
    STAA direction
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
    LDAA direction
    BNE move_left
    LDAA position
    CMPA #184
    BNE move_right
    LDAA #1
    STAA direction
move_left:
    LDAA position
    BEQ turn_right
    SUBA #4
    STAA position
    BRA render
turn_right:
    CLRA
    STAA direction
move_right:
    LDAA position
    ADDA #4
    STAA position
    BRA render
.section .bss, bss
.global position
.global direction
position:
    .space 1
direction:
    .space 1
.section .data, data
title:
    .byte 66, 79, 85, 78, 67, 69, 32, 83, 80, 82, 73, 84, 69, 0
caption:
    .byte 80, 79, 83, 73, 84, 73, 79, 78, 32, 65, 78, 68, 32, 84, 85, 82, 78, 0
