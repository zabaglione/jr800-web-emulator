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
    BRA frame_ready
