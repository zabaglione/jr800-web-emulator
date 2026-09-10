; SPDX-License-Identifier: MIT
.section .text, code
game_render:
    JMP grid_render
game_start:
    JSR grid_reset
    CLR undo_valid
    LDAA stage
    LDAB #99
    MUL
    ADDD #levels
    STD ice_source
    LDX ice_source
    LDAA 0,X
    STAA cursor
    INX
    STX ice_source
    CLR ice_index
ice_load:
    LDX ice_source
    LDAA 0,X
    INX
    STX ice_source
    LDAB ice_index
    LDX #board
    ABX
    STAA 0,X
    INC ice_index
    LDAA ice_index
    CMPA #98
    BNE ice_load
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
