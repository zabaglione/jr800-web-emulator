; SPDX-License-Identifier: MIT
.global undo_valid
.section .text, code
game_start:
    JSR grid_reset
    CLR undo_valid
    LDAA stage
    LDAB #25
    MUL
    ADDD #levels
    STD lamp_source
    CLR lamp_index
lamp_load:
    LDX lamp_source
    LDAA 0,X
    INX
    STX lamp_source
    LDAB lamp_index
    LDX #board
    ABX
    STAA 0,X
    INC lamp_index
    LDAB lamp_index
    CMPB #25
    BNE lamp_load
    JMP lamp_count
game_update:
    LDAA input_event
    BITA #16
    BEQ lamp_move
    LDAA cursor
    STAA lamp_last
    LDAA moves
    STAA lamp_saved_moves
    LDAA #1
    STAA undo_valid
    JSR lamp_toggle
    JSR grid_count_move
    TST grid_stat
    BNE lamp_idle
    LDAA #4
    STAA phase
    RTS
lamp_move:
    JSR grid_move
    CMPB #255
    BEQ lamp_idle
    STAB cursor
    JMP grid_changed
lamp_idle:
    RTS
game_aux:
    CMPA #1
    BNE lamp_restart
    TST undo_valid
    BEQ lamp_idle
    LDAA cursor
    STAA lamp_saved
    LDAA lamp_last
    STAA cursor
    JSR lamp_toggle
    LDAA lamp_saved
    STAA cursor
    CLR undo_valid
    LDAA lamp_saved_moves
    STAA moves
    RTS
lamp_restart:
    JMP game_start
lamp_toggle:
    LDAB cursor
    JSR lamp_flip
    LDX #neighbors
    STX lamp_source
    LDAA #4
    STAA lamp_index
lamp_near:
    LDX lamp_source
    LDAB cursor
    ABX
    LDAB 0,X
    CMPB #255
    BEQ lamp_skip
    JSR lamp_flip
lamp_skip:
    LDD lamp_source
    ADDD #25
    STD lamp_source
    DEC lamp_index
    BNE lamp_near
lamp_count:
    CLR grid_stat
    LDX #board
    LDAB #25
lamp_count_loop:
    TST 0,X
    BEQ lamp_count_next
    INC grid_stat
lamp_count_next:
    INX
    DECB
    BNE lamp_count_loop
    RTS
lamp_flip:
    LDX #board
    ABX
    LDAA 0,X
    EORA #1
    STAA 0,X
    RTS
grid_value:
    LDX #board
    ABX
    LDAA 0,X
    RTS
.section .bss, bss
lamp_source: .space 2
lamp_index: .space 1
lamp_last: .space 1
lamp_saved: .space 1
lamp_saved_moves: .space 1
undo_valid: .space 1
