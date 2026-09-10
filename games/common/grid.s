; SPDX-License-Identifier: MIT
; Logical cells can cover several 8x8 LCD tiles. All mapping is precomputed.
.global board
.global cursor
.global moves
.global grid_stat
.section .text, code
grid_reset:
    LDX #board
    LDAB #112
    CLRA
grid_zero:
    STAA 0,X
    INX
    DECB
    BNE grid_zero
    CLR cursor
    CLR moves
    CLR grid_stat
    RTS
; B = neighbour, $FF when no direction or outside the board. Cursor unchanged.
grid_move:
    LDAA input_event
    BITA #1
    BEQ grid_down
    LDX #neighbors
    BRA grid_neighbor
grid_down:
    BITA #2
    BEQ grid_left
    LDX #neighbors + CELLS
    BRA grid_neighbor
grid_left:
    BITA #4
    BEQ grid_right
    LDX #neighbors + CELLS * 2
    BRA grid_neighbor
grid_right:
    BITA #8
    BEQ grid_no_move
    LDX #neighbors + CELLS * 3
grid_neighbor:
    LDAB cursor
    ABX
    LDAB 0,X
    RTS
grid_no_move:
    LDAB #255
    RTS
grid_count_move:
    LDAA moves
    CMPA #255
    BEQ grid_changed
    INC moves
grid_changed:
    LDAA #1
    STAA redraw
    RTS
game_tile:
    LDX #view_cells
    ABX
    LDAA 0,X
    CMPA #255
    BEQ grid_blank
    STAA grid_cell
    LDX #view_subtiles
    ABX
    LDAA 0,X
    STAA grid_subtile
    LDAB grid_cell
    JSR grid_value
    LDAB #TILE_STRIDE
    MUL
    ADDB grid_subtile
    INCB
    TBA
    LDAB grid_cell
    CMPB cursor
    BNE grid_tile_done
    ORAA #128
grid_tile_done:
    RTS
grid_blank:
    CLRA
    RTS
.section .bss, bss
board: .space 112
cursor: .space 1
moves: .space 1
grid_stat: .space 1
grid_cell: .space 1
grid_subtile: .space 1
.section .data, data
grid_moves_label: .byte 77,79,86,69,83,0
grid_return_label: .byte 82,69,84,85,82,78,0
