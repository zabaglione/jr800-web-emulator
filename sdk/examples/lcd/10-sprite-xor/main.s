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
.extern sprite8
.extern sprite_width
.extern sprite_mode
.global position
.global vertical
.global direction
.section .text, code
entry:
    SEI
    LDS #$5FFF
    JSR basic_save
    JSR init
    JSR clear
    LDX #title
    LDD #framebuffer + 192 + 42
    JSR text
    LDX #framebuffer + 384
    CLRB
background:
    TBA
    ANDA #7
    BNE background_blank
    LDAA #$11
    BRA background_store
background_blank:
    CLRA
background_store:
    STAA 0,X
    INCB
    INX
    CPX #framebuffer + 1344
    BNE background
    LDAA #8
    STAA sprite_width
    LDAA #1
    STAA sprite_mode
    CLR position
    CLR direction
    LDAA #20
    STAA vertical
    JSR draw
    JSR present
frame_ready:
    JSR basic_check_break
    JSR delay
    ; XOR the old image away without storing a background copy.
    JSR draw
    TST direction
    BNE left
    INC position
    LDAA position
    CMPA #184
    BNE height
    INC direction
    BRA height
left:
    DEC position
    BNE height
    CLR direction
height:
    LDAA position
    ANDA #15
    ADDA #20
    STAA vertical
    JSR draw
    JSR present
    BRA frame_ready
draw:
    LDX #image
    LDAA position
    LDAB vertical
    JMP sprite8
.section .bss, bss
position: .space 1
vertical: .space 1
direction: .space 1
.section .data, data
title: .byte 88,79,82,32,83,80,82,73,84,69,0
image: .byte $3C,$7E,$DB,$FF,$FF,$DB,$7E,$3C
