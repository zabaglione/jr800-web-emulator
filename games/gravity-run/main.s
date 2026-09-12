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
    JSR actor_initial_state
    LDAA #1
    STAA actor_intro_pending
    RTS
actor_initial_state:
    JSR grid_reset
    CLR gravity_steps
    CLR gravity_distance
    CLR gravity_checkpoint
    CLR gravity_score
    CLR gravity_score + 1
    CLR gravity_stars
    CLR gravity_banner
    LDAA #4
    STAA gravity_camera
    LDAA #3
    STAA gravity_lives
gravity_load_course:
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
    TST gravity_banner
    BEQ gravity_banner_ready
    LDAA input_ticks
    SUBA gravity_banner_clock
    CMPA #72
    BCS gravity_banner_ready
    CLR gravity_banner
    LDAA #1
    STAA redraw
gravity_banner_ready:
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
    LDAB stage
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
    CMPA #1
    BNE gravity_rail_done
    LDAA gravity_distance
    ABA
    ANDA #3
    ADDA #6
    CMPB #16
    BCS gravity_rail_done
    ADDA #4
gravity_rail_done:
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
    LDAA phase
    CMPA #4
    BNE gravity_render_board
    JSR gravity_next_stage
gravity_render_board:
    ; Rebuild the source image before applying the camera's pixel offset.
    LDX #tile_cache
    LDAB #112
    LDAA #255
gravity_uncache:
    STAA 0,X
    INX
    DECB
    BNE gravity_uncache
    JSR paint_board
    JSR gravity_camera_draw
    JMP visual_hud
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
gravity_periods: .byte 14,14,13,13,12,12,11,11,10,10,9,9
gravity_dist_label: .byte 68,73,83,84,0
gravity_life_label: .byte 76,73,70,69,0
gravity_star_label: .byte 83,84,65,82,0
gravity_score_label: .byte 83,0
gravity_start_label: .byte 83,80,65,67,69,32,71,79,32,0
gravity_up_label: .byte 71,82,65,86,32,85,80,32,32,0
gravity_down_label: .byte 71,82,65,86,32,68,79,87,78,0

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
    TST actor_intro_step
    BNE actor_intro_wait
    LDX #180
    LDD #20
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

.global gravity_camera
.global gravity_banner
.section .text, code
gravity_next_stage:
    LDAA gravity_row
    STAA gravity_join_row
    LDAA gravity_direction
    STAA gravity_join_dir
    INC stage
    LDAA stage
    CMPA #12
    BCS gravity_next_load
    CLR stage
gravity_next_load:
    CLR gravity_checkpoint
    JSR gravity_load_course
    LDAA gravity_join_row
    STAA gravity_row
    ASLA
    ASLA
    ASLA
    ASLA
    ADDA #3
    STAA cursor
    LDAA gravity_join_dir
    STAA gravity_direction
    LDAA #2
    STAA phase
    LDAA #1
    STAA gravity_running
    STAA gravity_banner
    LDAA input_ticks
    STAA gravity_banner_clock
    RTS
; The small vertical tracking movement leaves all five playable lanes visible.
; The two extra rail margins absorb the crop at either edge of the LCD.
gravity_camera_draw:
    TST actor_intro_pending
    BEQ gravity_camera_active
    RTS
gravity_camera_active:
    TST gravity_running
    BEQ gravity_camera_position
    LDAA #5
    SUBA gravity_row
    ASLA
    CMPA gravity_camera
    BEQ gravity_camera_position
    BCS gravity_camera_up
    INC gravity_camera
    BRA gravity_camera_position
gravity_camera_up:
    DEC gravity_camera
gravity_camera_position:
    LDAA gravity_camera
    SUBA #4
    BNE gravity_camera_nonzero
    RTS
gravity_camera_nonzero:
    STAA gravity_camera_direction
    BPL gravity_camera_count
    NEGA
gravity_camera_count:
    STAA gravity_camera_amount
    LDAA #64
    STAA gravity_camera_x
gravity_camera_column:
    LDAA gravity_camera_amount
    STAA gravity_camera_bits
gravity_camera_shift:
    LDX #framebuffer + 192
    LDAB gravity_camera_x
    ABX
    LDAB #192
    LDAA #6
    STAA gravity_camera_rows
    TST gravity_camera_direction
    BPL gravity_camera_down
    ; ROR uses the next band's low bit, keeping the source row untouched.
gravity_camera_rise:
    LDAA 192,X
    LSRA
    ROR 0,X
    ABX
    DEC gravity_camera_rows
    BNE gravity_camera_rise
    SEC
    ROR 0,X
    BRA gravity_camera_column_done
gravity_camera_down:
    LDAA 0,X
    STAA gravity_camera_prev
    SEC
    ROL 0,X
    ABX
gravity_camera_fall:
    LDAA 0,X
    STAA gravity_camera_temp
    LDAA gravity_camera_prev
    ASLA
    ROL 0,X
    LDAA gravity_camera_temp
    STAA gravity_camera_prev
    ABX
    DEC gravity_camera_rows
    BNE gravity_camera_fall
gravity_camera_column_done:
    DEC gravity_camera_bits
    BNE gravity_camera_shift
    INC gravity_camera_x
    LDAA gravity_camera_x
    CMPA #192
    BNE gravity_camera_column
    LDAA #1
    STAA gravity_camera_rows
gravity_camera_dirty:
    LDAA gravity_camera_rows
    LDAB #64
    JSR dirty_mark
    LDAA gravity_camera_rows
    LDAB #191
    JSR dirty_mark
    INC gravity_camera_rows
    LDAA gravity_camera_rows
    CMPA #8
    BNE gravity_camera_dirty
gravity_camera_done:
    RTS
.section .bss, bss
gravity_join_row: .space 1
gravity_join_dir: .space 1
gravity_banner: .space 1
gravity_banner_clock: .space 1
gravity_camera: .space 1
gravity_camera_direction: .space 1
gravity_camera_amount: .space 1
gravity_camera_x: .space 1
gravity_camera_bits: .space 1
gravity_camera_rows: .space 1
gravity_camera_prev: .space 1
gravity_camera_temp: .space 1
