; SPDX-License-Identifier: MIT
.global orbit_enemies
.global orbit_wave
.global orbit_position
.global orbit_total
.global orbit_left
.global orbit_core
.global orbit_pulse
.global orbit_aim
.global orbit_running
.global orbit_flash
.global orbit_score
.global orbit_steps
.global orbit_turn
.global orbit_world
.global orbit_shoot
.global orbit_burst
.global orbit_weapon_clock
.section .text, code
game_start:
    JSR actor_initial_state
    LDAA #1
    STAA actor_intro_pending
    RTS
actor_initial_state:
    CLR orbit_motion
    CLR orbit_sound
    JSR grid_reset
    CLR orbit_position
    CLR orbit_aim
    CLR orbit_running
    CLR orbit_flash
    CLR orbit_score
    CLR orbit_score + 1
    CLR orbit_steps
    CLR orbit_turn
    LDAA #55
    STAA cursor
    LDAA #5
    STAA orbit_core
    LDAA #1
    STAA orbit_pulse
    LDAA stage
    ADDA #12
    STAA orbit_total
    STAA orbit_left
    LDAA stage
    LDAB #24
    MUL
    ADDD #orbit_levels
    STD orbit_pointer
    CLR orbit_index
orbit_load_wave:
    LDX orbit_pointer
    LDAA 0,X
    INX
    STX orbit_pointer
    LDAB orbit_index
    LDX #orbit_wave
    ABX
    STAA 0,X
    LDX #orbit_enemies
    ABX
    CLR 0,X
    LDX #orbit_drawn
    ABX
    CLR 0,X
    INC orbit_index
    LDAA orbit_index
    CMPA #24
    BNE orbit_load_wave
    CLR orbit_index
orbit_load_background:
    LDAB orbit_index
    LDX #orbit_background
    ABX
    LDAA 0,X
    LDX #board
    ABX
    STAA 0,X
    INC orbit_index
    LDAA orbit_index
    CMPA #112
    BNE orbit_load_background
    LDAA input_ticks
    STAA orbit_clock
    SUBA #4
    STAA orbit_weapon_clock
    RTS
game_aux:
    CMPA #2
    BNE orbit_burst
    JMP game_start
orbit_burst:
    TST orbit_pulse
    BEQ orbit_burst_done
    CLR orbit_pulse
    LDAA #5
    STAA orbit_sound
    CLR orbit_index
orbit_burst_loop:
    LDAB orbit_index
    LDX #orbit_enemies
    ABX
    LDAA 0,X
    BEQ orbit_burst_next
    CLR 0,X
    LDAB #10
    MUL
    ADDD orbit_score
    STD orbit_score
    DEC orbit_left
orbit_burst_next:
    INC orbit_index
    LDAA orbit_index
    CMPA #24
    BNE orbit_burst_loop
    JMP orbit_check_end
orbit_burst_done:
    RTS
game_update:
    TST resume_pending
    BEQ orbit_input
    CLR resume_pending
    LDAA input_ticks
    STAA orbit_clock
orbit_input:
    LDAA input_event
    BITA #1
    BEQ orbit_down
    CLR orbit_aim
    BRA orbit_turned
orbit_down:
    BITA #2
    BEQ orbit_left_input
    LDAA #4
    STAA orbit_aim
    BRA orbit_turned
orbit_left_input:
    BITA #4
    BEQ orbit_right_input
    LDAA orbit_aim
    DECA
    ANDA #7
    STAA orbit_aim
    BRA orbit_turned
orbit_right_input:
    BITA #8
    BEQ orbit_fire_input
    INC orbit_aim
    LDAA orbit_aim
    ANDA #7
    STAA orbit_aim
orbit_turned:
    LDAA #1
    STAA redraw
orbit_fire_input:
    LDAA input_event
    BITA #16
    BEQ orbit_timer
    TST orbit_running
    BNE orbit_weapon_ready
    LDAA #1
    STAA orbit_running
    STAA redraw
    LDAA input_ticks
    STAA orbit_clock
orbit_weapon_ready:
    LDAA input_ticks
    SUBA orbit_weapon_clock
    CMPA #4
    BCS orbit_timer
    LDAA input_ticks
    STAA orbit_weapon_clock
    STAA orbit_beam_clock
    LDAA #2
    STAA orbit_flash
    LDAA #1
    STAA redraw
    LDAA #2
    STAA orbit_sound
    JSR orbit_shoot
orbit_timer:
    LDAA phase
    CMPA #2
    BNE orbit_update_done
    TST orbit_running
    BEQ orbit_update_done
    LDAA stage
    LSRA
    LSRA
    TAB
    LDX #orbit_motion_periods
    ABX
    LDAA input_ticks
    SUBA orbit_clock
    CMPA 0,X
    BCS orbit_update_done
    LDAA input_ticks
    STAA orbit_clock
    INC orbit_motion
    LDAA orbit_motion
    CMPA #16
    BNE orbit_motion_finish
    JSR orbit_world
    LDAA orbit_position
    CMPA orbit_total
    BCC orbit_motion_redraw
    LDAA #1
    STAA orbit_sound
    BRA orbit_motion_redraw
orbit_motion_finish:
    CMPA #32
    BNE orbit_motion_redraw
    CLR orbit_motion
    JSR orbit_world
orbit_motion_redraw:
    LDAA #1
    STAA redraw
orbit_update_done:
    RTS
orbit_shoot:
    LDAB orbit_aim
orbit_shoot_ring:
    LDX #orbit_enemies
    ABX
    TST 0,X
    BNE orbit_hit
    ADDB #8
    CMPB #24
    BCS orbit_shoot_ring
    ; In the latter half of a spiral step, also accept the sector the
    ; visible enemy is approaching. Inner enemies move straight to the core.
    LDAA stage
    CMPA #8
    BCS orbit_shoot_done
    LDAA orbit_motion
    CMPA #16
    BCS orbit_shoot_done
    LDAB orbit_aim
    DECB
    ANDB #7
    ADDB #8
orbit_visible_ring:
    LDX #orbit_enemies
    ABX
    TST 0,X
    BNE orbit_hit
    ADDB #8
    CMPB #24
    BCS orbit_visible_ring
orbit_shoot_done:
    RTS
orbit_hit:
    DEC 0,X
    BNE orbit_hit_score
    DEC orbit_left
orbit_hit_score:
    LDAA #3
    STAA orbit_sound
    LDD orbit_score
    ADDD #10
    STD orbit_score
    JMP orbit_check_end
orbit_world:
    INC orbit_steps
    TST orbit_flash
    BEQ orbit_world_tick
    DEC orbit_flash
orbit_world_tick:
    LDAA orbit_turn
    EORA #1
    STAA orbit_turn
    BEQ orbit_advance
    RTS
orbit_advance:
    CLR orbit_index
orbit_leak_loop:
    LDAB orbit_index
    LDX #orbit_enemies
    ABX
    TST 0,X
    BEQ orbit_leak_next
    DEC orbit_left
    TST orbit_core
    BEQ orbit_leak_next
    DEC orbit_core
    LDAA #4
    STAA orbit_sound
orbit_leak_next:
    INC orbit_index
    LDAA orbit_index
    CMPA #8
    BNE orbit_leak_loop
    LDX #orbit_next
    LDAB #24
    CLRA
orbit_next_clear:
    STAA 0,X
    INX
    DECB
    BNE orbit_next_clear
    LDAA #8
    STAA orbit_index
orbit_shift_loop:
    LDAB orbit_index
    LDX #orbit_enemies
    ABX
    LDAA 0,X
    STAA orbit_value
    LDAA orbit_index
    SUBA #8
    STAA orbit_target
    LDAA stage
    CMPA #8
    BCS orbit_shift_store
    LDAA orbit_target
    ANDA #248
    STAA orbit_target_ring
    LDAA orbit_target
    INCA
    ANDA #7
    ADDA orbit_target_ring
    STAA orbit_target
orbit_shift_store:
    LDAB orbit_target
    LDX #orbit_next
    ABX
    LDAA orbit_value
    STAA 0,X
    INC orbit_index
    LDAA orbit_index
    CMPA #24
    BNE orbit_shift_loop
    CLR orbit_index
orbit_copy_next:
    LDAB orbit_index
    LDX #orbit_next
    ABX
    LDAA 0,X
    LDX #orbit_enemies
    ABX
    STAA 0,X
    INC orbit_index
    LDAA orbit_index
    CMPA #24
    BNE orbit_copy_next
    JSR orbit_check_end
    LDAA phase
    CMPA #2
    BNE orbit_world_done
    LDAA orbit_position
    CMPA orbit_total
    BCC orbit_world_done
    TAB
    LDX #orbit_wave
    ABX
    LDAA 0,X
    STAA orbit_code
    ANDA #7
    ADDA #16
    TAB
    LDX #orbit_enemies
    ABX
    LDAA orbit_code
    LSRA
    LSRA
    LSRA
    INCA
    STAA 0,X
    INC orbit_position
orbit_world_done:
    RTS
orbit_check_end:
    TST orbit_core
    BNE orbit_check_left
    LDAA #5
    STAA phase
    RTS
orbit_check_left:
    TST orbit_left
    BNE orbit_end_done
    LDAA #4
    STAA phase
orbit_end_done:
    RTS
grid_value:
    CMPB cursor
    BNE orbit_tile_slot
    LDAA orbit_aim
    ADDA #ORBIT_PLAYER
    RTS
orbit_tile_slot:
    STAB orbit_tile_cell
    LDX #orbit_slots
    ABX
    LDAB 0,X
    CMPB #255
    BEQ orbit_tile_background
    ; Warning belongs to the upcoming entry sector. Enemy sprites themselves
    ; are drawn between the rings at pixel positions, below.
    CMPB #16
    BCS orbit_tile_flash
    LDAA orbit_motion
    CMPA #16
    BCS orbit_tile_flash
    BITA #4
    BNE orbit_tile_flash
    LDAA orbit_position
    CMPA orbit_total
    BCC orbit_tile_flash
    STAB orbit_warning_slot
    TAB
    LDX #orbit_wave
    ABX
    LDAA 0,X
    ANDA #7
    ADDA #16
    LDAB orbit_warning_slot
    CBA
    BNE orbit_tile_flash
    LDAA #ORBIT_WARNING
    RTS
orbit_tile_flash:
    LDAA input_ticks
    SUBA orbit_beam_clock
    CMPA #6
    BCC orbit_tile_background
    TST orbit_flash
    BEQ orbit_tile_background
    TBA
    ANDA #7
    CMPA orbit_aim
    BNE orbit_tile_background
    LDAA #ORBIT_BEAM
    RTS
orbit_tile_background:
    LDAB orbit_tile_cell
    LDX #board
    ABX
    LDAA 0,X
    RTS
game_render:
    JSR actor_render_scene
    TST actor_intro_pending
    BEQ actor_render_done
    CLR actor_intro_pending
    JMP actor_intro
actor_render_done:
    JMP orbit_play_sound
actor_render_scene:
    JSR orbit_erase_sprites
    JSR paint_board
    JSR orbit_draw_sprites
    JMP visual_hud
.section .bss, bss
orbit_enemies: .space 24
orbit_next: .space 24
orbit_wave: .space 24
orbit_position: .space 1
orbit_total: .space 1
orbit_left: .space 1
orbit_core: .space 1
orbit_pulse: .space 1
orbit_aim: .space 1
orbit_running: .space 1
orbit_flash: .space 1
orbit_score: .space 2
orbit_steps: .space 1
orbit_turn: .space 1
orbit_clock: .space 1
orbit_weapon_clock: .space 1
orbit_pointer: .space 2
orbit_index: .space 1
orbit_value: .space 1
orbit_target: .space 1
orbit_target_ring: .space 1
orbit_code: .space 1
orbit_tile_cell: .space 1
orbit_hud_force: .space 1
orbit_old_left: .space 1
orbit_old_core: .space 1
orbit_old_pulse: .space 1
orbit_old_score: .space 2
orbit_old_running: .space 1
.section .data, data
orbit_periods: .byte 22,20,18
orbit_left_label: .byte 76,69,70,84,0
orbit_core_label: .byte 67,79,82,69,0
orbit_pulse_label: .byte 80,85,76,83,69,0
orbit_score_label: .byte 83,0
orbit_fire_label: .byte 83,80,65,67,69,32,70,73,82,69,0
orbit_start_label: .byte 83,80,65,67,69,32,71,79,32,32,0

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

; One radial approach takes 32 visible substeps. Integer interpolation moves
; no coordinate by more than one dot per substep, including the spiral waves.
.global orbit_motion
.global orbit_draw_x
.global orbit_draw_y
.global orbit_sound
.global orbit_sound_frame
.section .text, code
orbit_erase_sprites:
    CLR orbit_sprite_index
orbit_erase_next:
    LDAB orbit_sprite_index
    LDX #orbit_drawn
    ABX
    TST 0,X
    BEQ orbit_erase_skip
    CLR 0,X
    LDX #orbit_draw_x
    ABX
    LDAA 0,X
    STAA scene_x
    LDX #orbit_draw_y
    ABX
    LDAB 0,X
    STAB scene_y
    LDAA #8
    STAA scene_w
    STAA scene_h
    LDAA scene_x
    JSR scene_rect
orbit_erase_skip:
    INC orbit_sprite_index
    LDAA orbit_sprite_index
    CMPA #24
    BNE orbit_erase_next
    RTS
orbit_draw_sprites:
    CLR orbit_sprite_index
orbit_sprite_next:
    LDAB orbit_sprite_index
    LDX #orbit_enemies
    ABX
    LDAA 0,X
    BNE orbit_sprite_live
    JMP orbit_sprite_skip
orbit_sprite_live:
    DECA
    ADDA #ORBIT_ENEMY
    LDAB #8
    MUL
    ADDD #tiles
    STD orbit_sprite_source
    LDAB orbit_sprite_index
    LDX #orbit_x
    ABX
    LDAA 0,X
    STAA orbit_from_x
    LDX #orbit_y
    ABX
    LDAA 0,X
    STAA orbit_from_y
    LDAA #56
    STAA orbit_to_x
    LDAA #32
    STAA orbit_to_y
    CMPB #8
    BCS orbit_sprite_target
    SUBB #8
    STAB orbit_sprite_target_index
    LDAA stage
    CMPA #8
    BCS orbit_sprite_target_ready
    TBA
    ANDA #248
    STAA orbit_sprite_ring
    INCB
    ANDB #7
    ADDB orbit_sprite_ring
orbit_sprite_target_ready:
    LDX #orbit_x
    ABX
    LDAA 0,X
    STAA orbit_to_x
    LDX #orbit_y
    ABX
    LDAA 0,X
    STAA orbit_to_y
orbit_sprite_target:
    LDAA orbit_to_x
    SUBA orbit_from_x
    JSR orbit_interpolate
    ADDA orbit_from_x
    STAA orbit_sprite_x
    LDAB orbit_sprite_index
    LDX #orbit_draw_x
    ABX
    STAA 0,X
    LDAA orbit_to_y
    SUBA orbit_from_y
    JSR orbit_interpolate
    ADDA orbit_from_y
    STAA orbit_sprite_y
    LDAB orbit_sprite_index
    LDX #orbit_draw_y
    ABX
    STAA 0,X
    LDX #orbit_drawn
    ABX
    INC 0,X
    LDAA #8
    STAA scene_w
    STAA scene_h
    LDAA orbit_sprite_x
    LDAB orbit_sprite_y
    JSR scene_rect
    CLR orbit_sprite_col
orbit_sprite_column:
    LDX orbit_sprite_source
    LDAB orbit_sprite_col
    ABX
    LDAA 0,X
    STAA orbit_sprite_bits
    CLR orbit_sprite_row
orbit_sprite_pixel:
    LSR orbit_sprite_bits
    BCC orbit_sprite_pixel_next
    LDAA orbit_sprite_x
    ADDA orbit_sprite_col
    LDAB orbit_sprite_y
    ADDB orbit_sprite_row
    JSR scene_pixel
orbit_sprite_pixel_next:
    INC orbit_sprite_row
    LDAA orbit_sprite_row
    CMPA #8
    BNE orbit_sprite_pixel
    INC orbit_sprite_col
    LDAA orbit_sprite_col
    CMPA #8
    BNE orbit_sprite_column
orbit_sprite_skip:
    INC orbit_sprite_index
    LDAA orbit_sprite_index
    CMPA #24
    BEQ orbit_sprites_done
    JMP orbit_sprite_next
orbit_sprites_done:
    RTS
orbit_interpolate:
    CLR orbit_delta_negative
    TSTA
    BPL orbit_delta_absolute
    INC orbit_delta_negative
    NEGA
orbit_delta_absolute:
    LDAB orbit_motion
    MUL
    LSRD
    LSRD
    LSRD
    LSRD
    LSRD
    TBA
    TST orbit_delta_negative
    BEQ orbit_delta_done
    NEGA
orbit_delta_done:
    RTS
orbit_play_sound:
    LDAB orbit_sound
    BEQ orbit_sprites_done
    CLR orbit_sound
    DECB
    ASLB
    LDX #orbit_sound_notes
    ABX
    LDX 0,X
orbit_sound_frame:
    LDD #14
    JSR sound_tone
    JMP input_poll
.section .data, data
orbit_motion_periods: .byte 4,3,3
orbit_sound_notes: .word 339,139,90,508,226
.section .bss, bss
orbit_motion: .space 1
orbit_draw_x: .space 24
orbit_draw_y: .space 24
orbit_drawn: .space 24
orbit_sprite_index: .space 1
orbit_sprite_target_index: .space 1
orbit_sprite_ring: .space 1
orbit_sprite_source: .space 2
orbit_from_x: .space 1
orbit_from_y: .space 1
orbit_to_x: .space 1
orbit_to_y: .space 1
orbit_sprite_x: .space 1
orbit_sprite_y: .space 1
orbit_sprite_col: .space 1
orbit_sprite_row: .space 1
orbit_sprite_bits: .space 1
orbit_delta_negative: .space 1
orbit_warning_slot: .space 1
orbit_beam_clock: .space 1
orbit_sound: .space 1
