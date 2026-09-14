; SPDX-License-Identifier: MIT
; Four opaque bit operations, unrolled eight columns at a time.
; Spans of 3..7 pixels include the black contour in straight-line stores.
; Longer spans leave X one past their end for the contour epilogue.
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
    LDAB span_count
    CMPB #8
    BCC bb_long
bb_short:
    ASLB
    LDX #bb_short_table
    ABX
    LDX 0,X
    JMP 0,X
bb_long:
    LDAA span_count
    ANDA #7
    STAA span_tail
    LDAA span_count
    LSRA
    LSRA
    LSRA
    STAA span_count
    LDX row_start
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
    LDAB span_count
    CMPB #8
    BCC bw_long
bw_short:
    ASLB
    LDX #bw_short_table
    ABX
    LDX 0,X
    JMP 0,X
bw_long:
    LDAA span_count
    ANDA #7
    STAA span_tail
    LDAA span_count
    LSRA
    LSRA
    LSRA
    STAA span_count
    LDX row_start
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
    LDAB span_count
    CMPB #8
    BCC wb_long
wb_short:
    ASLB
    LDX #wb_short_table
    ABX
    LDX 0,X
    JMP 0,X
wb_long:
    LDAA span_count
    ANDA #7
    STAA span_tail
    LDAA span_count
    LSRA
    LSRA
    LSRA
    STAA span_count
    LDX row_start
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
    LDAB span_count
    CMPB #8
    BCC ww_long
ww_short:
    ASLB
    LDX #ww_short_table
    ABX
    LDX 0,X
    JMP 0,X
ww_long:
    LDAA span_count
    ANDA #7
    STAA span_tail
    LDAA span_count
    LSRA
    LSRA
    LSRA
    STAA span_count
    LDX row_start
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

bb_3:
    LDX row_start
    LDD 0,X
    ORAA row_mask
    ORAB row_mask
    STD 0,X
    LDAA 2,X
    ORAA row_mask
    STAA 2,X
    JMP row_done
bb_4:
    LDX row_start
    LDD 0,X
    ORAA row_mask
    ORAB row_mask
    STD 0,X
    LDD 2,X
    ORAA row_mask
    ORAB row_mask
    STD 2,X
    JMP row_done
bb_5:
    LDX row_start
    LDD 0,X
    ORAA row_mask
    ORAB row_mask
    STD 0,X
    LDD 2,X
    ORAA row_mask
    ORAB row_mask
    STD 2,X
    LDAA 4,X
    ORAA row_mask
    STAA 4,X
    JMP row_done
bb_6:
    LDX row_start
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
    JMP row_done
bb_7:
    LDX row_start
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
    LDAA 6,X
    ORAA row_mask
    STAA 6,X
    JMP row_done

bw_3:
    LDX row_start
    LDD 0,X
    ORAA row_mask
    ANDB clear_mask
    STD 0,X
    LDAA 2,X
    ORAA row_mask
    STAA 2,X
    JMP row_done
bw_4:
    LDX row_start
    LDD 0,X
    ORAA row_mask
    ANDB clear_mask
    STD 0,X
    LDD 2,X
    ORAA row_mask
    ORAB row_mask
    STD 2,X
    JMP row_done
bw_5:
    LDX row_start
    LDD 0,X
    ORAA row_mask
    ANDB clear_mask
    STD 0,X
    LDD 2,X
    ORAA row_mask
    ANDB clear_mask
    STD 2,X
    LDAA 4,X
    ORAA row_mask
    STAA 4,X
    JMP row_done
bw_6:
    LDX row_start
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
    ORAB row_mask
    STD 4,X
    JMP row_done
bw_7:
    LDX row_start
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
    LDAA 6,X
    ORAA row_mask
    STAA 6,X
    JMP row_done

wb_3:
    LDX row_start
    LDD 0,X
    ORAA row_mask
    ORAB row_mask
    STD 0,X
    LDAA 2,X
    ORAA row_mask
    STAA 2,X
    JMP row_done
wb_4:
    LDX row_start
    LDD 0,X
    ORAA row_mask
    ORAB row_mask
    STD 0,X
    LDD 2,X
    ANDA clear_mask
    ORAB row_mask
    STD 2,X
    JMP row_done
wb_5:
    LDX row_start
    LDD 0,X
    ORAA row_mask
    ORAB row_mask
    STD 0,X
    LDD 2,X
    ANDA clear_mask
    ORAB row_mask
    STD 2,X
    LDAA 4,X
    ORAA row_mask
    STAA 4,X
    JMP row_done
wb_6:
    LDX row_start
    LDD 0,X
    ORAA row_mask
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
    JMP row_done
wb_7:
    LDX row_start
    LDD 0,X
    ORAA row_mask
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
    LDAA 6,X
    ORAA row_mask
    STAA 6,X
    JMP row_done

ww_3:
    LDX row_start
    LDD 0,X
    ORAA row_mask
    ANDB clear_mask
    STD 0,X
    LDAA 2,X
    ORAA row_mask
    STAA 2,X
    JMP row_done
ww_4:
    LDX row_start
    LDD 0,X
    ORAA row_mask
    ANDB clear_mask
    STD 0,X
    LDD 2,X
    ANDA clear_mask
    ORAB row_mask
    STD 2,X
    JMP row_done
ww_5:
    LDX row_start
    LDD 0,X
    ORAA row_mask
    ANDB clear_mask
    STD 0,X
    LDD 2,X
    ANDA clear_mask
    ANDB clear_mask
    STD 2,X
    LDAA 4,X
    ORAA row_mask
    STAA 4,X
    JMP row_done
ww_6:
    LDX row_start
    LDD 0,X
    ORAA row_mask
    ANDB clear_mask
    STD 0,X
    LDD 2,X
    ANDA clear_mask
    ANDB clear_mask
    STD 2,X
    LDD 4,X
    ANDA clear_mask
    ORAB row_mask
    STD 4,X
    JMP row_done
ww_7:
    LDX row_start
    LDD 0,X
    ORAA row_mask
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
    LDAA 6,X
    ORAA row_mask
    STAA 6,X
    JMP row_done

.section .data, data
bb_short_table:
    .word row_done, row_done, row_done, bb_3, bb_4, bb_5, bb_6, bb_7
bw_short_table:
    .word row_done, row_done, row_done, bw_3, bw_4, bw_5, bw_6, bw_7
wb_short_table:
    .word row_done, row_done, row_done, wb_3, wb_4, wb_5, wb_6, wb_7
ww_short_table:
    .word row_done, row_done, row_done, ww_3, ww_4, ww_5, ww_6, ww_7
