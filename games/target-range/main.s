; SPDX-License-Identifier: MIT
.global range_round
.global range_mode
.global range_remaining
.global range_clock
.global range_target
.global range_decoy
.global range_decoy2
.global range_score
.global range_misses
.global range_result
.global range_shoot
.global range_expire
.global range_trial_pending
.section .text, code
game_start:
    JSR grid_reset
    LDAA #4
    STAA cursor
    CLR range_round
    CLR range_mode
    CLR range_remaining
    CLR range_score
    CLR range_score + 1
    CLR range_misses
    CLR range_result
    CLR range_trial_pending
    CLR range_time_display
    LDAA #255
    STAA range_target
    STAA range_decoy
    STAA range_decoy2
    LDAA input_ticks
    STAA range_clock
    RTS
game_aux:
    JMP game_start
range_start_trial:
    LDX #board
    LDAB #9
    CLRA
range_clear_board:
    STAA 0,X
    INX
    DECB
    BNE range_clear_board
    LDAA stage
    LDAB #60
    MUL
    ADDD #range_levels
    STD range_pointer
    LDAA range_round
    LDAB #3
    MUL
    ADDD range_pointer
    XGDX
    LDAA 0,X
    STAA range_target
    LDAA 1,X
    STAA range_decoy
    LDAA 2,X
    STAA range_decoy2
    LDAB range_target
    CMPB #255
    BEQ range_place_decoys
    LDX #board
    ABX
    LDAA #1
    STAA 0,X
range_place_decoys:
    LDAB range_decoy
    LDX #board
    ABX
    LDAA #2
    STAA 0,X
    LDAB range_decoy2
    CMPB #255
    BEQ range_trial_ready
    LDX #board
    ABX
    STAA 0,X
range_trial_ready:
    LDAB stage
    LDX #range_limits
    ABX
    LDAA 0,X
    STAA range_remaining
    LDAA #1
    STAA range_mode
    STAA range_trial_pending
    STAA redraw
    CLR range_result
    LDAA input_ticks
    STAA range_clock
    JSR input_gate
    JMP range_time_value
game_update:
    TST resume_pending
    BNE range_reset_clock
    TST range_trial_pending
    BEQ range_update_mode
range_reset_clock:
    CLR resume_pending
    CLR range_trial_pending
    LDAA input_ticks
    STAA range_clock
range_update_mode:
    TST range_mode
    BNE range_tick
    LDAA input_event
    BITA #16
    BEQ range_update_done
    JMP range_start_trial
range_tick:
    LDAA input_ticks
    SUBA range_clock
    STAA range_elapsed
    LDAA input_ticks
    STAA range_clock
    LDAA range_remaining
    SUBA range_elapsed
    BLS range_timeout
    STAA range_remaining
    LDAA range_mode
    CMPA #1
    BNE range_update_done
    JSR range_time_value
    JSR grid_move
    CMPB #255
    BEQ range_action
    STAB cursor
    LDAA #1
    STAA redraw
range_action:
    LDAA input_event
    BITA #16
    BEQ range_update_done
    JMP range_shoot
range_timeout:
    LDAA range_mode
    CMPA #1
    BNE range_next_trial
    JMP range_expire
range_next_trial:
    INC range_round
    LDAA range_round
    CMPA #20
    BCC range_finish
    JMP range_start_trial
range_finish:
    LDAB stage
    ASLB
    LDX #range_goals
    ABX
    LDD range_score
    SUBD 0,X
    BCC range_clear
    LDAA #5
    BRA range_finished
range_clear:
    LDAA #4
range_finished:
    STAA phase
    LDAA #1
    STAA redraw
range_update_done:
    RTS
range_shoot:
    LDAA cursor
    CMPA range_target
    BNE range_wrong
    LDAA range_remaining
    LSRA
    LSRA
    LSRA
    ADDA #10
    TAB
    CLRA
    ADDD range_score
    STD range_score
    LDAA #1
    STAA range_result
    LDAB cursor
    LDX #board
    ABX
    LDAA #3
    STAA 0,X
    BRA range_feedback
range_wrong:
    INC range_misses
    LDAA #4
    STAA range_result
    LDAB cursor
    LDX #board
    ABX
    LDAA #4
    STAA 0,X
    BRA range_feedback
range_expire:
    LDAA range_target
    CMPA #255
    BNE range_missed
    LDD range_score
    ADDD #10
    STD range_score
    LDAA #2
    STAA range_result
    BRA range_feedback
range_missed:
    INC range_misses
    LDAA #3
    STAA range_result
range_feedback:
    LDAA #2
    STAA range_mode
    LDAA #12
    STAA range_remaining
    CLR range_time_display
    LDAA #1
    STAA redraw
    LDAA range_misses
    CMPA #3
    BCS range_feedback_done
    LDAA #5
    STAA phase
range_feedback_done:
    RTS
range_time_value:
    LDAA range_remaining
    ADDA #7
    LSRA
    LSRA
    LSRA
    STAA range_time_display
    CMPA range_old_time
    BEQ range_time_done
    LDAA #1
    STAA redraw
range_time_done:
    RTS
grid_value:
    LDX #board
    ABX
    LDAA 0,X
    RTS
game_render:
    JSR paint_board
    CLR range_hud_force
    TST resume_pending
    BEQ range_hud_values
    INC range_hud_force
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
    LDX #range_score_label
    LDAA #132
    LDAB #1
    JSR paint_text
    LDX #range_round_label
    LDAA #132
    LDAB #3
    JSR paint_text
    LDX #range_time_label
    LDAA #138
    LDAB #5
    JSR paint_text
    LDX #range_miss_label
    LDAA #132
    LDAB #7
    JSR paint_text
range_hud_values:
    TST range_hud_force
    BNE range_score_value
    LDD range_score
    SUBD range_old_score
    BEQ range_round_check
range_score_value:
    LDD range_score
    STD range_old_score
    LDAA #138
    STAA paint_x
    LDAA #2
    STAA paint_band
    LDD range_score
    JSR paint_number16
range_round_check:
    TST range_hud_force
    BNE range_round_value
    LDAA range_round
    CMPA range_old_round
    BEQ range_time_check
range_round_value:
    LDAA range_round
    STAA range_old_round
    CMPA #20
    BCS range_round_increment
    LDAA #19
range_round_increment:
    INCA
    STAA range_show_round
    LDAA #138
    STAA paint_x
    LDAA #4
    STAA paint_band
    LDAA range_show_round
    JSR paint_number
range_time_check:
    TST range_hud_force
    BNE range_time_paint
    LDAA range_time_display
    CMPA range_old_time
    BEQ range_miss_check
range_time_paint:
    LDAA range_time_display
    STAA range_old_time
    LDAA #138
    STAA paint_x
    LDAA #6
    STAA paint_band
    LDAA range_time_display
    JSR paint_number
range_miss_check:
    TST range_hud_force
    BNE range_miss_value
    LDAA range_misses
    CMPA range_old_misses
    BEQ range_mode_check
range_miss_value:
    LDAA range_misses
    STAA range_old_misses
    LDAA #162
    STAA paint_x
    LDAA #7
    STAA paint_band
    LDAA range_misses
    JSR paint_number
range_mode_check:
    TST range_hud_force
    BNE range_mode_value
    LDAA range_mode
    CMPA range_old_mode
    BNE range_mode_value
    LDAA range_result
    CMPA range_old_result
    BEQ range_render_done
range_mode_value:
    LDAA range_mode
    STAA range_old_mode
    LDAA range_result
    STAA range_old_result
    LDAB #11
    MUL
    ADDD #range_labels
    XGDX
    TST range_mode
    BNE range_mode_paint
    LDX #range_start_label
range_mode_paint:
    LDAA #78
    CLRB
    JMP paint_text
range_render_done:
    RTS
.section .bss, bss
range_trial_pending: .space 1
range_round: .space 1
range_mode: .space 1
range_remaining: .space 1
range_clock: .space 1
range_target: .space 1
range_decoy: .space 1
range_decoy2: .space 1
range_score: .space 2
range_misses: .space 1
range_result: .space 1
range_elapsed: .space 1
range_pointer: .space 2
range_time_display: .space 1
range_hud_force: .space 1
range_old_score: .space 2
range_old_round: .space 1
range_old_time: .space 1
range_old_misses: .space 1
range_old_mode: .space 1
range_old_result: .space 1
range_show_round: .space 1
.section .data, data
range_limits: .byte 144,112,88
range_goals: .word 180,220,250
range_score_label: .byte 83,67,79,82,69,0
range_round_label: .byte 82,79,85,78,68,0
range_time_label: .byte 84,73,77,69,0
range_miss_label: .byte 77,73,83,83,0
range_start_label: .byte 83,80,65,67,69,32,71,79,32,32,0
range_labels:
    .byte 83,80,65,67,69,32,70,73,82,69,0
    .byte 72,73,84,32,32,32,32,32,32,32,0
    .byte 83,65,70,69,32,32,32,32,32,32,0
    .byte 77,73,83,83,32,32,32,32,32,32,0
    .byte 87,82,79,78,71,32,32,32,32,32,0
