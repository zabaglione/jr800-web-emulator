; SPDX-License-Identifier: MIT
.global scroll_left
.global scroll_right
.section .text, code
; X = start of a writable 192-byte band, A = 1..191 pixels.
; Vacated bytes become zero. Invalid amount is a no-op. A/B/X/CC clobbered.
scroll_left:
    TSTA
    BEQ scroll_return
    CMPA #192
    BCC scroll_return
    STAA scroll_amount
    STX scroll_dest
    TAB
    ABX
    STX scroll_source
    LDAA #192
    SUBA scroll_amount
    STAA scroll_count
scroll_left_copy:
    LDX scroll_source
    LDAA 0,X
    INX
    STX scroll_source
    LDX scroll_dest
    STAA 0,X
    INX
    STX scroll_dest
    DEC scroll_count
    BNE scroll_left_copy
    LDAB scroll_amount
    CLRA
scroll_left_clear:
    STAA 0,X
    INX
    DECB
    BNE scroll_left_clear
scroll_return:
    RTS
scroll_right:
    TSTA
    BEQ scroll_return
    CMPA #192
    BCC scroll_return
    STAA scroll_amount
    LDAB #191
    ABX
    STX scroll_dest
    LDAB scroll_amount
scroll_right_seek:
    DEX
    DECB
    BNE scroll_right_seek
    STX scroll_source
    LDAA #192
    SUBA scroll_amount
    STAA scroll_count
scroll_right_copy:
    LDX scroll_source
    LDAA 0,X
    DEX
    STX scroll_source
    LDX scroll_dest
    STAA 0,X
    DEX
    STX scroll_dest
    DEC scroll_count
    BNE scroll_right_copy
    LDAB scroll_amount
    CLRA
scroll_right_clear:
    STAA 0,X
    DEX
    DECB
    BNE scroll_right_clear
    RTS
.section .bss, bss
scroll_amount: .space 1
scroll_count: .space 1
scroll_source: .space 2
scroll_dest: .space 2
