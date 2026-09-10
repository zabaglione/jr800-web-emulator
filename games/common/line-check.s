; SPDX-License-Identifier: MIT
.global line_last
.global line_longest
.global line_open
.section .text, code
; Check four axes through the most recently placed piece, with explicit edges.
line_score:
    LDAB line_last
    LDX #board
    ABX
    LDAA 0,X
    STAA line_piece
    CLR line_axis
    CLR line_longest
    CLR line_open
line_axis_loop:
    LDAA #1
    STAA line_count
    CLR line_side
    CLR line_axis_open
    LDAA line_axis
    LDAB #CELLS * 2
    MUL
    ADDD #line_links
    STD line_pointer
line_side_loop:
    LDAA line_last
    STAA line_walk
line_walk_loop:
    LDX line_pointer
    LDAB line_walk
    ABX
    LDAB 0,X
    CMPB #255
    BEQ line_side_done
    STAB line_walk
    LDX #board
    ABX
    LDAA 0,X
    CMPA line_piece
    BNE line_side_stop
    INC line_count
    BRA line_walk_loop
line_side_stop:
    TSTA
    BNE line_side_done
    INC line_axis_open
line_side_done:
    INC line_side
    LDAA line_side
    CMPA #2
    BEQ line_axis_done
    LDD line_pointer
    ADDD #CELLS
    STD line_pointer
    BRA line_side_loop
line_axis_done:
    LDAA line_count
    CMPA line_longest
    BCS line_next_axis
    BNE line_new_longest
    LDAA line_axis_open
    CMPA line_open
    BLS line_next_axis
    STAA line_open
    BRA line_next_axis
line_new_longest:
    STAA line_longest
    LDAA line_axis_open
    STAA line_open
line_next_axis:
    INC line_axis
    LDAA line_axis
    CMPA #4
    BEQ line_score_done
    JMP line_axis_loop
line_score_done:
    RTS
.section .bss, bss
line_last: .space 1
line_piece: .space 1
line_axis: .space 1
line_side: .space 1
line_count: .space 1
line_longest: .space 1
line_pointer: .space 2
line_walk: .space 1
line_open: .space 1
line_axis_open: .space 1
