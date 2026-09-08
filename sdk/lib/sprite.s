; SPDX-License-Identifier: MIT
.global sprite8
.global sprite_width
.global sprite_mode
.extern framebuffer
.section .text, code
; X = vertical-byte columns, A = unsigned x, B = unsigned y.
; Height 8, width 1..192; right/bottom clipping. mode 0=OR, 1=XOR.
; Invalid origin/width/mode is a no-op. A/B/X/CC clobbered, non-reentrant.
sprite8:
    CMPA #192
    BCC sprite_return
    CMPB #64
    BCC sprite_return
    STAA sprite_x
    STAB sprite_y
    STX sprite_source
    LDAA sprite_width
    BEQ sprite_return
    CMPA #193
    BCC sprite_return
    STAA sprite_count
    LDAA sprite_mode
    CMPA #2
    BCC sprite_return
    TBA
    ANDA #7
    STAA sprite_shift
    TBA
    LSRA
    LSRA
    LSRA
    LDAB #192
    MUL
    ADDD #framebuffer
    STD sprite_band
sprite_column:
    LDX sprite_source
    LDAA 0,X
    STAA sprite_bits
    CLR sprite_bits + 1
    INX
    STX sprite_source
    LDAB sprite_shift
    BEQ sprite_copy
sprite_align:
    ASL sprite_bits
    ROL sprite_bits + 1
    DECB
    BNE sprite_align
sprite_copy:
    LDX sprite_band
    LDAB sprite_x
    ABX
    LDAA sprite_bits
    JSR sprite_combine
    LDAA sprite_y
    CMPA #56
    BCC sprite_next
    LDAB #192
    ABX
    LDAA sprite_bits + 1
    JSR sprite_combine
sprite_next:
    INC sprite_x
    LDAA sprite_x
    CMPA #192
    BCC sprite_return
    DEC sprite_count
    BNE sprite_column
sprite_return:
    RTS
sprite_combine:
    TST sprite_mode
    BNE sprite_xor
    ORAA 0,X
    STAA 0,X
    RTS
sprite_xor:
    EORA 0,X
    STAA 0,X
    RTS
.section .bss, bss
sprite_width: .space 1
sprite_mode: .space 1
sprite_x: .space 1
sprite_y: .space 1
sprite_shift: .space 1
sprite_count: .space 1
sprite_source: .space 2
sprite_band: .space 2
sprite_bits: .space 2
