; SPDX-License-Identifier: MIT
.global knight_player
.global knight_legal
.global knight_stuck
.section .text, code
game_start:
    JSR grid_reset
    LDAB stage
    LDX #knight_starts
    ABX
    LDAB 0,X
    STAB cursor
    STAB knight_player
    STAB knight_history
    LDX #board
    ABX
    LDAA #1
    STAA 0,X
    LDAA #35
    STAA grid_stat
    JMP knight_options
game_update:
    LDAA input_event
    BITA #16
    BNE knight_jump
    JSR grid_move
    CMPB #255
    BEQ knight_idle
    STAB cursor
    JMP grid_changed
knight_jump:
    LDAB cursor
    LDX #knight_legal
    ABX
    TST 0,X
    BEQ knight_idle
    STAB knight_player
    INC moves
    LDAA moves
    TAB
    LDX #knight_history
    ABX
    LDAA knight_player
    STAA 0,X
    LDAB knight_player
    LDX #board
    ABX
    LDAA moves
    INCA
    STAA 0,X
    DEC grid_stat
    JSR knight_options
    JSR grid_changed
    TST grid_stat
    BNE knight_idle
    LDAA #4
    STAA phase
knight_idle:
    RTS
knight_options:
    LDX #knight_legal
    LDAB #36
    CLRA
knight_clear:
    STAA 0,X
    INX
    DECB
    BNE knight_clear
    LDAA #1
    STAA knight_stuck
    LDAA knight_player
    LDAB #8
    MUL
    ADDD #knight_links
    STD knight_pointer
    CLR knight_index
knight_option_loop:
    LDX knight_pointer
    LDAB knight_index
    ABX
    LDAB 0,X
    CMPB #255
    BEQ knight_option_next
    LDX #board
    ABX
    TST 0,X
    BNE knight_option_next
    LDX #knight_legal
    ABX
    LDAA #1
    STAA 0,X
    CLR knight_stuck
knight_option_next:
    INC knight_index
    LDAA knight_index
    CMPA #8
    BNE knight_option_loop
    TST grid_stat
    BNE knight_options_done
    CLR knight_stuck
knight_options_done:
    RTS
game_aux:
    CMPA #1
    BNE knight_restart
    TST moves
    BEQ knight_idle
    LDAB knight_player
    LDX #board
    ABX
    CLR 0,X
    DEC moves
    INC grid_stat
    LDAB moves
    LDX #knight_history
    ABX
    LDAA 0,X
    STAA knight_player
    STAA cursor
    JMP knight_options
knight_restart:
    JMP game_start
grid_value:
    CMPB knight_player
    BNE knight_visited
    LDAA #37
    RTS
knight_visited:
    LDX #board
    ABX
    LDAA 0,X
    BNE knight_tile_done
    LDX #knight_legal
    ABX
    TST 0,X
    BEQ knight_tile_done
    LDAA #38
knight_tile_done:
    RTS
game_render:
    JSR grid_render
    LDX #knight_blank_label
    LDAA #132
    LDAB #5
    JSR paint_text
    LDX #knight_jump_label
    TST knight_stuck
    BEQ knight_status
    LDX #knight_stuck_label
knight_status:
    LDAA #138
    LDAB #5
    JMP paint_text
.section .bss, bss
knight_player: .space 1
knight_legal: .space 36
knight_history: .space 36
knight_stuck: .space 1
knight_pointer: .space 2
knight_index: .space 1
.section .data, data
knight_blank_label: .byte 32,32,32,32,32,32,32,32,32,32,0
knight_jump_label: .byte 74,85,77,80,0
knight_stuck_label: .byte 83,84,85,67,75,0
