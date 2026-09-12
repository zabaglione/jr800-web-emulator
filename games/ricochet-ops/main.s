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
    JSR actor_initial_state
    LDAA #1
    STAA actor_intro_pending
    RTS
actor_initial_state:
    JSR grid_reset
    CLR rico_aim
    CLR rico_active
    CLR rico_score
    CLR rico_score + 1
    CLR rico_steps
    CLR rico_prev_valid
    CLR rico_sound
    LDAA #16
    STAA rico_x
    JSR challenge_start
    LDAB stage
    LDX #rico_ammos
    ABX
    LDAA 0,X
    STAA rico_ammo
    LDAA #41
    STAA cursor
    LDX #board
    JSR challenge_load
    CLR rico_left
    LDAB #0
rico_count_targets:
    LDX #board
    ABX
    LDAA 0,X
    CMPA #4
    BNE rico_count_next
    INC rico_left
rico_count_next:
    INCB
    CMPB #48
    BNE rico_count_targets
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
    JSR challenge_undo
    CLR rico_prev_valid
    CLR rico_active
    LDAA rico_prev_cursor
    STAA cursor
    ANDA #7
    ASLA
    ASLA
    ASLA
    ASLA
    STAA rico_x
    ; Undo restores the launch position together with the shot state.
    LDX #tile_cache + 80
    LDAB #16
    LDAA #255
rico_restore_row:
    STAA 0,X
    INX
    DECB
    BNE rico_restore_row
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
    JSR challenge_step
    LDAA #1
    STAA rico_sound
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
    LDAB #96
    CLRA
rico_clear_seen:
    STAA 0,X
    INX
    DECB
    BNE rico_clear_seen
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
    LDAB rico_direction
    LDX #rico_bits
    ABX
    LDAA 0,X
    LDAB rico_next_cell
    LDX #rico_visited
    ABX
    BITA 0,X
    BEQ rico_continue_5
    JMP rico_stop
rico_continue_5:
    ORAA 0,X
    STAA 0,X
    LDAB rico_next_cell
    STAB rico_bullet
    JSR challenge_touch
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
    LDAA #2
    STAA rico_sound
    RTS
rico_target:
    LDX #board
    ABX
    CLR 0,X
    DEC rico_left
    LDD rico_score
    ADDD #100
    STD rico_score
    LDAA #3
    STAA rico_sound
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
game_bonus:
    RTS
game_render:
    JSR actor_render_scene
    TST actor_intro_pending
    BEQ actor_render_done
    CLR actor_intro_pending
    JMP actor_intro
actor_render_done:
    LDAB rico_sound
    BEQ rico_sound_done
    CLR rico_sound
    DECB
    ASLB
    LDX #rico_sounds
    ABX
    LDX 0,X
    LDD #20
    JSR sound_tone
    JSR input_poll
rico_sound_done:
    RTS
actor_render_scene:
    LDAA cursor
    ANDA #7
    ASLA
    ASLA
    ASLA
    ASLA
    STAA rico_destination
    CMPA rico_x
    BNE rico_move_pixels
    JSR rico_paint_cannon
    JMP visual_hud
; A grid move is displayed one dot at a time. The firing position and the
; authored mirror puzzle stay synchronized at each resting launch position.
.global rico_motion_frame
.global rico_x
rico_move_pixels:
    CLR actor_intro_bytes
    CLR actor_intro_bytes + 1
rico_move_pixel:
    JSR rico_erase_cannon
    LDAA rico_x
    CMPA rico_destination
    BCS rico_move_right
    DEC rico_x
    BRA rico_move_draw
rico_move_right:
    INC rico_x
rico_move_draw:
    JSR rico_paint_cannon
    JSR visual_hud
    JSR dirty_begin
rico_motion_transfer:
    JSR dirty_next
    BEQ rico_motion_sum
    JSR input_poll
    BRA rico_motion_transfer
rico_motion_sum:
    LDD dirty_bytes
    ADDD actor_intro_bytes
    STD actor_intro_bytes
rico_motion_frame:
    LDAA input_ticks
    STAA actor_intro_clock
rico_motion_wait:
    JSR input_poll
    LDAA input_ticks
    CMPA actor_intro_clock
    BEQ rico_motion_wait
    LDAA rico_x
    CMPA rico_destination
    BNE rico_move_pixel
    LDD actor_intro_bytes
    STD dirty_bytes
    CLR redraw
    LDS #$5FFF
    JMP frame_ready
rico_erase_cannon:
    LDAB rico_x
    LSRB
    LSRB
    LSRB
    LDX #tile_cache + 80
    ABX
    LDAA #255
    STAA 0,X
    STAA 1,X
    STAA 2,X
    RTS
rico_paint_cannon:
    JSR rico_erase_cannon
    JSR paint_board
    LDAA rico_aim
    ADDA #5
    LDAB #16
    MUL
    ADDD #tiles + 8
    STD paint_source
    LDAA rico_x
    ADDA #VIEW_X
    STAA paint_x
    LDAA #6
    STAA paint_band
    JSR paint_address
    CLR paint_id
    LDAA #16
    STAA paint_count
    JMP paint_blit
.section .bss, bss
rico_x: .space 1
rico_destination: .space 1
rico_sound: .space 1
rico_aim: .space 1
rico_active: .space 1
rico_ammo: .space 1
rico_left: .space 1
rico_score: .space 2
rico_bullet: .space 1
rico_direction: .space 1
rico_visited: .space 48
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
rico_index: .space 1
rico_column: .space 1
rico_row: .space 1
rico_next_cell: .space 1
rico_cell_value: .space 1
.section .data, data
rico_bits: .byte 1,2,4,8,16,32,64,128
rico_sounds: .word 139,339,90
rico_dx: .byte 0,1,1,1,0,255,255,255
rico_dy: .byte 255,255,0,1,1,1,0,255
rico_slash_turn: .byte 2,1,0,7,6,5,4,3
rico_back_turn: .byte 6,5,4,3,2,1,0,7
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
.global actor_intro_blink
    CLR actor_intro_bytes + 1
    JMP actor_intro_blink
.section .runtime, code
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
    LDAB #16
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
    ADDB #15
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
    JMP actor_intro_frame
.section .text, code
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
