; SPDX-License-Identifier: MIT
.global maze_ghosts
.global maze_sleep
.global maze_power
.global maze_lives
.global maze_running
.global maze_direction
.global maze_queued
.global maze_clock
.global maze_ticks
.global maze_score
.global maze_distance
.global maze_world
.global maze_trace
.global maze_contact
.section .text, code
game_start:
    JSR actor_initial_state
    LDAA #1
    STAA actor_intro_pending
    RTS
actor_initial_state:
    JSR grid_reset
    CLR maze_score
    CLR maze_score + 1
    LDAA #3
    STAA maze_lives
    LDAA stage
    LDAB #105
    MUL
    ADDD #maze_levels
    STD maze_pointer
    CLR maze_index
maze_load:
    LDX maze_pointer
    LDAA 0,X
    INX
    STX maze_pointer
    CMPA #2
    BCS maze_load_store
    INC grid_stat
maze_load_store:
    LDAB maze_index
    LDX #board
    ABX
    STAA 0,X
    INC maze_index
    LDAA maze_index
    CMPA #105
    BNE maze_load
    CLR board + 16
    DEC grid_stat
maze_reset_positions:
    LDAA #16
    STAA cursor
    LDAA #88
    STAA maze_ghosts
    LDAA #76
    STAA maze_ghosts + 1
    CLR maze_sleep
    CLR maze_sleep + 1
    CLR maze_ticks
    CLR maze_running
    LDAA #32
    STAA maze_power
    LDAA #3
    STAA maze_direction
    STAA maze_queued
    JSR maze_trace
    RTS
game_aux:
    CMPA #2
    BNE maze_pause
    JMP game_start
maze_pause:
    CLR maze_running
    JMP grid_changed
game_update:
    TST resume_pending
    BEQ maze_clock_ready
    CLR resume_pending
    LDAA input_ticks
    STAA maze_clock
maze_clock_ready:
    LDAA input_event
    BITA #16
    BEQ maze_input
    LDAA #1
    STAA maze_running
    JSR grid_changed
maze_input:
    LDAA input_event
    CLRB
    BITA #1
    BNE maze_queue_turn
    INCB
    BITA #2
    BNE maze_queue_turn
    INCB
    BITA #4
    BNE maze_queue_turn
    INCB
    BITA #8
    BEQ maze_timer
maze_queue_turn:
    STAB maze_queued
maze_timer:
    TST maze_running
    BEQ maze_idle
    LDAA stage
    LSRA
    LSRA
    STAA maze_speed
    LDAA #18
    SUBA maze_speed
    SUBA maze_speed
    STAA maze_speed
    LDAA input_ticks
    SUBA maze_clock
    CMPA maze_speed
    BCS maze_idle
    LDAA input_ticks
    STAA maze_clock
    JSR maze_world
    JMP grid_changed
maze_idle:
    RTS
; A=cell,B=direction, B=neighbour or 255.
maze_neighbor:
    STAA maze_query
    LDAA #105
    MUL
    ADDD #neighbors
    ADDB maze_query
    ADCA #0
    XGDX
    LDAB 0,X
    RTS
maze_world:
    TST maze_power
    BEQ maze_try_move
    DEC maze_power
maze_try_move:
    LDAA cursor
    LDAB maze_queued
    JSR maze_neighbor
    CMPB #255
    BEQ maze_keep_direction
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BEQ maze_keep_direction
    LDAA maze_queued
    STAA maze_direction
    BRA maze_player_move
maze_keep_direction:
    LDAA cursor
    LDAB maze_direction
    JSR maze_neighbor
    CMPB #255
    BEQ maze_after_player
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BEQ maze_after_player
maze_player_move:
    STAB cursor
    LDAA 0,X
    CMPA #2
    BCS maze_player_trace
    DEC grid_stat
    CLR 0,X
    LDAB #10
    CMPA #3
    BNE maze_dot_score
    LDAA #32
    STAA maze_power
    LDAB #20
maze_dot_score:
    JSR maze_add_score
maze_player_trace:
    JSR maze_trace
maze_after_player:
    JSR maze_contact
    TST maze_running
    BNE maze_check_clear
    RTS
maze_check_clear:
    TST grid_stat
    BNE maze_enemy_clock
    JMP maze_clear
maze_enemy_clock:
    INC maze_ticks
    LDAA maze_ticks
    CMPA #3
    BCC maze_enemy_tick
    RTS
maze_enemy_tick:
    CLR maze_ticks
    CLR maze_actor
maze_move_ghost:
    LDAB maze_actor
    LDX #maze_sleep
    ABX
    TST 0,X
    BEQ maze_active_ghost
    DEC 0,X
    BRA maze_ghost_next
maze_active_ghost:
    LDX #maze_ghosts
    ABX
    LDAA 0,X
    STAA maze_origin
    STAA maze_best
    LDAA #255
    TST maze_power
    BEQ maze_best_init
    CLRA
maze_best_init:
    STAA maze_best_distance
    CLR maze_scan_direction
maze_candidate:
    LDAA maze_origin
    LDAB maze_scan_direction
    JSR maze_neighbor
    CMPB #255
    BEQ maze_candidate_next
    STAB maze_option
    LDX #maze_distance
    ABX
    LDAA 0,X
    CMPA #255
    BEQ maze_candidate_next
    TST maze_power
    BNE maze_run_away
    CMPA maze_best_distance
    BCC maze_candidate_next
    BRA maze_choose_step
maze_run_away:
    CMPA maze_best_distance
    BCS maze_candidate_next
maze_choose_step:
    STAA maze_best_distance
    LDAA maze_option
    STAA maze_best
maze_candidate_next:
    INC maze_scan_direction
    LDAA maze_scan_direction
    CMPA #4
    BNE maze_candidate
    LDAB maze_actor
    LDX #maze_ghosts
    ABX
    LDAA maze_best
    STAA 0,X
maze_ghost_next:
    INC maze_actor
    LDAA maze_actor
    CMPA #2
    BNE maze_move_ghost
    JMP maze_contact
maze_clear:
    CLR maze_running
    LDAA #4
    STAA phase
maze_world_done:
    RTS
; Either capture an active frightened ghost or lose one life.
maze_contact:
    CLR maze_actor
maze_contact_loop:
    LDAB maze_actor
    LDX #maze_sleep
    ABX
    TST 0,X
    BNE maze_contact_next
    LDX #maze_ghosts
    ABX
    LDAA 0,X
    CMPA cursor
    BNE maze_contact_next
    TST maze_power
    BEQ maze_hurt
    LDX #maze_sleep
    ABX
    LDAA #2
    STAA 0,X
    LDX #maze_homes
    ABX
    LDAA 0,X
    LDX #maze_ghosts
    ABX
    STAA 0,X
    LDAB #50
    JSR maze_add_score
maze_contact_next:
    INC maze_actor
    LDAA maze_actor
    CMPA #2
    BNE maze_contact_loop
    RTS
maze_hurt:
    DEC maze_lives
    BEQ maze_failed
    JMP maze_reset_positions
maze_failed:
    CLR maze_running
    LDAA #5
    STAA phase
    RTS
maze_add_score:
    CLRA
    ADDD maze_score
    STD maze_score
    RTS
; Breadth-first distance field; walls never change, only the player root does.
maze_trace:
    LDX #maze_distance
    LDAB #105
    LDAA #255
maze_distance_clear:
    STAA 0,X
    INX
    DECB
    BNE maze_distance_clear
    LDAB cursor
    LDX #maze_distance
    ABX
    CLR 0,X
    STAB maze_bfs_queue
    CLR maze_read
    LDAA #1
    STAA maze_write
maze_bfs_next:
    LDAB maze_read
    LDX #maze_bfs_queue
    ABX
    LDAA 0,X
    STAA maze_current
    TAB
    LDX #maze_distance
    ABX
    LDAA 0,X
    INCA
    STAA maze_depth
    CLR maze_bfs_direction
maze_bfs_neighbor:
    LDAA maze_current
    LDAB maze_bfs_direction
    JSR maze_neighbor
    CMPB #255
    BEQ maze_bfs_skip
    STAB maze_option
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BEQ maze_bfs_skip
    LDX #maze_distance
    ABX
    LDAA 0,X
    CMPA #255
    BNE maze_bfs_skip
    LDAA maze_depth
    STAA 0,X
    LDAA maze_option
    LDAB maze_write
    LDX #maze_bfs_queue
    ABX
    STAA 0,X
    INC maze_write
maze_bfs_skip:
    INC maze_bfs_direction
    LDAA maze_bfs_direction
    CMPA #4
    BNE maze_bfs_neighbor
    INC maze_read
    LDAA maze_read
    ANDA #7
    BNE maze_bfs_polled
    JSR input_poll
maze_bfs_polled:
    LDAA maze_read
    CMPA maze_write
    BNE maze_bfs_next
    RTS
grid_value:
    CMPB cursor
    BNE maze_check_ghost_tile
    LDAA #4
    RTS
maze_check_ghost_tile:
    TST maze_sleep
    BNE maze_second_ghost_tile
    CMPB maze_ghosts
    BEQ maze_ghost_tile
maze_second_ghost_tile:
    TST maze_sleep + 1
    BNE maze_floor_tile
    CMPB maze_ghosts + 1
    BEQ maze_ghost_tile
maze_floor_tile:
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BEQ maze_wall_tile
    RTS
maze_wall_tile:
    STAB maze_tile_cell
    CLR maze_tile_mask
    CLR maze_tile_dir
maze_wall_neighbor:
    LDAB maze_tile_dir
    LDAA #105
    MUL
    ADDD #neighbors
    ADDB maze_tile_cell
    ADCA #0
    XGDX
    LDAB 0,X
    CMPB #255
    BEQ maze_wall_next
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BNE maze_wall_next
    LDAB maze_tile_dir
    LDX #maze_tile_bits
    ABX
    LDAA maze_tile_mask
    ORAA 0,X
    STAA maze_tile_mask
maze_wall_next:
    INC maze_tile_dir
    LDAA maze_tile_dir
    CMPA #4
    BNE maze_wall_neighbor
    LDAA maze_tile_mask
    ADDA #7
    RTS
maze_ghost_tile:
    LDAA #5
    TST maze_power
    BEQ maze_tile_done
    INCA
maze_tile_done:
    RTS
game_render:
    JSR actor_render_scene
    TST actor_intro_pending
    BEQ actor_render_done
    CLR actor_intro_pending
    JMP actor_intro
actor_render_done:
    RTS
actor_render_scene:
    JSR paint_board
    JMP visual_hud
.section .bss, bss
maze_ghosts: .space 2
maze_sleep: .space 2
maze_power: .space 1
maze_lives: .space 1
maze_running: .space 1
maze_direction: .space 1
maze_queued: .space 1
maze_ticks: .space 1
maze_score: .space 2
maze_clock: .space 1
maze_speed: .space 1
maze_pointer: .space 2
maze_index: .space 1
maze_query: .space 1
maze_actor: .space 1
maze_origin: .space 1
maze_best: .space 1
maze_best_distance: .space 1
maze_scan_direction: .space 1
maze_option: .space 1
maze_distance: .space 105
maze_bfs_queue: .space 105
maze_read: .space 1
maze_write: .space 1
maze_current: .space 1
maze_depth: .space 1
maze_bfs_direction: .space 1
maze_hud_force: .space 1
maze_old_score: .space 2
maze_old_food: .space 1
maze_old_life: .space 1
maze_old_power: .space 1
maze_old_running: .space 1
.section .data, data
maze_homes: .byte 88,76
maze_food_label: .byte 70,79,79,68,0
maze_life_label: .byte 76,73,70,69,0
maze_power_label: .byte 80,79,87,69,82,0
maze_start_label: .byte 83,80,65,67,69,32,0
maze_score_label: .byte 83,0

; A short, cycle-timed start cue highlights the actor before controls begin.
.global actor_intro_frame
.global actor_intro_step
.global actor_intro_x
.global actor_intro_band
.section .text, code
actor_intro:
    CLRB
    LDX #view_cells
actor_intro_find:
    LDAA 0,X
    CMPA cursor
    BEQ actor_intro_found
    INX
    INCB
    CMPB #112
    BNE actor_intro_find
actor_intro_found:
    TBA
    ANDA #15
    ASLA
    ASLA
    ASLA
    ADDA #VIEW_X
    STAA paint_x
    TBA
    LSRA
    LSRA
    LSRA
    LSRA
    INCA
    STAA paint_band
    LDAA paint_x
    STAA actor_intro_x
    LDAA paint_band
    STAA actor_intro_band
    CLR actor_intro_step
    CLR actor_intro_bytes
    CLR actor_intro_bytes + 1
actor_intro_blink:
    LDAA actor_intro_band
    STAA paint_band
    LDAA actor_intro_x
    STAA paint_x
    JSR paint_address
    LDAA #1
    STAA actor_intro_rows
actor_intro_row:
    LDX paint_dest
    LDAB #8
actor_intro_pixels:
    COM 0,X
    INX
    DECB
    BNE actor_intro_pixels
    LDAA paint_band
    LDAB actor_intro_x
    JSR dirty_mark
    LDAA paint_band
    LDAB actor_intro_x
    ADDB #7
    JSR dirty_mark
    LDD paint_dest
    ADDD #192
    STD paint_dest
    INC paint_band
    DEC actor_intro_rows
    BNE actor_intro_row
    JSR dirty_begin
actor_intro_transfer:
    JSR dirty_next
    BEQ actor_intro_sum
    JSR input_poll
    BRA actor_intro_transfer
actor_intro_sum:
    LDD dirty_bytes
    ADDD actor_intro_bytes
    STD actor_intro_bytes
actor_intro_frame:
    LDAB actor_intro_step
    ASLB
    LDX #actor_start_notes
    ABX
    LDX 0,X
    LDD #48
    JSR sound_tone
actor_intro_wait:
    LDAA input_ticks
    STAA actor_intro_clock
actor_intro_delay:
    JSR input_poll
    LDAA input_ticks
    SUBA actor_intro_clock
    CMPA #8
    BCS actor_intro_delay
    INC actor_intro_step
    LDAA actor_intro_step
    CMPA #4
    BEQ actor_intro_done
    JMP actor_intro_blink
actor_intro_done:
    LDAA #1
    STAA maze_running
    LDAA input_ticks
    STAA maze_clock
    JSR actor_render_scene
    JSR dirty_begin
actor_intro_finish_flush:
    JSR dirty_next
    BEQ actor_intro_finish_sum
    JSR input_poll
    BRA actor_intro_finish_flush
actor_intro_finish_sum:
    LDD dirty_bytes
    ADDD actor_intro_bytes
    STD actor_intro_bytes
    JSR input_gate
    LDAA #1
    STAA resume_pending
    CLR redraw
    LDD actor_intro_bytes
    STD dirty_bytes
    ; Finish the complete shell frame for both stage starts and menu resets.
    LDS #$5FFF
    JMP frame_ready
.section .bss, bss
actor_intro_pending: .space 1
actor_intro_x: .space 1
actor_intro_band: .space 1
actor_intro_step: .space 1
actor_intro_clock: .space 1
actor_intro_rows: .space 1
actor_intro_bytes: .space 2

.section .data, data
actor_start_notes: .word 226,169,139,110

.section .bss, bss
maze_tile_cell: .space 1
maze_tile_mask: .space 1
maze_tile_dir: .space 1
.section .data, data
maze_tile_bits: .byte 1,2,4,8
