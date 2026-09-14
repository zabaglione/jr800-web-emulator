; SPDX-License-Identifier: MIT
; Four opaque bit operations, unrolled eight columns at a time.
; X exits one byte past the span, matching the shared contour epilogue.
.equ row_start, $BA
.equ span_tail, $BD
.equ span_count, $BE
.equ row_mask, $BF
.equ clear_mask, $C0
.extern span_done
.extern row_done
.global span_bb
.global span_bw
.global span_wb
.global span_ww
.section .text, code
span_bb:
    LDX row_start
    TST span_count
    BEQ bb_tail
bb_block:
    LDD 0,X
    ORAA row_mask
    ORAB row_mask
    STD 0,X
    LDD 2,X
    ORAA row_mask
    ORAB row_mask
    STD 2,X
    LDD 4,X
    ORAA row_mask
    ORAB row_mask
    STD 4,X
    LDD 6,X
    ORAA row_mask
    ORAB row_mask
    STD 6,X
    LDAB #8
    ABX
    DEC span_count
    BNE bb_block
bb_tail:
    LDAB span_tail
    BEQ bb_done
bb_first:
    LDAA 0,X
    ORAA row_mask
    STAA 0,X
    INX
    DECB
    BEQ bb_done
    LDAA 0,X
    ORAA row_mask
    STAA 0,X
    INX
    DECB
    BNE bb_first
bb_done:
    JMP row_done
span_bw:
    LDX row_start
    TST span_count
    BEQ bw_tail
bw_block:
    LDD 0,X
    ORAA row_mask
    ANDB clear_mask
    STD 0,X
    LDD 2,X
    ORAA row_mask
    ANDB clear_mask
    STD 2,X
    LDD 4,X
    ORAA row_mask
    ANDB clear_mask
    STD 4,X
    LDD 6,X
    ORAA row_mask
    ANDB clear_mask
    STD 6,X
    LDAB #8
    ABX
    DEC span_count
    BNE bw_block
bw_tail:
    LDAB span_tail
    BEQ bw_done
bw_first:
    LDAA 0,X
    ORAA row_mask
    STAA 0,X
    INX
    DECB
    BEQ bw_done
    LDAA 0,X
    ANDA clear_mask
    STAA 0,X
    INX
    DECB
    BNE bw_first
bw_done:
    JMP span_done
span_wb:
    LDX row_start
    TST span_count
    BEQ wb_tail
wb_block:
    LDD 0,X
    ANDA clear_mask
    ORAB row_mask
    STD 0,X
    LDD 2,X
    ANDA clear_mask
    ORAB row_mask
    STD 2,X
    LDD 4,X
    ANDA clear_mask
    ORAB row_mask
    STD 4,X
    LDD 6,X
    ANDA clear_mask
    ORAB row_mask
    STD 6,X
    LDAB #8
    ABX
    DEC span_count
    BNE wb_block
wb_tail:
    LDAB span_tail
    BEQ wb_done
wb_first:
    LDAA 0,X
    ANDA clear_mask
    STAA 0,X
    INX
    DECB
    BEQ wb_done
    LDAA 0,X
    ORAA row_mask
    STAA 0,X
    INX
    DECB
    BNE wb_first
wb_done:
    JMP span_done
span_ww:
    LDX row_start
    TST span_count
    BEQ ww_tail
ww_block:
    LDD 0,X
    ANDA clear_mask
    ANDB clear_mask
    STD 0,X
    LDD 2,X
    ANDA clear_mask
    ANDB clear_mask
    STD 2,X
    LDD 4,X
    ANDA clear_mask
    ANDB clear_mask
    STD 4,X
    LDD 6,X
    ANDA clear_mask
    ANDB clear_mask
    STD 6,X
    LDAB #8
    ABX
    DEC span_count
    BNE ww_block
ww_tail:
    LDAB span_tail
    BEQ ww_done
ww_first:
    LDAA 0,X
    ANDA clear_mask
    STAA 0,X
    INX
    DECB
    BEQ ww_done
    LDAA 0,X
    ANDA clear_mask
    STAA 0,X
    INX
    DECB
    BNE ww_first
ww_done:
    JMP span_done
