; SPDX-License-Identifier: MIT
.global gravity_course
.global gravity_collected
.global gravity_distance
.global gravity_row
.global gravity_direction
.global gravity_running
.global gravity_lives
.global gravity_checkpoint
.global gravity_score
.global gravity_stars
.global gravity_steps
.global gravity_world
.section .text, code
game_start:
    JSR grid_reset
    CLR gravity_steps
    CLR gravity_distance
    CLR gravity_checkpoint
    CLR gravity_score
    CLR gravity_score + 1
    CLR gravity_stars
    LDAA #3
    STAA gravity_lives
    LDAA stage
    LDAB #80
    MUL
    ADDD #gravity_levels
    STD gravity_pointer
    CLR gravity_index
gravity_load:
    LDX gravity_pointer
    LDAA 0,X
    INX
    STX gravity_pointer
    LDAB gravity_index
    LDX #gravity_course
    ABX
    STAA 0,X
    LDX #gravity_collected
    ABX
    CLR 0,X
    INC gravity_index
    LDAA gravity_index
    CMPA #80
    BNE gravity_load
    LDX #board
    LDAA #1
    LDAB #16
gravity_walls:
    STAA 0,X
    STAA 96,X
    INX
    DECB
    BNE gravity_walls
    JSR gravity_ready
    JMP gravity_view
gravity_ready:
    LDAA gravity_checkpoint
    STAA gravity_distance
    LDAA #5
    STAA gravity_row
    LDAA #83
    STAA cursor
    LDAA #1
    STAA gravity_direction
    CLR gravity_running
    LDAA input_ticks
    STAA gravity_clock
    RTS
game_aux:
    CMPA #2
    BNE gravity_pause
    JMP game_start
gravity_pause:
    CLR gravity_running
    RTS
game_update:
    TST resume_pending
    BEQ gravity_input
    CLR resume_pending
    LDAA input_ticks
    STAA gravity_clock
gravity_input:
    LDAA input_event
    BITA #1
    BEQ gravity_down
    LDAA #255
    STAA gravity_direction
    BRA gravity_direction_changed
gravity_down:
    BITA #2
    BEQ gravity_space
    LDAA #1
    STAA gravity_direction
gravity_direction_changed:
    LDAA #1
    STAA redraw
gravity_space:
    LDAA input_event
    BITA #16
    BEQ gravity_timer
    TST gravity_running
    BEQ gravity_begin
    NEG gravity_direction
    BRA gravity_changed
gravity_begin:
    LDAA #1
    STAA gravity_running
    LDAA input_ticks
    STAA gravity_clock
gravity_changed:
    LDAA #1
    STAA redraw
gravity_timer:
    TST gravity_running
    BEQ gravity_update_done
    LDAA stage
    LSRA
    LSRA
    TAB
    LDX #gravity_periods
    ABX
    LDAA input_ticks
    SUBA gravity_clock
    CMPA 0,X
    BCS gravity_update_done
    LDAA input_ticks
    STAA gravity_clock
    JSR gravity_world
    LDAA #1
    STAA redraw
gravity_update_done:
    RTS
gravity_world:
    INC gravity_steps
    INC gravity_distance
    LDAA gravity_row
    ADDA gravity_direction
    CMPA #1
    BCC gravity_lower_bound
    LDAA #1
gravity_lower_bound:
    CMPA #6
    BCS gravity_store_row
    LDAA #5
gravity_store_row:
    STAA gravity_row
    ASLA
    ASLA
    ASLA
    ASLA
    ADDA #3
    STAA cursor
    LDAA gravity_distance
    ADDA #3
    STAA gravity_world_col
    TAB
    LDX #gravity_course
    ABX
    LDAA 0,X
    STAA gravity_code
    LDAB gravity_row
    DECB
    LDX #gravity_masks
    ABX
    ANDA 0,X
    BEQ gravity_safe
    DEC gravity_lives
    BNE gravity_respawn
    LDAA #5
    STAA phase
    JMP gravity_view
gravity_respawn:
    JSR input_gate
    JSR gravity_ready
    JMP gravity_view
gravity_safe:
    LDAA gravity_code
    LSRA
    LSRA
    LSRA
    LSRA
    LSRA
    CMPA gravity_row
    BNE gravity_progress
    LDAB gravity_world_col
    LDX #gravity_collected
    ABX
    TST 0,X
    BNE gravity_progress
    INC 0,X
    INC gravity_stars
    LDD gravity_score
    ADDD #10
    STD gravity_score
gravity_progress:
    LDAA gravity_distance
    CMPA #32
    BCS gravity_finish_check
    LDAA #32
    STAA gravity_checkpoint
gravity_finish_check:
    LDAA gravity_distance
    CMPA #64
    BNE gravity_view
    LDD gravity_score
    ADDD #100
    STD gravity_score
    LDAA #4
    STAA phase
gravity_view:
    CLR gravity_view_col
gravity_view_column:
    LDAA gravity_distance
    ADDA gravity_view_col
    TAB
    LDX #gravity_course
    ABX
    LDAA 0,X
    STAA gravity_view_bits
    LSRA
    LSRA
    LSRA
    LSRA
    LSRA
    STAA gravity_view_star
    LDX #gravity_collected
    ABX
    TST 0,X
    BEQ gravity_view_star_ready
    CLR gravity_view_star
gravity_view_star_ready:
    LDAA #1
    STAA gravity_view_row
    LDAA gravity_view_col
    ADDA #16
    STAA gravity_view_cell
gravity_view_row_loop:
    CLR gravity_view_tile
    LSR gravity_view_bits
    BCC gravity_view_not_spike
    LDAA #2
    STAA gravity_view_tile
    BRA gravity_view_store
gravity_view_not_spike:
    LDAA gravity_view_star
    CMPA gravity_view_row
    BNE gravity_view_store
    LDAA #3
    STAA gravity_view_tile
gravity_view_store:
    LDAB gravity_view_cell
    LDX #board
    ABX
    LDAA gravity_view_tile
    STAA 0,X
    LDAA gravity_view_cell
    ADDA #16
    STAA gravity_view_cell
    INC gravity_view_row
    LDAA gravity_view_row
    CMPA #6
    BNE gravity_view_row_loop
    INC gravity_view_col
    LDAA gravity_view_col
    CMPA #16
    BNE gravity_view_column
    RTS
grid_value:
    CMPB cursor
    BNE gravity_tile_board
    LDAA #4
    TST gravity_direction
    BPL gravity_tile_done
    INCA
gravity_tile_done:
    RTS
gravity_tile_board:
    LDX #board
    ABX
    LDAA 0,X
    RTS
game_render:
    JSR paint_board
    CLR gravity_hud_force
    TST resume_pending
    BEQ gravity_hud_values
    INC gravity_hud_force
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
    LDX #gravity_dist_label
    LDAA #138
    LDAB #1
    JSR paint_text
    LDX #gravity_life_label
    LDAA #138
    LDAB #3
    JSR paint_text
    LDX #gravity_star_label
    LDAA #138
    LDAB #5
    JSR paint_text
    LDX #gravity_score_label
    LDAA #132
    LDAB #7
    JSR paint_text
gravity_hud_values:
    TST gravity_hud_force
    BNE gravity_distance_value
    LDAA gravity_distance
    CMPA gravity_old_distance
    BEQ gravity_life_check
gravity_distance_value:
    LDAA gravity_distance
    STAA gravity_old_distance
    LDAA #138
    STAA paint_x
    LDAA #2
    STAA paint_band
    LDAA gravity_distance
    JSR paint_number
gravity_life_check:
    TST gravity_hud_force
    BNE gravity_life_value
    LDAA gravity_lives
    CMPA gravity_old_lives
    BEQ gravity_star_check
gravity_life_value:
    LDAA gravity_lives
    STAA gravity_old_lives
    LDAA #138
    STAA paint_x
    LDAA #4
    STAA paint_band
    LDAA gravity_lives
    JSR paint_number
gravity_star_check:
    TST gravity_hud_force
    BNE gravity_star_value
    LDAA gravity_stars
    CMPA gravity_old_stars
    BEQ gravity_score_check
gravity_star_value:
    LDAA gravity_stars
    STAA gravity_old_stars
    LDAA #138
    STAA paint_x
    LDAA #6
    STAA paint_band
    LDAA gravity_stars
    JSR paint_number
gravity_score_check:
    TST gravity_hud_force
    BNE gravity_score_value
    LDD gravity_score
    SUBD gravity_old_score
    BEQ gravity_mode_check
gravity_score_value:
    LDD gravity_score
    STD gravity_old_score
    LDAA #138
    STAA paint_x
    LDAA #7
    STAA paint_band
    LDD gravity_score
    JSR paint_number16
gravity_mode_check:
    TST gravity_hud_force
    BNE gravity_mode_value
    LDAA gravity_running
    CMPA gravity_old_running
    BNE gravity_mode_value
    LDAA gravity_direction
    CMPA gravity_old_direction
    BEQ gravity_render_done
gravity_mode_value:
    LDAA gravity_running
    STAA gravity_old_running
    LDAA gravity_direction
    STAA gravity_old_direction
    LDX #gravity_up_label
    TSTA
    BMI gravity_mode_text
    LDX #gravity_down_label
gravity_mode_text:
    TST gravity_running
    BNE gravity_mode_paint
    LDX #gravity_start_label
gravity_mode_paint:
    LDAA #78
    CLRB
    JMP paint_text
gravity_render_done:
    RTS
.section .bss, bss
gravity_course: .space 80
gravity_collected: .space 80
gravity_distance: .space 1
gravity_row: .space 1
gravity_direction: .space 1
gravity_running: .space 1
gravity_lives: .space 1
gravity_checkpoint: .space 1
gravity_score: .space 2
gravity_stars: .space 1
gravity_steps: .space 1
gravity_clock: .space 1
gravity_pointer: .space 2
gravity_index: .space 1
gravity_world_col: .space 1
gravity_code: .space 1
gravity_view_col: .space 1
gravity_view_bits: .space 1
gravity_view_star: .space 1
gravity_view_row: .space 1
gravity_view_cell: .space 1
gravity_view_tile: .space 1
gravity_hud_force: .space 1
gravity_old_distance: .space 1
gravity_old_lives: .space 1
gravity_old_stars: .space 1
gravity_old_score: .space 2
gravity_old_running: .space 1
gravity_old_direction: .space 1
.section .data, data
gravity_masks: .byte 1,2,4,8,16
gravity_periods: .byte 14,12,10
gravity_dist_label: .byte 68,73,83,84,0
gravity_life_label: .byte 76,73,70,69,0
gravity_star_label: .byte 83,84,65,82,0
gravity_score_label: .byte 83,0
gravity_start_label: .byte 83,80,65,67,69,32,71,79,32,0
gravity_up_label: .byte 71,82,65,86,32,85,80,32,32,0
gravity_down_label: .byte 71,82,65,86,32,68,79,87,78,0
