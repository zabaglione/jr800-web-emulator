; SPDX-License-Identifier: MIT
.global gates
.section .text, code
game_start:
    JSR grid_reset
    CLR undo_valid
    CLR gates
    LDAA stage
    LDAB #99
    MUL
    ADDD #levels
    STD switch_source
    LDX switch_source
    LDAA 0,X
    STAA cursor
    INX
    STX switch_source
    CLR switch_index
switch_load:
    LDX switch_source
    LDAA 0,X
    INX
    STX switch_source
    LDAB switch_index
    LDX #board
    ABX
    STAA 0,X
    INC switch_index
    LDAA switch_index
    CMPA #98
    BNE switch_load
    LDAA #2
    STAA grid_stat
    RTS
game_update:
    JSR grid_move
    CMPB #255
    BNE switch_candidate
    RTS
switch_candidate:
    STAB switch_target
    LDX #board
    ABX
    LDAA 0,X
    STAA switch_tile
    CMPA #1
    BNE switch_door_a
    RTS
switch_door_a:
    CMPA #6
    BNE switch_door_b
    LDAA gates
    BITA #1
    BNE switch_accept
    RTS
switch_door_b:
    CMPA #7
    BNE switch_accept
    LDAA gates
    BITA #2
    BNE switch_accept
    RTS
switch_accept:
    JSR grid_snapshot
    LDAA gates
    STAA switch_saved_gates
    LDAB switch_target
    STAB cursor
    LDAA switch_tile
    CMPA #2
    BEQ switch_key
    CMPA #3
    BEQ switch_key
    CMPA #4
    BEQ switch_a
    CMPA #5
    BEQ switch_b
    CMPA #8
    BNE switch_finish
    TST grid_stat
    BNE switch_finish
    LDAA #4
    STAA phase
    BRA switch_finish
switch_key:
    LDX #board
    ABX
    CLR 0,X
    DEC grid_stat
    BRA switch_finish
switch_a:
    LDAA gates
    EORA #1
    STAA gates
    BRA switch_finish
switch_b:
    LDAA gates
    EORA #2
    STAA gates
switch_finish:
    JMP grid_count_move
game_aux:
    CMPA #1
    BNE switch_restart
    TST undo_valid
    BEQ switch_idle
    JSR grid_restore
    LDAA switch_saved_gates
    STAA gates
switch_idle:
    RTS
switch_restart:
    JMP game_start
grid_value:
    CMPB cursor
    BNE switch_board_tile
    LDAA #11
    RTS
switch_board_tile:
    LDX #board
    ABX
    LDAA 0,X
    CMPA #6
    BNE switch_tile_b
    LDAB gates
    BITB #1
    BEQ switch_tile_done
    LDAA #9
    RTS
switch_tile_b:
    CMPA #7
    BNE switch_tile_done
    LDAB gates
    BITB #2
    BEQ switch_tile_done
    LDAA #10
switch_tile_done:
    RTS
.section .bss, bss
gates: .space 1
switch_saved_gates: .space 1
switch_source: .space 2
switch_index: .space 1
switch_target: .space 1
switch_tile: .space 1
