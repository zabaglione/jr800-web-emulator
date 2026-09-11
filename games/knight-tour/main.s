; SPDX-License-Identifier: MIT
.global knight_player
.global knight_legal
.global knight_stuck
.section .text, code
game_start:
    JSR grid_reset
    JSR challenge_start
    LDX #board
    JSR challenge_load
    LDX #board
    LDAB #36
knight_count_open:
    TST 0,X
    BNE knight_count_next
    INC grid_stat
knight_count_next:
    INX
    DECB
    BNE knight_count_open
    DEC grid_stat
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
    JSR challenge_step
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
    JSR challenge_undo
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
    CMPA #255
    BNE knight_tile_available
    LDAA #39
    RTS
knight_tile_available:
    TSTA
    BNE knight_tile_done
    LDX #knight_legal
    ABX
    TST 0,X
    BEQ knight_tile_done
    LDAA #38
knight_tile_done:
    RTS
game_render:
    JSR paint_board
    JMP visual_hud

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

.section .text, code
game_bonus:
    CLR challenge_bonus
    TST grid_stat
    BNE knight_bonus_done
    LDAA knight_player
    CMPA challenge_cells
    BNE knight_bonus_done
    INC challenge_bonus
knight_bonus_done:
    RTS
