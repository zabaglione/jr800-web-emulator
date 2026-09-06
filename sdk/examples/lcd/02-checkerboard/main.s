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
    LDX #framebuffer
    LDAB #48
band:
    LDAA #$0F
column:
    STAA 0,X
    INX
    STAA 0,X
    INX
    STAA 0,X
    INX
    STAA 0,X
    INX
    EORA #$FF
    DECB
    BNE column
    CPX #framebuffer + 1536
    BEQ filled
    LDAB #48
    BRA band
filled:
    JSR present
frame_ready:
    JSR basic_check_break
    BRA frame_ready
