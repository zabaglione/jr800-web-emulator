; SPDX-License-Identifier: MIT
.global river_lanes
.global river_periods
.global river_timers
.global river_homes
.global river_running
.global river_lives
.global river_time
.global river_clock
.global river_score
.global river_world
.global river_check
.global river_hop
.section .text, code
game_start:
    JSR actor_initial_state
    LDAA #1
    STAA actor_intro_pending
    RTS
actor_initial_state:
    JSR grid_reset
    CLR river_score
    CLR river_score + 1
    CLR river_display_score
    CLR river_display_score + 1
    CLR river_cycle
    CLR river_steps
    CLR river_effect
    CLR river_banner
    LDAA #5
    STAA river_lives
river_stage_load:
    CLR grid_stat
    LDX #river_homes
    LDAB #5
river_home_clear:
    CLR 0,X
    INX
    DECB
    BNE river_home_clear
    LDAA stage
    LDAB #60
    MUL
    ADDD #river_levels
    STD river_pointer
    CLR river_index
river_load:
    LDX river_pointer
    LDAA 0,X
    INX
    STX river_pointer
    LDAB river_index
    LDX #river_lanes
    ABX
    STAA 0,X
    INC river_index
    LDAA river_index
    CMPA #60
    BNE river_load
    LDD river_periods
    STD river_timers
    LDD river_periods + 2
    STD river_timers + 2
    ; Each stage shortens the environmental tick. Successive circuits keep
    ; that difficulty instead of returning to the easiest tempo.
    LDAA #18
    SUBA stage
    SUBA river_cycle
    CMPA #6
    BCC river_speed_ready
    LDAA #6
river_speed_ready:
    STAA river_tick_period
river_spawn:
    LDAA #90
    STAA cursor
    LDAA #100
    STAA river_time
    CLR river_running
    RTS
game_aux:
    CMPA #2
    BNE river_pause
    JMP game_start
river_pause:
    CLR river_running
    JMP grid_changed
game_update:
    TST resume_pending
    BEQ river_clock_ready
    CLR resume_pending
    LDAA input_ticks
    STAA river_clock
river_clock_ready:
    LDAA input_event
    BITA #16
    BEQ river_controls
    TST river_running
    BNE river_controls
    LDAA #1
    STAA river_running
    LDAA input_ticks
    STAA river_clock
    JSR grid_changed
river_controls:
    TST river_running
    BEQ river_idle
    JSR grid_move
    CMPB #255
    BEQ river_timer
    JSR river_hop
    JSR grid_changed
river_timer:
    TST river_running
    BEQ river_idle
    LDAA input_ticks
    SUBA river_clock
    CMPA river_tick_period
    BCS river_idle
    LDAA input_ticks
    STAA river_clock
    JSR river_world
    JMP grid_changed
river_idle:
    RTS
; B is the destination cell. A hop is checked before the traffic advances.
river_hop:
    STAB cursor
    JMP river_check
river_world:
    INC river_steps
    TST river_banner
    BEQ river_banner_done
    DEC river_banner
river_banner_done:
    DEC river_time
    BNE river_time_left
    JMP river_died
river_time_left:
    CLR river_lane
river_lane_tick:
    LDAB river_lane
    LDX #river_timers
    ABX
    DEC 0,X
    BNE river_lane_next
    LDX #river_periods
    ABX
    LDAA 0,X
    LDX #river_timers
    ABX
    STAA 0,X
    LDX #river_directions
    ABX
    LDAA 0,X
    STAA river_direction
    LDX #river_rows
    ABX
    LDAA 0,X
    LDAB #14
    MUL
    STAB river_row_start
    LDAA river_lane
    LDAB #14
    MUL
    ADDD #river_lanes
    STD river_base
    XGDX
    LDAA river_direction
    BMI river_shift_left
    LDAA 13,X
    STAA river_edge
    LDAB #13
    ABX
river_right_loop:
    DEX
    LDAA 0,X
    STAA 1,X
    DECB
    BNE river_right_loop
    LDAA river_edge
    STAA 0,X
    BRA river_carry
river_shift_left:
    LDAA 0,X
    STAA river_edge
    LDAB #13
river_left_loop:
    LDAA 1,X
    STAA 0,X
    INX
    DECB
    BNE river_left_loop
    LDAA river_edge
    STAA 0,X
river_carry:
    LDAA river_lane
    CMPA #2
    BCC river_lane_next
    LDAA cursor
    SUBA river_row_start
    BCS river_lane_next
    CMPA #14
    BCC river_lane_next
    ADDA river_direction
    CMPA #14
    BCC river_died
    ADDA river_row_start
    STAA cursor
river_lane_next:
    INC river_lane
    LDAA river_lane
    CMPA #4
    BEQ river_lanes_done
    JMP river_lane_tick
river_lanes_done:
    JMP river_check
river_died:
    LDAA cursor
    STAA river_effect_cell
    LDAA #1
    STAA river_effect
    DEC river_lives
    BEQ river_failed
    JMP river_spawn
river_failed:
    CLR river_running
    LDAA #5
    STAA phase
    RTS
river_check:
    LDAA cursor
    CMPA #14
    BCS river_home_check
    CMPA #42
    BCS river_water_check
    CMPA #56
    BCS river_safe
    CMPA #84
    BCC river_safe
    SUBA #28
    TAB
    LDX #river_lanes
    ABX
    TST 0,X
    BNE river_died
river_safe:
    RTS
river_water_check:
    SUBA #14
    TAB
    LDX #river_lanes
    ABX
    TST 0,X
    BEQ river_died
    RTS
river_home_check:
    CLR river_index
river_find_home:
    LDAB river_index
    LDX #river_home_columns
    ABX
    CMPA 0,X
    BEQ river_home_found
    INC river_index
    LDAB river_index
    CMPB #5
    BNE river_find_home
    BRA river_died
river_home_found:
    LDX #river_homes
    ABX
    TST 0,X
    BNE river_died
    LDAA #1
    STAA 0,X
    INC grid_stat
    LDAA cursor
    STAA river_effect_cell
    LDAA #2
    STAA river_effect
    CLRA
    LDAB river_time
    ADDD #100
    ADDD river_score
    BCC river_score_safe
    LDD #65535
river_score_safe:
    STD river_score
    LDAA grid_stat
    CMPA #5
    BCC river_clear
    JMP river_spawn
river_clear:
    CLR river_running
    LDAA #4
    STAA phase
    RTS
grid_value:
    CMPB cursor
    BNE river_background
    LDAA #8
    RTS
river_background:
    CMPB #14
    BCS river_home_tile
    CMPB #42
    BCS river_water_tile
    CMPB #56
    BCS river_grass_tile
    CMPB #84
    BCC river_grass_tile
    SUBB #28
    LDX #river_lanes
    ABX
    TST 0,X
    BEQ river_road_tile
    LDAA #7
    RTS
river_road_tile:
    LDAA #3
    RTS
river_water_tile:
    SUBB #14
    LDX #river_lanes
    ABX
    TST 0,X
    BEQ river_open_water
    LDAA #6
    RTS
river_open_water:
    LDAA #2
    RTS
river_grass_tile:
    LDAA #1
    RTS
river_home_tile:
    STAB river_tile_cell
    CLR river_tile_index
river_home_tile_loop:
    LDAB river_tile_index
    LDX #river_home_columns
    ABX
    LDAA 0,X
    CMPA river_tile_cell
    BEQ river_home_tile_found
    INC river_tile_index
    LDAA river_tile_index
    CMPA #5
    BNE river_home_tile_loop
    BRA river_open_water
river_home_tile_found:
    LDX #river_homes
    ABX
    LDAA 0,X
    ADDA #4
    RTS
game_render:
    JSR actor_render_scene
    TST actor_intro_pending
    BEQ actor_render_done
    CLR actor_intro_pending
    JMP actor_intro
actor_render_done:
    TST river_effect
    BEQ river_render_live
    JMP river_present
river_render_live:
    RTS
actor_render_scene:
    JSR paint_board
    JMP visual_hud
.section .bss, bss
river_lanes: .space 56
river_periods: .space 4
river_timers: .space 4
river_homes: .space 5
river_running: .space 1
river_lives: .space 1
river_time: .space 1
river_clock: .space 1
river_score: .space 2
river_pointer: .space 2
river_index: .space 1
river_lane: .space 1
river_direction: .space 1
river_row_start: .space 1
river_base: .space 2
river_edge: .space 1
river_tile_cell: .space 1
river_tile_index: .space 1
river_hud_force: .space 1
river_old_score: .space 2
river_old_time: .space 1
river_old_home: .space 1
river_old_life: .space 1
river_old_running: .space 1
.section .data, data
river_score_label: .byte 83,0
river_time_label: .byte 84,73,77,69,0
river_home_label: .byte 72,79,77,69,0
river_life_label: .byte 76,73,70,69,0
river_start_label: .byte 83,80,65,67,69,32,0

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
    TST river_effect
    BEQ river_intro_note
    LDX #river_hit_notes
river_intro_note:
    ABX
    LDX 0,X
    LDD #40
    JSR sound_tone
actor_intro_wait:
    LDAA input_ticks
    STAA actor_intro_clock
actor_intro_delay:
    JSR input_poll
    LDAA input_ticks
    SUBA actor_intro_clock
    LDAB #8
    TST river_effect
    BEQ river_intro_wait_ticks
    LDAB #4
river_intro_wait_ticks:
    STAB river_wait_ticks
    CMPA river_wait_ticks
    BCS actor_intro_delay
    INC actor_intro_step
    LDAA actor_intro_step
    CMPA #4
    BEQ actor_intro_done
    JMP actor_intro_blink
actor_intro_done:
    TST river_effect
    BEQ river_start_done
    JMP river_effect_done
river_start_done:
    LDAA #1
    STAA river_running
    LDAA input_ticks
    STAA river_clock
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
actor_start_notes: .word 286,254,189,143

; Short effects own their intermediate LCD transfers; the traffic stays frozen
; during the cue and resumes automatically from a fresh emulated-clock sample.
.global river_effect
.global river_score_frame
.global river_display_score
.global river_tick_period
.global river_cycle
.global river_steps
.section .text, code
river_present:
    LDAA cursor
    STAA river_resume_cell
    LDAA river_effect_cell
    STAA cursor
    JSR actor_render_scene
    LDAA river_effect
    CMPA #1
    BNE river_score_begin
    JMP actor_intro
river_score_begin:
    CLR actor_intro_bytes
    CLR actor_intro_bytes + 1
    CLR actor_intro_step
river_score_next:
    LDD river_display_score
    ADDD #50
    BCC river_candidate_safe
    LDD river_score
river_candidate_safe:
    STD river_score_candidate
    SUBD river_score
    BCS river_score_candidate_ok
    LDD river_score
    BRA river_score_store
river_score_candidate_ok:
    LDD river_score_candidate
    BRA river_score_store
    LDD river_score
river_score_store:
    STD river_display_score
    JSR visual_hud
    JSR river_effect_flush
river_score_frame:
    LDAA input_ticks
    STAA actor_intro_clock
    LDAB actor_intro_step
    ASLB
    LDX #river_score_notes
    ABX
    LDX 0,X
    LDD #20
    JSR sound_tone
river_score_wait:
    JSR input_poll
    LDAA input_ticks
    SUBA actor_intro_clock
    CMPA #4
    BCS river_score_wait
    INC actor_intro_step
    LDD river_display_score
    SUBD river_score
    BNE river_score_next
river_effect_done:
    LDAA river_resume_cell
    STAA cursor
    LDAA phase
    CMPA #5
    BNE river_effect_continue
    JSR actor_render_scene
    JSR lose_game
    JSR river_effect_flush
    CLR actor_intro_step
river_over_next:
    LDAB actor_intro_step
    ASLB
    LDX #river_over_notes
    ABX
    LDX 0,X
    LDD #48
    JSR sound_tone
    JSR input_poll
    INC actor_intro_step
    LDAA actor_intro_step
    CMPA #5
    BNE river_over_next
    BRA river_effect_finish
river_effect_continue:
    CMPA #4
    BNE river_effect_resume
    INC stage
    LDAA stage
    CMPA #12
    BCS river_next_stage
    CLR stage
    LDAA river_cycle
    CMPA #12
    BCC river_next_stage
    LDAA #12
    STAA river_cycle
river_next_stage:
    JSR river_stage_load
    LDAA #8
    STAA river_banner
    LDAA #2
    STAA phase
river_effect_resume:
    LDAA #1
    STAA river_running
    JSR actor_render_scene
    JSR river_effect_flush
river_effect_finish:
    CLR river_effect
    JSR input_gate
    JSR input_poll
    LDAA input_ticks
    STAA river_clock
    LDAA #1
    STAA resume_pending
    CLR redraw
    LDD actor_intro_bytes
    STD dirty_bytes
    LDS #$5FFF
    JMP frame_ready
river_effect_flush:
    JSR dirty_begin
river_effect_transfer:
    JSR dirty_next
    BEQ river_effect_sum
    JSR input_poll
    BRA river_effect_transfer
river_effect_sum:
    LDD dirty_bytes
    ADDD actor_intro_bytes
    STD actor_intro_bytes
    RTS
.section .data, data
river_hit_notes: .word 339,381,452,508
river_score_notes: .word 226,189,169,139
river_over_notes: .word 226,286,339,381,452
.section .bss, bss
river_effect: .space 1
river_effect_cell: .space 1
river_resume_cell: .space 1
river_display_score: .space 2
river_tick_period: .space 1
river_cycle: .space 1
river_steps: .space 1
river_banner: .space 1
river_wait_ticks: .space 1
river_score_candidate: .space 2
