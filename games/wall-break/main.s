; SPDX-License-Identifier: MIT
.global wall_board
.global wall_left
.global wall_lives
.global wall_active
.global wall_x
.global wall_y
.global wall_dx
.global wall_dy
.global wall_paddle
.global wall_score
.global wall_step
.global wall_hit
.section .text, code
game_start:
    JSR actor_initial_state
    LDAA #1
    STAA actor_intro_pending
    RTS
actor_initial_state:
    CLR wall_sound
    CLR wall_score
    CLR wall_score + 1
    CLR wall_left
    LDAA #3
    STAA wall_lives
    LDAA #52
    STAA wall_paddle
    LDAA stage
    LDAB #24
    MUL
    ADDD #wall_levels
    STD wall_pointer
    CLR wall_index
wall_load:
    LDX wall_pointer
    LDAA 0,X
    INX
    STX wall_pointer
    TSTA
    BEQ wall_store
    INC wall_left
wall_store:
    LDAB wall_index
    LDX #wall_board
    ABX
    STAA 0,X
    INC wall_index
    LDAA wall_index
    CMPA #24
    BNE wall_load
    JMP wall_ready
wall_ready:
    CLR wall_active
    LDAA wall_paddle
    ADDA #10
    STAA wall_x
    LDAA #57
    STAA wall_y
    LDAA #1
    STAA wall_dx
    LDAA #255
    STAA wall_dy
    LDAA input_ticks
    STAA wall_clock
    RTS
game_aux:
    CMPA #2
    BEQ game_start
    TST wall_active
    BEQ wall_aux_done
    JSR wall_erase
    JSR wall_miss
wall_aux_done:
    RTS
game_update:
    TST resume_pending
    BEQ wall_clock_ready
    CLR resume_pending
    LDAA input_ticks
    STAA wall_clock
wall_clock_ready:
    LDAA input_ticks
    SUBA wall_clock
    CMPA #3
    BCS wall_launch
    LDAA input_ticks
    STAA wall_clock
    TST wall_active
    BNE wall_tick_change
    LDAA input_held
    BITA #4
    BEQ wall_idle_right
    TST wall_paddle
    BEQ wall_launch
    BRA wall_tick_change
wall_idle_right:
    BITA #8
    BEQ wall_launch
    LDAA wall_paddle
    CMPA #106
    BEQ wall_launch
wall_tick_change:
    JSR wall_erase
    LDAA input_held
    BITA #4
    BEQ wall_move_right
    LDAA wall_paddle
    SUBA #3
    BCC wall_paddle_store
    CLRA
    BRA wall_paddle_store
wall_move_right:
    BITA #8
    BEQ wall_moved
    LDAA wall_paddle
    ADDA #3
    CMPA #107
    BCS wall_paddle_store
    LDAA #106
wall_paddle_store:
    STAA wall_paddle
wall_moved:
    TST wall_active
    BNE wall_flight
    LDAA wall_paddle
    ADDA #10
    STAA wall_x
    BRA wall_changed
wall_flight:
    JSR wall_step
wall_changed:
    LDAA #1
    STAA redraw
wall_launch:
    LDAA phase
    CMPA #2
    BNE wall_update_done
    LDAA input_event
    BITA #16
    BEQ wall_update_done
    TST wall_active
    BNE wall_update_done
    LDAA #1
    STAA wall_active
    STAA wall_sound
    STAA redraw
wall_update_done:
    RTS
; Restore only previous moving-object tiles before drawing the new scene.
wall_erase:
    LDAA #2
    STAA scene_w
    STAA scene_h
    LDAA wall_x
    LDAB wall_y
    JSR scene_rect
    LDAA #22
    STAA scene_w
    LDAA wall_paddle
    LDAB #60
    JMP scene_rect
; One integer movement step; axis-separated contacts cannot skip a brick.
wall_step:
    CLR wall_sound
    LDAA wall_x
    ADDA wall_dx
    CMPA #1
    BCS wall_bounce_x
    CMPA #126
    BCC wall_bounce_x
    STAA wall_candidate_x
    LDAB wall_y
    JSR wall_hit
    TSTA
    BNE wall_bounce_x
    LDAA wall_candidate_x
    STAA wall_x
    BRA wall_vertical
wall_bounce_x:
    TST wall_sound
    BNE wall_bounce_x_move
    LDAA #2
    STAA wall_sound
wall_bounce_x_move:
    NEG wall_dx
wall_vertical:
    LDAA wall_y
    ADDA wall_dy
    CMPA #9
    BCS wall_bounce_y
    STAA wall_candidate_y
    CMPA #59
    BCC wall_paddle_contact
    TAB
    LDAA wall_x
    JSR wall_hit
    TSTA
    BNE wall_bounce_y
    LDAA wall_candidate_y
    STAA wall_y
    RTS
wall_bounce_y:
    TST wall_sound
    BNE wall_bounce_y_move
    LDAA #2
    STAA wall_sound
wall_bounce_y_move:
    NEG wall_dy
    RTS
wall_paddle_contact:
    LDAA wall_dy
    BMI wall_store_y
    LDAA wall_x
    INCA
    SUBA wall_paddle
    BCS wall_below
    CMPA #22
    BCC wall_below
    LDAA wall_y
    CMPA #59
    BCC wall_below
    LDAA wall_x
    INCA
    SUBA wall_paddle
    CMPA #5
    BCS wall_deflect_far_left
    CMPA #11
    BCS wall_deflect_left
    CMPA #17
    BCC wall_deflect_far_right
    LDAA #1
    BRA wall_deflect
wall_deflect_far_right:
    LDAA #2
    BRA wall_deflect
wall_deflect_far_left:
    LDAA #254
    BRA wall_deflect
wall_deflect_left:
    LDAA #255
wall_deflect:
    STAA wall_dx
    LDAA #4
    STAA wall_sound
    LDAA #255
    STAA wall_dy
    RTS
wall_below:
    LDAA wall_candidate_y
    CMPA #63
    BCC wall_miss
wall_store_y:
    LDAA wall_candidate_y
    STAA wall_y
    RTS
wall_miss:
    LDAA #5
    STAA wall_sound
    DEC wall_lives
    BEQ wall_failed
    JMP wall_ready
wall_failed:
    CLR wall_active
    LDAA #5
    STAA phase
    RTS
; A=x,B=y centre approximation, returns A=1 on a destructible brick.
wall_hit:
    INCB
    CMPB #32
    BCC wall_hit_none
    CMPB #8
    BCS wall_hit_none
    INCA
    LSRA
    LSRA
    LSRA
    LSRA
    STAA wall_col
    TBA
    SUBA #8
    ANDA #248
    ADDA wall_col
    TAB
    LDX #wall_board
    ABX
    TST 0,X
    BEQ wall_hit_none
    DEC 0,X
    BNE wall_hit_points
    DEC wall_left
wall_hit_points:
    LDAA #3
    STAA wall_sound
    LDD wall_score
    ADDD #10
    STD wall_score
    TST wall_left
    BNE wall_hit_yes
    LDAA #4
    STAA phase
    CLR wall_active
wall_hit_yes:
    LDAA #1
    RTS
wall_hit_none:
    CLRA
    RTS
game_tile:
    CMPB #48
    BCC wall_tile_blank
    TBA
    ANDA #1
    STAA wall_tile_half
    LSRB
    LDX #wall_board
    ABX
    LDAA 0,X
    ASLA
    ADDA wall_tile_half
    INCA
    RTS
wall_tile_blank:
    CLRA
    RTS
game_render:
    JSR actor_render_scene
    TST actor_intro_pending
    BEQ actor_render_done
    CLR actor_intro_pending
    JMP actor_intro
actor_render_done:
    JMP wall_play_sound
actor_render_scene:
    JSR paint_board
    LDAA wall_x
    LDAB wall_y
    JSR scene_pixel
    LDAA wall_x
    INCA
    LDAB wall_y
    JSR scene_pixel
    LDAA wall_x
    LDAB wall_y
    INCB
    JSR scene_pixel
    LDAA wall_x
    INCA
    LDAB wall_y
    INCB
    JSR scene_pixel
    LDAA wall_paddle
    STAA wall_draw_x
    LDAA #22
    STAA wall_draw_count
wall_draw_paddle:
    LDAA wall_draw_x
    LDAB #60
    JSR scene_pixel
    LDAA wall_draw_x
    LDAB #61
    JSR scene_pixel
    INC wall_draw_x
    JSR input_poll
    DEC wall_draw_count
    BNE wall_draw_paddle
    JMP visual_hud
.section .bss, bss
wall_board: .space 24
wall_left: .space 1
wall_lives: .space 1
wall_active: .space 1
wall_x: .space 1
wall_y: .space 1
wall_dx: .space 1
wall_dy: .space 1
wall_paddle: .space 1
wall_score: .space 2
wall_clock: .space 1
wall_pointer: .space 2
wall_index: .space 1
wall_candidate_x: .space 1
wall_candidate_y: .space 1
wall_col: .space 1
wall_tile_half: .space 1
wall_draw_x: .space 1
wall_draw_count: .space 1
wall_hud_force: .space 1
wall_old_score: .space 2
wall_old_left: .space 1
wall_old_life: .space 1
wall_old_active: .space 1
.section .data, data
wall_score_label: .byte 83,67,79,82,69,0
wall_left_label: .byte 76,69,70,84,0
wall_life_label: .byte 76,73,70,69,0
wall_return_label: .byte 82,69,84,85,82,78,0
wall_start_label: .byte 83,80,65,67,69,0
wall_blank_label: .byte 32,32,32,32,32,0

; A short, cycle-timed start cue highlights the actor before controls begin.
.global actor_intro_frame
.global actor_intro_step
.global actor_intro_x
.global actor_intro_band
.section .text, code
actor_intro:
    LDAA wall_paddle
    ADDA #VIEW_X
    STAA paint_x
    LDAA #7
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
    LDAB #24
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
    ADDB #23
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

; Collision logic records one cue. Playback follows rendering, never physics.
.global wall_sound
.global wall_sound_frame
.section .text, code
wall_play_sound:
    LDAB wall_sound
    BEQ wall_sound_done
    CLR wall_sound
    DECB
    ASLB
    LDX #wall_sound_notes
    ABX
    LDX 0,X
wall_sound_frame:
    LDD #16
    JSR sound_tone
    JMP input_poll
wall_sound_done:
    RTS
.section .data, data
; Serve, wall, brick, paddle, lost ball: five clearly separated pitches.
wall_sound_notes: .word 189,452,110,226,678
.section .bss, bss
wall_sound: .space 1
