; SPDX-License-Identifier: MIT
.global rico_aim
.global rico_active
.global rico_ammo
.global rico_left
.global rico_score
.global rico_bullet
.global rico_direction
.global rico_visited
.global rico_trail
.global rico_steps
.global rico_prev_valid
.global rico_step
.global rico_fire
.section .text, code
game_start:
    JSR grid_reset
    CLR rico_aim
    CLR rico_active
    CLR rico_score
    CLR rico_score + 1
    CLR rico_steps
    CLR rico_prev_valid
    LDAA #3
    STAA rico_ammo
    STAA rico_left
    LDAA #41
    STAA cursor
    LDAA stage
    LDAB #48
    MUL
    ADDD #rico_levels
    STD rico_pointer
    CLR rico_index
rico_load:
    LDX rico_pointer
    LDAA 0,X
    INX
    STX rico_pointer
    LDAB rico_index
    LDX #board
    ABX
    STAA 0,X
    INC rico_index
    LDAA rico_index
    CMPA #48
    BNE rico_load
    LDAA #255
    STAA rico_bullet
    JMP rico_clear_path
game_aux:
    CMPA #2
    BNE rico_undo
    JMP game_start
rico_undo:
    TST rico_prev_valid
    BNE rico_restore
    RTS
rico_restore:
    CLR rico_prev_valid
    CLR rico_active
    LDAA rico_prev_cursor
    STAA cursor
    LDAA rico_prev_aim
    STAA rico_aim
    LDAA rico_prev_ammo
    STAA rico_ammo
    LDAA rico_prev_left
    STAA rico_left
    LDD rico_prev_score
    STD rico_score
    CLR rico_index
rico_restore_board:
    LDAB rico_index
    LDX #rico_previous
    ABX
    LDAA 0,X
    LDX #board
    ABX
    STAA 0,X
    INC rico_index
    LDAA rico_index
    CMPA #48
    BNE rico_restore_board
    LDAA #255
    STAA rico_bullet
    JMP rico_clear_path
game_update:
    TST resume_pending
    BEQ rico_mode
    CLR resume_pending
    LDAA input_ticks
    STAA rico_clock
rico_mode:
    TST rico_active
    BEQ rico_input
    LDAA input_ticks
    SUBA rico_clock
    CMPA #6
    BCS rico_update_done
    LDAA input_ticks
    STAA rico_clock
    JSR rico_step
    LDAA #1
    STAA redraw
    RTS
rico_input:
    LDAA input_event
    BITA #1
    BEQ rico_down
    INC rico_aim
    LDAA rico_aim
    ANDA #7
    STAA rico_aim
    BRA rico_changed
rico_down:
    BITA #2
    BEQ rico_left_input
    LDAA rico_aim
    DECA
    ANDA #7
    STAA rico_aim
    BRA rico_changed
rico_left_input:
    BITA #4
    BEQ rico_right_input
    LDAA cursor
    CMPA #41
    BEQ rico_space
    DEC cursor
    BRA rico_changed
rico_right_input:
    BITA #8
    BEQ rico_space
    LDAA cursor
    CMPA #46
    BEQ rico_space
    INC cursor
rico_changed:
    LDAA #1
    STAA redraw
rico_space:
    LDAA input_event
    BITA #16
    BEQ rico_update_done
    JMP rico_fire
rico_update_done:
    RTS
rico_fire:
    LDAA #1
    STAA rico_prev_valid
    STAA rico_active
    STAA redraw
    LDAA cursor
    STAA rico_prev_cursor
    STAA rico_bullet
    LDAA rico_aim
    STAA rico_prev_aim
    STAA rico_direction
    LDAA rico_ammo
    STAA rico_prev_ammo
    DEC rico_ammo
    LDAA rico_left
    STAA rico_prev_left
    LDD rico_score
    STD rico_prev_score
    CLR rico_index
rico_save_board:
    LDAB rico_index
    LDX #board
    ABX
    LDAA 0,X
    LDX #rico_previous
    ABX
    STAA 0,X
    INC rico_index
    LDAA rico_index
    CMPA #48
    BNE rico_save_board
    LDAA input_ticks
    STAA rico_clock
    JMP rico_clear_path
rico_clear_path:
    LDX #rico_visited
    LDAB #192
    CLRA
rico_clear_seen:
    STAA 0,X
    STAA 192,X
    INX
    DECB
    BNE rico_clear_seen
    LDX #rico_trail
    LDAB #48
rico_clear_trail:
    STAA 0,X
    INX
    DECB
    BNE rico_clear_trail
    RTS
rico_step:
    INC rico_steps
    LDAA rico_bullet
    ANDA #7
    STAA rico_column
    LDAB rico_direction
    LDX #rico_dx
    ABX
    LDAA 0,X
    ADDA rico_column
    CMPA #8
    BCS rico_continue_1
    JMP rico_stop
rico_continue_1:
    STAA rico_column
    LDAA rico_bullet
    LSRA
    LSRA
    LSRA
    STAA rico_row
    LDX #rico_dy
    ABX
    LDAA 0,X
    ADDA rico_row
    CMPA #6
    BCS rico_continue_2
    JMP rico_stop
rico_continue_2:
    ASLA
    ASLA
    ASLA
    ADDA rico_column
    CMPA cursor
    BNE rico_continue_3
    JMP rico_stop
rico_continue_3:
    STAA rico_next_cell
    TAB
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BNE rico_continue_4
    JMP rico_stop
rico_continue_4:
    STAA rico_cell_value
    LDAA rico_next_cell
    LDAB #8
    MUL
    ADDB rico_direction
    ADCA #0
    ADDD #rico_visited
    XGDX
    TST 0,X
    BEQ rico_continue_5
    JMP rico_stop
rico_continue_5:
    INC 0,X
    LDAB rico_next_cell
    STAB rico_bullet
    LDX #rico_trail
    ABX
    LDAA #1
    STAA 0,X
    LDAA rico_cell_value
    CMPA #4
    BEQ rico_target
    CMPA #2
    BEQ rico_slash
    CMPA #3
    BEQ rico_back
    RTS
rico_slash:
    LDX #rico_slash_turn
    BRA rico_reflect
rico_back:
    LDX #rico_back_turn
rico_reflect:
    LDAB rico_direction
    ABX
    LDAA 0,X
    STAA rico_direction
    RTS
rico_target:
    LDX #board
    ABX
    CLR 0,X
    DEC rico_left
    LDD rico_score
    ADDD #100
    STD rico_score
    TST rico_left
    BNE rico_step_done
    LDAA #4
    STAA phase
rico_stop:
    CLR rico_active
    LDAA #255
    STAA rico_bullet
    LDAA phase
    CMPA #4
    BEQ rico_step_done
    TST rico_ammo
    BNE rico_step_done
    LDAA #5
    STAA phase
rico_step_done:
    RTS
grid_value:
    CMPB cursor
    BNE rico_tile_bullet
    LDAA rico_aim
    ADDA #5
    RTS
rico_tile_bullet:
    CMPB rico_bullet
    BNE rico_tile_board
    LDAA #14
    RTS
rico_tile_board:
    LDX #board
    ABX
    LDAA 0,X
    BNE rico_tile_done
    LDX #rico_trail
    ABX
    TST 0,X
    BEQ rico_tile_done
    LDAA #13
rico_tile_done:
    RTS
game_render:
    JSR paint_board
    JMP visual_hud
.section .bss, bss
rico_aim: .space 1
rico_active: .space 1
rico_ammo: .space 1
rico_left: .space 1
rico_score: .space 2
rico_bullet: .space 1
rico_direction: .space 1
rico_visited: .space 384
rico_trail: .space 48
rico_previous: .space 48
rico_steps: .space 1
rico_prev_valid: .space 1
rico_prev_cursor: .space 1
rico_prev_aim: .space 1
rico_prev_ammo: .space 1
rico_prev_left: .space 1
rico_prev_score: .space 2
rico_clock: .space 1
rico_pointer: .space 2
rico_index: .space 1
rico_column: .space 1
rico_row: .space 1
rico_next_cell: .space 1
rico_cell_value: .space 1
rico_hud_force: .space 1
rico_old_aim: .space 1
rico_old_left: .space 1
rico_old_ammo: .space 1
rico_old_score: .space 2
rico_old_active: .space 1
.section .data, data
rico_dx: .byte 0,1,1,1,0,255,255,255
rico_dy: .byte 255,255,0,1,1,1,0,255
rico_slash_turn: .byte 2,1,0,7,6,5,4,3
rico_back_turn: .byte 6,5,4,3,2,1,0,7
rico_aim_label: .byte 65,73,77,0
rico_left_label: .byte 76,69,70,84,0
rico_ammo_label: .byte 65,77,77,79,0
rico_score_label: .byte 83,0
rico_fire_label: .byte 83,80,65,67,69,32,70,73,82,69,0
rico_flight_label: .byte 73,78,32,70,76,73,71,72,84,32,0
rico_aim_names:
    .byte 85,80,32,32,32,0
    .byte 85,80,45,82,32,0
    .byte 82,73,71,72,84,0
    .byte 68,78,45,82,32,0
    .byte 68,79,87,78,32,0
    .byte 68,78,45,76,32,0
    .byte 76,69,70,84,32,0
    .byte 85,80,45,76,32,0
