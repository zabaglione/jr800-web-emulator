; SPDX-License-Identifier: MIT
.global undo_valid
.section .text, code
game_render:
    JSR paint_board
    JMP visual_hud

game_start:
    JSR grid_reset
    CLR undo_valid
    LDAA stage
    LDAB #9
    MUL
    ADDD #levels
    STD slide_source
    CLR slide_index
slide_load:
    LDX slide_source
    LDAA 0,X
    INX
    STX slide_source
    LDAB slide_index
    LDX #board
    ABX
    STAA 0,X
    TSTA
    BNE slide_loaded
    STAB cursor
slide_loaded:
    INC slide_index
    LDAA slide_index
    CMPA #9
    BNE slide_load
    JMP slide_count
game_update:
    JSR grid_move
    CMPB #255
    BEQ slide_idle
    LDAA cursor
    STAA slide_last
    LDAA moves
    STAA slide_saved_moves
    LDAA #1
    STAA undo_valid
    JSR slide_swap
    JSR grid_count_move
    JSR slide_count
    TST grid_stat
    BNE slide_idle
    LDAA #4
    STAA phase
slide_idle:
    RTS
game_aux:
    CMPA #1
    BNE slide_restart
    TST undo_valid
    BEQ slide_idle
    LDAB slide_last
    JSR slide_swap
    CLR undo_valid
    LDAA slide_saved_moves
    STAA moves
    JMP slide_count
slide_restart:
    JMP game_start
slide_swap:
    STAB slide_target
    LDX #board
    ABX
    LDAA 0,X
    CLR 0,X
    LDX #board
    LDAB cursor
    ABX
    STAA 0,X
    LDAA slide_target
    STAA cursor
    RTS
slide_count:
    CLR grid_stat
    LDX #board
    LDAB #1
slide_count_loop:
    LDAA 0,X
    BEQ slide_count_next
    CBA
    BEQ slide_count_next
    INC grid_stat
slide_count_next:
    INX
    INCB
    CMPB #10
    BNE slide_count_loop
    RTS
grid_value:
    LDX #board
    ABX
    LDAA 0,X
    RTS
.section .bss, bss
slide_source: .space 2
slide_index: .space 1
slide_last: .space 1
slide_target: .space 1
slide_saved_moves: .space 1
undo_valid: .space 1
