; SPDX-License-Identifier: MIT
.global heights
.global angle
.global power
.global wind
.global health
.global cpu_health
.global wins
.global cpu_wins
.global terrain
.global arc_mode
.global shot_x
.global shot_y
.global impact_x
.global impact_y
.global shots
.section .text, code
game_start:
    CLR terrain
arc_new_match:
    CLR wins
    CLR cpu_wins
    CLR shots
    CLR arc_help
arc_new_round:
    LDAA #1
    STAA actor_intro_pending
    CLR arc_mode
    LDAA #100
    STAA health
    STAA cpu_health
    LDAA #6
    STAA angle
    STAA power
    JSR random
arc_wind_mod:
    CMPA #7
    BCS arc_wind_ready
    SUBA #7
    BRA arc_wind_mod
arc_wind_ready:
    SUBA #3
    STAA wind
    LDAA terrain
    LDAB #192
    MUL
    ADDD #terrain_data
    STD arc_source
    LDX #heights
    STX arc_dest
    LDAB #192
arc_load_terrain:
    LDX arc_source
    LDAA 0,X
    INX
    STX arc_source
    LDX arc_dest
    STAA 0,X
    INX
    STX arc_dest
    DECB
    BNE arc_load_terrain
    JMP arc_all
arc_all:
    CLR arc_left
    LDAA #191
    STAA arc_right
    LDAA #1
    STAA redraw
    RTS
game_aux:
    CMPA #1
    BEQ arc_next_terrain
    LDAA arc_help
    EORA #1
    STAA arc_help
    JMP arc_all
arc_next_terrain:
    INC terrain
    LDAA terrain
    CMPA #6
    BCS arc_restart
    CLR terrain
arc_restart:
    JMP arc_new_match
game_update:
    TST resume_pending
    BEQ arc_clock_ready
    CLR resume_pending
    LDAA input_ticks
    STAA arc_clock
arc_clock_ready:
    LDAA arc_mode
    BEQ arc_adjust
    CMPA #3
    BNE arc_not_thinking
    JMP cpu_candidate
arc_not_thinking:
    CMPA #4
    BNE arc_flight_tick
    LDAA input_event
    BITA #16
    BEQ arc_idle
    INC terrain
    LDAA terrain
    CMPA #6
    BCS arc_next_round
    CLR terrain
arc_next_round:
    JSR paint_clear
    JMP arc_new_round
arc_flight_tick:
    LDAA input_ticks
    SUBA arc_clock
    CMPA #2
    BCS arc_idle
    LDAA input_ticks
    STAA arc_clock
    JMP arc_flight
arc_adjust:
    LDAA input_event
    BITA #16
    BNE player_fire
    BITA #1
    BEQ arc_angle_down
    LDAA angle
    CMPA #12
    BCC arc_idle
    INC angle
    BRA arc_adjusted
arc_angle_down:
    BITA #2
    BEQ arc_power_down
    TST angle
    BEQ arc_idle
    DEC angle
    BRA arc_adjusted
arc_power_down:
    BITA #4
    BEQ arc_power_up
    LDAA power
    CMPA #2
    BLS arc_idle
    DEC power
    BRA arc_adjusted
arc_power_up:
    BITA #8
    BEQ arc_idle
    LDAA power
    CMPA #9
    BCC arc_idle
    INC power
arc_adjusted:
    LDAA #1
    STAA redraw
    ; No landscape reconstruction for a numeric aiming adjustment.
    LDAA #192
    STAA arc_left
arc_idle:
    RTS
player_fire:
    LDAA #1
    STAA arc_mode
    CLR shooter
    LDAA angle
    STAA sim_angle
    LDAA power
    STAA sim_power
    JSR shot_begin
    JMP shot_redraw
shot_redraw:
    LDAA shot_x
    STAA arc_left
    STAA arc_right
    LDAA #1
    STAA redraw
    RTS
shot_begin:
    INC shots
    CLR shot_stop
    CLR shot_steps
    LDAB sim_angle
    LDX #velocity_cos
    ABX
    LDAA 0,X
    LDAB sim_power
    MUL
    TST shooter
    BEQ shot_vx_ready
    COMA
    COMB
    ADDD #1
shot_vx_ready:
    STD velocity_x
    LDAB sim_angle
    LDX #velocity_sin
    ABX
    LDAA 0,X
    LDAB sim_power
    MUL
    COMA
    COMB
    ADDD #1
    STD velocity_y
    LDAA #16
    TST shooter
    BEQ shot_origin
    LDAA #175
shot_origin:
    CLRB
    STD shot_x
    TAB
    LDX #heights
    ABX
    LDAA 0,X
    SUBA #7
    CLRB
    STD shot_y
    LDAA input_ticks
    STAA arc_clock
    RTS
; Q8.8 trajectory. Horizontal wind acceleration; gravity = 32/256 px/step^2.
simulate_shot:
    INC shot_steps
    LDAA shot_steps
    CMPA #160
    BCC shot_out
    LDD shot_x
    ADDD velocity_x
    STD shot_x
    LDD shot_y
    ADDD velocity_y
    STD shot_y
    LDD velocity_y
    ADDD #32
    STD velocity_y
    LDAB wind
    CLRA
    TSTB
    BPL shot_wind_positive
    DECA
shot_wind_positive:
    ADDD velocity_x
    STD velocity_x
    LDAA shot_x
    BMI shot_left_check
    BRA shot_horizontal
shot_left_check:
    CMPA #192
    BCC shot_out
shot_horizontal:
    CMPA #192
    BCC shot_out
    STAA impact_x
    LDAA shot_y
    BMI shot_running
    CMPA #64
    BCC shot_bottom
    STAA impact_y
    LDAB impact_x
    LDX #heights
    ABX
    CMPA 0,X
    BCS shot_running
    LDAA #1
    STAA shot_stop
shot_running:
    RTS
shot_bottom:
    LDAA #63
    STAA impact_y
    LDAA #1
    STAA shot_stop
    RTS
shot_out:
    LDAA #2
    STAA shot_stop
    RTS
arc_flight:
    LDAA shot_x
    CMPA #192
    BCS arc_old_visible
    CLRA
arc_old_visible:
    STAA arc_left
    STAA arc_right
    JSR simulate_shot
    LDAA shot_x
    CMPA #192
    BCC arc_flight_end_range
    CMPA arc_left
    BCC arc_right_range
    STAA arc_left
arc_right_range:
    CMPA arc_right
    BCS arc_flight_end_range
    STAA arc_right
arc_flight_end_range:
    LDAA #1
    STAA redraw
    TST shot_stop
    BNE arc_landed
    RTS
arc_landed:
    LDAA shot_stop
    CMPA #1
    BNE arc_after_impact
    JSR explosion
arc_after_impact:
    TST health
    BEQ cpu_round_win
    TST cpu_health
    BEQ player_round_win
    TST shooter
    BEQ cpu_begin
    CLR arc_mode
    JSR input_gate
    RTS
player_round_win:
    INC wins
    LDAA wins
    CMPA #2
    BCS arc_round_wait
    LDAA #4
    STAA phase
    BRA arc_round_wait
cpu_round_win:
    INC cpu_wins
    LDAA cpu_wins
    CMPA #2
    BCS arc_round_wait
    LDAA #5
    STAA phase
arc_round_wait:
    LDAA #4
    STAA arc_mode
    JSR input_gate
    JMP arc_all
; CPU evaluates one candidate per update so menus and BREAK stay responsive.
cpu_begin:
    LDAA #3
    STAA arc_mode
    LDAA #1
    STAA shooter
    CLR cpu_angle
    LDAA #2
    STAA cpu_power
    LDAA #255
    STAA best_error
    RTS
cpu_candidate:
    LDAA cpu_angle
    STAA sim_angle
    LDAA cpu_power
    STAA sim_power
    JSR shot_begin
    DEC shots
cpu_sim_loop:
    JSR simulate_shot
    TST shot_stop
    BEQ cpu_sim_loop
    LDAA shot_stop
    CMPA #1
    BNE cpu_next_candidate
    LDAA impact_x
    SUBA #16
    BCC cpu_abs_x
    NEGA
cpu_abs_x:
    STAA arc_distance
    LDAA impact_y
    SUBA heights + 16
    BCC cpu_abs_y
    NEGA
cpu_abs_y:
    ADDA arc_distance
    BCS cpu_next_candidate
    CMPA best_error
    BCC cpu_next_candidate
    STAA best_error
    LDAA cpu_angle
    STAA best_angle
    LDAA cpu_power
    STAA best_power
cpu_next_candidate:
    INC cpu_angle
    LDAA cpu_angle
    CMPA #13
    BCS cpu_candidate_done
    CLR cpu_angle
    INC cpu_power
    LDAA cpu_power
    CMPA #10
    BCS cpu_candidate_done
    LDAA best_angle
    STAA sim_angle
    LDAA best_power
    STAA sim_power
    ; Difficulty 1/2 adds a bounded aiming error; 3 uses the best measured shot.
    LDAA #2
    SUBA stage
    STAA arc_error
    JSR random
    BITA #1
    BEQ cpu_error_low
    LDAA sim_angle
    ADDA arc_error
    CMPA #13
    BCS cpu_error_store
    LDAA #12
    BRA cpu_error_store
cpu_error_low:
    LDAA sim_angle
    SUBA arc_error
    BCC cpu_error_store
    CLRA
cpu_error_store:
    STAA sim_angle
    LDAA #2
    STAA arc_mode
    JSR shot_begin
    JMP shot_redraw
cpu_candidate_done:
    JMP input_poll
explosion:
    LDAA impact_x
    SUBA #20
    BCC blast_left_range
    CLRA
blast_left_range:
    STAA arc_left
    LDAA impact_x
    ADDA #20
    CMPA #192
    BCS blast_right_range
    LDAA #191
blast_right_range:
    STAA arc_right
    ; Blast damage uses both axes; nearby high ledges do not imply a direct hit.
    LDAA #16
    JSR blast_damage
    LDAA health
    SUBA arc_damage
    BCC blast_player_store
    CLRA
blast_player_store:
    STAA health
    LDAA #175
    JSR blast_damage
    LDAA cpu_health
    SUBA arc_damage
    BCC blast_cpu_store
    CLRA
blast_cpu_store:
    STAA cpu_health
    LDAA impact_x
    SUBA #12
    BCC crater_left_ready
    CLRA
crater_left_ready:
    STAA arc_index
crater_loop:
    LDAA arc_index
    SUBA impact_x
    BCC crater_abs
    NEGA
crater_abs:
    CMPA #13
    BCC crater_done
    TAB
    LDX #crater_depth
    ABX
    LDAA 0,X
    ADDA impact_y
    CMPA #63
    BCS crater_depth_ready
    LDAA #62
crater_depth_ready:
    LDAB arc_index
    LDX #heights
    ABX
    CMPA 0,X
    BCS crater_next
    STAA 0,X
crater_next:
    INC arc_index
    LDAA arc_index
    CMPA #192
    BCS crater_loop
crater_done:
    RTS
blast_damage:
    TAB
    SUBA impact_x
    BCC blast_abs_x
    NEGA
blast_abs_x:
    STAA arc_distance
    LDX #heights
    ABX
    LDAA 0,X
    SUBA #3
    SUBA impact_y
    BCC blast_abs_y
    NEGA
blast_abs_y:
    ADDA arc_distance
    BCS blast_none
    CMPA #14
    BCS blast_direct
    CMPA #26
    BCC blast_none
    LDAA #20
    BRA blast_store
blast_direct:
    LDAA #40
    BRA blast_store
blast_none:
    CLRA
blast_store:
    STAA arc_damage
    RTS
game_tile:
    CLRA
    RTS
game_render:
    JSR arc_render_scene
    TST actor_intro_pending
    BEQ actor_render_done
    CLR actor_intro_pending
    JMP actor_intro
actor_render_done:
    RTS
arc_render_scene:
    TST resume_pending
    BEQ arc_render_begin
    CLR arc_left
    LDAA #191
    STAA arc_right
arc_render_begin:
    LDAA arc_left
    STAA render_x
    CMPA #192
    BCS arc_column
    JMP arc_draw_hud
arc_column:
    LDAA #2
    STAA render_band
arc_band:
    LDAA render_band
    ASLA
    ASLA
    ASLA
    STAA render_y
    LDAB render_x
    LDX #heights
    ABX
    LDAA 0,X
    SUBA render_y
    BCS arc_ground_full
    CMPA #8
    BCC arc_ground_empty
    TAB
    LDX #ground_masks
    ABX
    LDAA 0,X
    BRA arc_ground_byte
arc_ground_full:
    LDAA #255
    BRA arc_ground_byte
arc_ground_empty:
    CLRA
arc_ground_byte:
    STAA render_byte
    LDAA render_x
    STAA paint_x
    LDAA render_band
    STAA paint_band
    JSR paint_address
    LDX #render_byte
    STX paint_source
    CLR paint_id
    LDAA #1
    STAA paint_count
    JSR paint_blit
    INC render_band
    LDAA render_band
    CMPA #8
    BNE arc_band
    LDAA render_x
    CMPA #11
    BCS arc_column_cpu
    CMPA #22
    BCC arc_column_cpu
    LDAB heights + 16
    JSR arc_gun_column
arc_column_cpu:
    LDAA render_x
    CMPA #170
    BCS arc_column_next
    CMPA #181
    BCC arc_column_next
    LDAB heights + 175
    JSR arc_gun_column
arc_column_next:
    INC render_x
    LDAA render_x
    ANDA #3
    BNE arc_column_polled
    JSR input_poll
arc_column_polled:
    LDAA render_x
    CMPA arc_right
    BHI arc_columns_done
    JMP arc_column
arc_columns_done:
    LDAA arc_mode
    CMPA #1
    BEQ arc_draw_ball
    CMPA #2
    BNE arc_draw_hud
arc_draw_ball:
    LDAA shot_x
    LDAB shot_y
    JSR arc_pixel
arc_draw_hud:
    JSR visual_hud
arc_render_help:
    TST arc_help
    BEQ arc_render_done
    LDX #arc_help1
    CLRA
    LDAB #2
    JSR paint_text
    LDX #arc_help2
    CLRA
    LDAB #3
    JSR paint_text
arc_render_done:
    LDAA #192
    STAA arc_left
    RTS
; Compact cannon silhouette rebuilt only if its columns intersect the dirty range.
arc_gun_column:
    SUBB #4
    STAB gun_y
    LDAA #4
    STAA gun_count
arc_gun_bits:
    LDAA render_x
    LDAB gun_y
    JSR arc_pixel
    INC gun_y
    DEC gun_count
    BNE arc_gun_bits
    LDAA render_x
    CMPA #16
    BEQ arc_barrel
    CMPA #175
    BNE arc_gun_done
arc_barrel:
    LDAB gun_y
    SUBB #7
    JSR arc_pixel
    LDAA render_x
    LDAB gun_y
    SUBB #6
    JSR arc_pixel
    LDAA render_x
    LDAB gun_y
    SUBB #5
    JMP arc_pixel
arc_gun_done:
    RTS
; A=x, B=y, clipped to the playfield. Compare before writing/marking.
arc_pixel:
    CMPA #192
    BCC arc_pixel_done
    CMPB #16
    BCS arc_pixel_done
    CMPB #64
    BCC arc_pixel_done
    STAA pixel_x
    STAB pixel_y
    TBA
    ANDA #7
    TAB
    LDX #pixel_masks
    ABX
    LDAA 0,X
    STAA pixel_mask
    LDAA pixel_y
    LSRA
    LSRA
    LSRA
    STAA pixel_band
    LDAB #192
    MUL
    ADDD #framebuffer
    ADDB pixel_x
    ADCA #0
    XGDX
    LDAA 0,X
    ORAA pixel_mask
    CMPA 0,X
    BEQ arc_pixel_done
    STAA 0,X
    LDAA pixel_band
    LDAB pixel_x
    JMP dirty_mark
arc_pixel_done:
    RTS
.section .bss, bss
heights: .space 192
angle: .space 1
power: .space 1
wind: .space 1
health: .space 1
cpu_health: .space 1
wins: .space 1
cpu_wins: .space 1
terrain: .space 1
arc_mode: .space 1
shot_x: .space 2
shot_y: .space 2
velocity_x: .space 2
velocity_y: .space 2
sim_angle: .space 1
sim_power: .space 1
shooter: .space 1
shot_steps: .space 1
shot_stop: .space 1
impact_x: .space 1
impact_y: .space 1
shots: .space 1
cpu_angle: .space 1
cpu_power: .space 1
best_angle: .space 1
best_power: .space 1
best_error: .space 1
arc_error: .space 1
arc_help: .space 1
arc_clock: .space 1
arc_source: .space 2
arc_dest: .space 2
arc_left: .space 1
arc_right: .space 1
arc_index: .space 1
arc_distance: .space 1
arc_damage: .space 1
render_x: .space 1
render_y: .space 1
render_band: .space 1
render_byte: .space 1
pixel_x: .space 1
pixel_y: .space 1
pixel_mask: .space 1
pixel_band: .space 1
gun_y: .space 1
gun_count: .space 1
.section .data, data
wind_plus: .byte 43,0
wind_minus: .byte 45,0

; A short, cycle-timed start cue highlights the actor before controls begin.
.global actor_intro_frame
.global actor_intro_step
.global actor_intro_x
.global actor_intro_band
.section .text, code
actor_intro:
    LDAA #11
    STAA actor_intro_x
    LDAA heights + 16
    SUBA #8
    LSRA
    LSRA
    LSRA
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
    LDAA #2
    STAA actor_intro_rows
actor_intro_row:
    LDX paint_dest
    LDAB #11
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
    ADDB #10
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
