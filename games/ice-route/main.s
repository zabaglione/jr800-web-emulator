; SPDX-License-Identifier: MIT
.section .text, code
game_render:
    JSR paint_board
    JMP visual_hud

game_start:
    JSR grid_reset
    JSR challenge_start
    CLR undo_valid
    LDX #board
    JSR challenge_load
    LDAB stage
    LDX #start_cells
    ABX
    LDAA 0,X
    STAA cursor
    LDAA #2
    STAA grid_stat
    RTS
game_update:
    JSR grid_move
    CMPB #255
    BEQ ice_idle
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BEQ ice_idle
    JSR grid_snapshot
ice_slide:
    JSR grid_move
    CMPB #255
    BEQ ice_stopped
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BEQ ice_stopped
    STAB cursor
    JSR challenge_touch
    CMPA #3
    BCS ice_exit
    CMPA #4
    BHI ice_slide
    CLR 0,X
    DEC grid_stat
    BRA ice_slide
ice_exit:
    CMPA #2
    BNE ice_slide
    TST grid_stat
    BNE ice_slide
    LDAA #4
    STAA phase
ice_stopped:
    JMP grid_count_move
ice_idle:
    RTS
game_aux:
    CMPA #1
    BNE ice_restart
    JMP grid_restore
ice_restart:
    JMP game_start
grid_value:
    CMPB cursor
    BNE ice_tile
    LDAA #5
    RTS
ice_tile:
    LDX #board
    ABX
    LDAA 0,X
    RTS
.section .bss, bss
ice_source: .space 2
ice_index: .space 1

.section .text, code
game_bonus:
    RTS
