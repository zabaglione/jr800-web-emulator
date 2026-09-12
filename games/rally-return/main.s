; SPDX-License-Identifier: MIT
.global rally_x
.global rally_y
.global rally_dx
.global rally_dy
.global rally_player
.global rally_cpu
.global rally_you
.global rally_them
.global rally_active
.global rally_spin
.global rally_count
.global rally_period
.global rally_ai_period
.global rally_ai_clock
.global rally_world
.global rally_motion
.global rally_deflect
.section .text, code
game_start:
    CLR rally_point_pending
    JSR actor_initial_state
    LDAA #1
    STAA actor_intro_pending
    RTS
actor_initial_state:
    CLR rally_you
    CLR rally_them
    CLR rally_spin
    CLR rally_count
    LDAA #30
    STAA rally_player
    LDAA #3
    SUBA stage
    STAA rally_ai_period
    LDAA #2
    TST stage
    BNE rally_period_set
    INCA
rally_period_set:
    STAA rally_period
rally_ready:
    CLR rally_active
    CLR rally_ai_clock
    LDAA #60
    STAA rally_x
    LDAA #36
    STAA rally_y
    LDAA #30
    STAA rally_cpu
    LDAA #2
    STAA rally_dx
    CLR rally_dy
    LDAA #1
    STAA rally_hud
    RTS
game_aux:
    CMPA #2
    BEQ game_start
    CLR rally_spin
    LDAA #1
    STAA rally_hud
    RTS
game_update:
    TST resume_pending
    BEQ rally_clock_ready
    CLR resume_pending
    LDAA input_ticks
    STAA rally_clock
rally_clock_ready:
    LDAA input_event
    BITA #4
    BEQ rally_choose_right
    LDAB #255
    BRA rally_choose_spin
rally_choose_right:
    BITA #8
    BEQ rally_elapsed
    LDAB #1
rally_choose_spin:
    CMPB rally_spin
    BEQ rally_elapsed
    STAB rally_spin
    LDAA #1
    STAA rally_hud
    STAA redraw
rally_elapsed:
    LDAA input_ticks
    SUBA rally_clock
    CMPA rally_period
    BCS rally_serve
    LDAA input_ticks
    STAA rally_clock
    TST rally_active
    BNE rally_update_world
    LDAA input_held
    ANDA #3
    BEQ rally_serve
rally_update_world:
    JSR rally_erase
    LDAA input_held
    BITA #1
    BEQ rally_player_down
    LDAA rally_player
    SUBA #3
    CMPA #9
    BCC rally_player_store
    LDAA #9
    BRA rally_player_store
rally_player_down:
    BITA #2
    BEQ rally_player_done
    LDAA rally_player
    ADDA #3
    CMPA #52
    BCS rally_player_store
    LDAA #51
rally_player_store:
    STAA rally_player
rally_player_done:
    TST rally_active
    BEQ rally_changed
    JSR rally_world
rally_changed:
    LDAA #1
    STAA redraw
rally_serve:
    TST rally_active
    BNE rally_update_done
    LDAA phase
    CMPA #2
    BNE rally_update_done
    LDAA input_event
    BITA #16
    BEQ rally_update_done
    LDAA #1
    STAA rally_active
    STAA rally_hud
    STAA redraw
    LDAA input_ticks
    STAA rally_clock
rally_update_done:
    RTS
rally_world:
    INC rally_count
    INC rally_ai_clock
    LDAA rally_ai_clock
    CMPA rally_ai_period
    BCS rally_motion
    CLR rally_ai_clock
    LDAA rally_cpu
    ADDA #5
    CMPA rally_y
    BEQ rally_motion
    BCS rally_cpu_down
    LDAA rally_cpu
    CMPA #9
    BEQ rally_motion
    DEC rally_cpu
    BRA rally_motion
rally_cpu_down:
    LDAA rally_cpu
    CMPA #51
    BEQ rally_motion
    INC rally_cpu
rally_motion:
    LDAA rally_y
    ADDA rally_dy
    CMPA #9
    BCS rally_bounce_y
    CMPA #62
    BCC rally_bounce_y
    STAA rally_y
    BRA rally_horizontal
rally_bounce_y:
    NEG rally_dy
rally_horizontal:
    LDAA rally_x
    ADDA rally_dx
    STAA rally_next_x
    TST rally_dx
    BPL rally_test_cpu
    CMPA #10
    BCC rally_move_x
    LDAB rally_x
    CMPB #10
    BCS rally_move_x
    LDAA rally_y
    INCA
    SUBA rally_player
    BCS rally_move_x
    CMPA #13
    BCC rally_move_x
    JSR rally_deflect
    ADDA rally_spin
    CMPA #253
    BGE rally_clamp_upper
    LDAA #253
rally_clamp_upper:
    CMPA #3
    BLE rally_player_angle
    LDAA #3
rally_player_angle:
    STAA rally_dy
    LDAA #2
    STAA rally_dx
    LDAA #10
    STAA rally_x
    RTS
rally_test_cpu:
    CMPA #115
    BCS rally_move_x
    LDAB rally_x
    CMPB #115
    BCC rally_move_x
    LDAA rally_y
    INCA
    SUBA rally_cpu
    BCS rally_move_x
    CMPA #13
    BCC rally_move_x
    JSR rally_deflect
    STAA rally_dy
    LDAA #254
    STAA rally_dx
    LDAA #114
    STAA rally_x
    RTS
rally_move_x:
    LDAA rally_next_x
    CMPA #2
    BCS rally_cpu_point
    CMPA #125
    BCC rally_player_point
    STAA rally_x
    RTS
rally_cpu_point:
    INC rally_them
    JSR rally_ready
    LDAA #2
    STAA rally_point_pending
    LDAA rally_them
    CMPA #5
    BCS rally_motion_done
    LDAA #5
    STAA phase
    RTS
rally_player_point:
    INC rally_you
    JSR rally_ready
    LDAA #1
    STAA rally_point_pending
    LDAA rally_you
    CMPA #5
    BCS rally_motion_done
    LDAA #4
    STAA phase
rally_motion_done:
    RTS
; A is the relative impact height0..12; return signed vertical speed.
rally_deflect:
    CMPA #3
    BCS rally_deflect_top
    CMPA #6
    BCS rally_deflect_upper
    CMPA #10
    BCS rally_deflect_lower
    LDAA #3
    RTS
rally_deflect_top:
    LDAA #253
    RTS
rally_deflect_upper:
    LDAA #255
    RTS
rally_deflect_lower:
    LDAA #1
    RTS
rally_erase:
    LDAA #2
    STAA scene_w
    STAA scene_h
    LDAA rally_x
    LDAB rally_y
    JSR scene_rect
    LDAA #12
    STAA scene_h
    LDAA #8
    LDAB rally_player
    JSR scene_rect
    LDAA #116
    LDAB rally_cpu
    JMP scene_rect
game_tile:
    CMPB #16
    BCS rally_tile_top
    CMPB #96
    BCC rally_tile_bottom
    ANDB #15
    CMPB #8
    BNE rally_tile_blank
    LDAA #4
    RTS
rally_tile_top:
    LDAA #2
    RTS
rally_tile_bottom:
    LDAA #3
    RTS
rally_tile_blank:
    CLRA
    RTS
game_render:
    JSR actor_render_scene
    TST actor_intro_pending
    BEQ actor_render_done
    CLR actor_intro_pending
    JMP actor_intro
actor_render_done:
    TST rally_point_pending
    BEQ rally_render_done
    JMP rally_point_music
rally_render_done:
    RTS
actor_render_scene:
    JSR paint_board
    LDAA #2
    STAA rally_draw_height
    LDAA rally_x
    LDAB rally_y
    JSR rally_draw
    LDAA #12
    STAA rally_draw_height
    LDAA #8
    LDAB rally_player
    JSR rally_draw
    LDAA #116
    LDAB rally_cpu
    JSR rally_draw
    JMP visual_hud
rally_draw:
    STAA rally_draw_x
    STAB rally_draw_y
    LDAA rally_draw_height
    STAA rally_draw_left
rally_draw_row:
    LDAA rally_draw_x
    LDAB rally_draw_y
    JSR scene_pixel
    LDAA rally_draw_x
    INCA
    LDAB rally_draw_y
    JSR scene_pixel
    JSR input_poll
    INC rally_draw_y
    DEC rally_draw_left
    BNE rally_draw_row
    RTS
.section .bss, bss
rally_x: .space 1
rally_y: .space 1
rally_dx: .space 1
rally_dy: .space 1
rally_player: .space 1
rally_cpu: .space 1
rally_you: .space 1
rally_them: .space 1
rally_active: .space 1
rally_spin: .space 1
rally_count: .space 1
rally_period: .space 1
rally_ai_period: .space 1
rally_ai_clock: .space 1
rally_clock: .space 1
rally_next_x: .space 1
rally_hud: .space 1
rally_draw_x: .space 1
rally_draw_y: .space 1
rally_draw_left: .space 1
rally_draw_height: .space 1
.section .data, data
rally_you_label: .byte 89,79,85,0
rally_cpu_label: .byte 67,80,85,0
rally_spin_label: .byte 83,80,73,78,0
rally_space_label: .byte 83,80,65,67,69,0
rally_ready_label: .byte 82,69,65,68,89,0
rally_blank_label: .byte 32,32,32,32,32,0
rally_spin_zero: .byte 32,48,32,0
rally_spin_minus: .byte 45,49,32,0
rally_spin_positive: .byte 43,49,32,0

; A short, cycle-timed start cue highlights the actor before controls begin.
.global actor_intro_frame
.global actor_intro_step
.global actor_intro_x
.global actor_intro_band
.section .text, code
actor_intro:
    LDAA #VIEW_X + 8
    STAA paint_x
    LDAA rally_player
    LSRA
    LSRA
    LSRA
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
    LDAA #3
    STAA actor_intro_rows
actor_intro_row:
    LDX paint_dest
    LDAB #4
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
    ADDB #3
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

.section .text, code
.global rally_point_frame
.global rally_point_note
rally_point_music:
    CLR rally_point_note
    CLR rally_point_bytes
    CLR rally_point_bytes + 1
    JSR rally_point_flush
rally_point_frame:
    LDAB rally_point_note
    ASLB
    LDX #rally_point_notes
    LDAA rally_point_pending
    CMPA #2
    BNE rally_point_notes_ready
    ADDB #6
rally_point_notes_ready:
    ABX
    LDX 0,X
    LDD #128
    JSR sound_tone
    JSR input_poll
    INC rally_point_note
    LDAA rally_point_note
    CMPA #3
    BNE rally_point_frame
    LDAA phase
    CMPA #4
    BNE rally_point_loss
    JSR win_game
    BRA rally_point_finish
rally_point_loss:
    CMPA #5
    BNE rally_point_finish
    JSR lose_game
rally_point_finish:
    CLR rally_point_pending
    JSR rally_point_flush
    JSR input_gate
    JSR input_poll
    LDAA #1
    STAA resume_pending
    CLR redraw
    LDD rally_point_bytes
    STD dirty_bytes
    LDS #$5FFF
    JMP frame_ready
rally_point_flush:
    JSR dirty_begin
rally_point_transfer:
    JSR dirty_next
    BEQ rally_point_sum
    JSR input_poll
    BRA rally_point_transfer
rally_point_sum:
    LDD dirty_bytes
    ADDD rally_point_bytes
    STD rally_point_bytes
    RTS

.section .bss, bss
rally_point_pending: .space 1
rally_point_note: .space 1
rally_point_bytes: .space 2
.section .data, data
rally_point_notes: .word 189,139,110,226,286,339
