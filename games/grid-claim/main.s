; SPDX-License-Identifier: MIT
.global claim_mode
.global claim_enemy
.global claim_dir
.global claim_queued
.global claim_cpu_dir
.global claim_arena
.global claim_wins
.global claim_losses
.global claim_steps
.global claim_player_next
.global claim_ai
.global claim_area
.global claim_step
.section .text, code
game_start:
    CLR claim_wins
    CLR claim_losses
    CLR claim_steps
    LDAA stage
    ASLA
    STAA claim_arena
claim_round:
    JSR grid_reset
    LDAA claim_arena
    LDAB #112
    MUL
    ADDD #claim_arenas
    STD claim_pointer
    CLR claim_index
claim_load:
    LDX claim_pointer
    LDAA 0,X
    INX
    STX claim_pointer
    LDAB claim_index
    LDX #board
    ABX
    STAA 0,X
    INC claim_index
    LDAA claim_index
    CMPA #112
    BNE claim_load
    LDAA #50
    STAA cursor
    LDAA #61
    STAA claim_enemy
    LDAA #1
    STAA board + 50
    LDAA #2
    STAA board + 61
    STAA claim_cpu_dir
    LDAA #3
    STAA claim_dir
    STAA claim_queued
    CLR claim_mode
    LDAA input_ticks
    STAA claim_clock
    RTS
game_aux:
    CMPA #2
    BNE claim_aux_pause
    JMP game_start
claim_aux_pause:
    LDAA claim_mode
    CMPA #1
    BNE claim_aux_done
    CLR claim_mode
claim_aux_done:
    RTS
game_update:
    TST resume_pending
    BEQ claim_input
    CLR resume_pending
    LDAA input_ticks
    STAA claim_clock
claim_input:
    LDAA claim_mode
    CMPA #2
    BCS claim_direction_input
    LDAA input_event
    BITA #16
    BNE claim_round_confirm
    RTS
claim_round_confirm:
    INC claim_arena
    LDAA claim_arena
    CMPA #6
    BCS claim_next_arena
    CLR claim_arena
claim_next_arena:
    JSR input_gate
    JSR paint_clear
    JSR claim_round
    LDAA #1
    STAA resume_pending
    STAA redraw
    RTS
claim_direction_input:
    LDAA input_event
    CLRB
    BITA #1
    BNE claim_accept_direction
    INCB
    BITA #2
    BNE claim_accept_direction
    INCB
    BITA #4
    BNE claim_accept_direction
    INCB
    BITA #8
    BEQ claim_start_input
claim_accept_direction:
    STAB claim_temp_dir
    LDAB claim_dir
    LDX #claim_opposite
    ABX
    LDAA 0,X
    CMPA claim_temp_dir
    BEQ claim_start_input
    LDAA claim_temp_dir
    STAA claim_queued
claim_start_input:
    LDAA input_event
    BITA #16
    BEQ claim_timer
    TST claim_mode
    BNE claim_timer
    LDAA #1
    STAA claim_mode
    STAA redraw
    LDAA input_ticks
    STAA claim_clock
claim_timer:
    LDAA claim_mode
    CMPA #1
    BNE claim_update_done
    LDAB stage
    LDX #claim_periods
    ABX
    LDAA input_ticks
    SUBA claim_clock
    CMPA 0,X
    BCS claim_update_done
    LDAA input_ticks
    STAA claim_clock
    JSR claim_step
    LDAA #1
    STAA redraw
claim_update_done:
    RTS
; A=direction, B=cell. The precomputed table clips all screen edges.
claim_neighbor:
    STAB claim_neighbor_cell
    LDAB #112
    MUL
    ADDD #neighbors
    XGDX
    LDAB claim_neighbor_cell
    ABX
    LDAB 0,X
    RTS
claim_step:
    INC claim_steps
    LDAA claim_queued
    STAA claim_dir
    LDAB cursor
    JSR claim_neighbor
    STAB claim_player_next
    JSR claim_ai
    STAB claim_cpu_next
    CLR claim_player_bad
    LDAB claim_player_next
    CMPB #255
    BEQ claim_player_crash
    LDX #board
    ABX
    TST 0,X
    BEQ claim_check_cpu
claim_player_crash:
    INC claim_player_bad
claim_check_cpu:
    LDAA claim_cpu_next
    CMPA #255
    BEQ claim_cpu_crash
    CMPA claim_player_next
    BEQ claim_draw
    TST claim_player_bad
    BNE claim_lost
    LDAB claim_player_next
    STAB cursor
    LDX #board
    ABX
    LDAA #1
    STAA 0,X
    LDAB claim_cpu_next
    STAB claim_enemy
    LDX #board
    ABX
    LDAA #2
    STAA 0,X
    RTS
claim_cpu_crash:
    TST claim_player_bad
    BNE claim_draw
    INC claim_wins
    LDAA #2
    STAA claim_mode
    LDAA claim_wins
    CMPA #2
    BNE claim_step_done
    LDAA #4
    STAA phase
    RTS
claim_lost:
    INC claim_losses
    LDAA #3
    STAA claim_mode
    LDAA claim_losses
    CMPA #2
    BNE claim_step_done
    LDAA #5
    STAA phase
    RTS
claim_draw:
    LDAA #4
    STAA claim_mode
claim_step_done:
    RTS
; Three difficulty policies evaluate legal turns without changing the board.
claim_ai:
    LDAA #255
    STAA claim_best_cell
    CLR claim_best_score
    CLR claim_ai_dir
claim_ai_loop:
    LDAB claim_cpu_dir
    LDX #claim_opposite
    ABX
    LDAA 0,X
    CMPA claim_ai_dir
    BNE claim_branch_1
    JMP claim_ai_next
claim_branch_1:
    LDAA claim_ai_dir
    LDAB claim_enemy
    JSR claim_neighbor
    CMPB #255
    BNE claim_branch_2
    JMP claim_ai_next
claim_branch_2:
    LDX #board
    ABX
    TST 0,X
    BEQ claim_branch_3
    JMP claim_ai_next
claim_branch_3:
    STAB claim_candidate
    CLR claim_score
    LDAA stage
    BEQ claim_easy_score
    CMPA #1
    BEQ claim_degree
    JSR claim_area
    ASLA
    STAA claim_score
    BRA claim_straight_bonus
claim_degree:
    CLR claim_local_dir
claim_degree_loop:
    LDAA claim_local_dir
    LDAB claim_candidate
    JSR claim_neighbor
    CMPB #255
    BEQ claim_degree_next
    LDX #board
    ABX
    TST 0,X
    BNE claim_degree_next
    LDAA claim_score
    ADDA #4
    STAA claim_score
claim_degree_next:
    INC claim_local_dir
    LDAA claim_local_dir
    CMPA #4
    BNE claim_degree_loop
    BRA claim_straight_bonus
claim_easy_score:
    LDAA #1
    STAA claim_score
claim_straight_bonus:
    LDAA claim_ai_dir
    CMPA claim_cpu_dir
    BNE claim_choose
    INC claim_score
claim_choose:
    LDAA claim_best_cell
    CMPA #255
    BEQ claim_take
    LDAA claim_score
    CMPA claim_best_score
    BHI claim_branch_4
    JMP claim_ai_next
claim_branch_4:
claim_take:
    LDAA claim_score
    STAA claim_best_score
    LDAA claim_candidate
    STAA claim_best_cell
    LDAA claim_ai_dir
    STAA claim_best_dir
claim_ai_next:
    INC claim_ai_dir
    LDAA claim_ai_dir
    CMPA #4
    BEQ claim_ai_finish
    JMP claim_ai_loop
claim_ai_finish:
    LDAB claim_best_cell
    CMPB #255
    BEQ claim_ai_done
    LDAA claim_best_dir
    STAA claim_cpu_dir
claim_ai_done:
    RTS
; Bounded flood fill counts reachable empty cells while reserving the player's next cell.
claim_area:
    LDX #claim_seen
    LDAB #112
    CLRA
claim_seen_clear:
    STAA 0,X
    INX
    DECB
    BNE claim_seen_clear
    CLR claim_head
    CLR claim_tail
    LDAB claim_candidate
    CMPB claim_player_next
    BEQ claim_area_done
    JSR claim_enqueue
claim_area_loop:
    LDAA claim_head
    CMPA claim_tail
    BEQ claim_area_done
    ANDA #7
    BNE claim_area_get
    JSR input_poll
claim_area_get:
    LDAB claim_head
    LDX #claim_queue
    ABX
    LDAA 0,X
    STAA claim_area_cell
    INC claim_head
    CLR claim_area_dir
claim_area_neighbors:
    LDAA claim_area_dir
    LDAB claim_area_cell
    JSR claim_neighbor
    CMPB #255
    BEQ claim_area_next
    CMPB claim_player_next
    BEQ claim_area_next
    LDX #board
    ABX
    TST 0,X
    BNE claim_area_next
    LDX #claim_seen
    ABX
    TST 0,X
    BNE claim_area_next
    JSR claim_enqueue
claim_area_next:
    INC claim_area_dir
    LDAA claim_area_dir
    CMPA #4
    BNE claim_area_neighbors
    BRA claim_area_loop
claim_area_done:
    LDAA claim_tail
    RTS
claim_enqueue:
    LDX #claim_seen
    ABX
    LDAA #1
    STAA 0,X
    STAB claim_enqueue_cell
    LDAB claim_tail
    LDX #claim_queue
    ABX
    LDAA claim_enqueue_cell
    STAA 0,X
    INC claim_tail
    RTS
grid_value:
    CMPB cursor
    BNE claim_tile_enemy
    LDAA #4
    RTS
claim_tile_enemy:
    CMPB claim_enemy
    BNE claim_tile_board
    LDAA #5
    RTS
claim_tile_board:
    LDX #board
    ABX
    LDAA 0,X
    RTS
game_render:
    JSR paint_board
    CLR claim_hud_force
    TST resume_pending
    BEQ claim_hud_values
    INC claim_hud_force
    LDX #game_name
    CLRA
    CLRB
    JSR paint_text
    LDAA #168
    STAA paint_x
    CLR paint_band
    LDAA stage
    INCA
    JSR paint_number
    LDX #claim_you_label
    LDAA #138
    LDAB #1
    JSR paint_text
    LDX #claim_cpu_label
    LDAA #138
    LDAB #3
    JSR paint_text
    LDX #claim_map_label
    LDAA #138
    LDAB #5
    JSR paint_text
    LDAA #138
    STAA paint_x
    LDAA #6
    STAA paint_band
    LDAA claim_arena
    INCA
    JSR paint_number
    LDX #claim_menu_label
    LDAA #132
    LDAB #7
    JSR paint_text
claim_hud_values:
    TST claim_hud_force
    BNE claim_you_value
    LDAA claim_wins
    CMPA claim_old_wins
    BEQ claim_cpu_check
claim_you_value:
    LDAA claim_wins
    STAA claim_old_wins
    LDAA #138
    STAA paint_x
    LDAA #2
    STAA paint_band
    LDAA claim_wins
    JSR paint_number
claim_cpu_check:
    TST claim_hud_force
    BNE claim_cpu_value
    LDAA claim_losses
    CMPA claim_old_losses
    BEQ claim_mode_check
claim_cpu_value:
    LDAA claim_losses
    STAA claim_old_losses
    LDAA #138
    STAA paint_x
    LDAA #4
    STAA paint_band
    LDAA claim_losses
    JSR paint_number
claim_mode_check:
    TST claim_hud_force
    BNE claim_mode_value
    LDAA claim_mode
    CMPA claim_old_mode
    BEQ claim_render_done
claim_mode_value:
    LDAA claim_mode
    STAA claim_old_mode
    LDAB #10
    MUL
    ADDD #claim_mode_labels
    XGDX
    LDAA #66
    CLRB
    JMP paint_text
claim_render_done:
    RTS
.section .bss, bss
claim_mode: .space 1
claim_enemy: .space 1
claim_dir: .space 1
claim_queued: .space 1
claim_cpu_dir: .space 1
claim_arena: .space 1
claim_wins: .space 1
claim_losses: .space 1
claim_steps: .space 1
claim_clock: .space 1
claim_pointer: .space 2
claim_index: .space 1
claim_temp_dir: .space 1
claim_neighbor_cell: .space 1
claim_player_next: .space 1
claim_cpu_next: .space 1
claim_player_bad: .space 1
claim_ai_dir: .space 1
claim_score: .space 1
claim_best_score: .space 1
claim_best_cell: .space 1
claim_best_dir: .space 1
claim_candidate: .space 1
claim_local_dir: .space 1
claim_seen: .space 112
claim_queue: .space 112
claim_head: .space 1
claim_tail: .space 1
claim_area_cell: .space 1
claim_area_dir: .space 1
claim_enqueue_cell: .space 1
claim_hud_force: .space 1
claim_old_wins: .space 1
claim_old_losses: .space 1
claim_old_mode: .space 1
.section .data, data
claim_opposite: .byte 1,0,3,2
claim_periods: .byte 18,16,14
claim_you_label: .byte 89,79,85,0
claim_cpu_label: .byte 67,80,85,0
claim_map_label: .byte 77,65,80,0
claim_menu_label: .byte 82,69,84,85,82,78,0
claim_mode_labels:
    .byte 83,80,65,67,69,32,71,79,32,0
    .byte 82,65,67,73,78,71,32,32,32,0
    .byte 89,79,85,32,87,79,78,32,32,0
    .byte 89,79,85,32,76,79,83,84,32,0
    .byte 68,82,65,87,32,32,32,32,32,0
