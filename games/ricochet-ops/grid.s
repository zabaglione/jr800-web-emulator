; SPDX-License-Identifier: MIT
; This shooting board has 48 cells and a separately drawn moving cannon.
.global board
.global cursor
.section .text, code
grid_reset:
    LDX #board
    LDAB #112
    CLRA
rico_grid_zero:
    STAA 0,X
    INX
    DECB
    BNE rico_grid_zero
    CLR cursor
    RTS
game_tile:
    LDX #view_cells
    ABX
    LDAA 0,X
    CMPA #255
    BEQ rico_grid_blank
    STAA grid_cell
    LDX #view_subtiles
    ABX
    LDAA 0,X
    STAA grid_subtile
    LDAB grid_cell
    JSR grid_value
    ASLA
    ADDA grid_subtile
    INCA
    RTS
rico_grid_blank:
    CLRA
    RTS
.section .bss, bss
board: .space 112
cursor: .space 1
grid_cell: .space 1
grid_subtile: .space 1
