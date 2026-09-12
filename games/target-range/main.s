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
    CLR range_flip_pending
    CLR range_flip_step
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
    LDAA #255
    STAA range_flip_cell
    LDAB stage
    LDX #range_limits
    ABX
    LDAA 0,X
    STAA range_remaining
    LDAA #1
    STAA range_mode
    STAA range_trial_pending
    STAA range_flip_pending
    STAA redraw
    CLR range_result
    LDAA input_ticks
    STAA range_clock
    JSR input_gate
    JMP range_time_value
game_update:
    JSR cursor_blink_tick
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
    STAA range_flip_cell
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
    STAA range_flip_cell
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
    STAA range_flip_pending
    STAA range_trial_pending
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
    TST range_flip_step
    BEQ range_tile_done
    TSTA
    BEQ range_tile_done
    PSHA
    LDAA range_flip_cell
    CMPA #255
    BEQ range_tile_turn
    CMPB range_flip_cell
    BEQ range_tile_turn
    PULA
    RTS
range_tile_turn:
    PULA
    LDAB range_flip_step
    CMPB #3
    BEQ range_tile_front
    TBA
    ADDA #4
    RTS
range_tile_front:
    ADDA #6
range_tile_done:
    RTS
game_render:
    JSR cursor_blink_prepare
    TST range_flip_pending
    BNE range_flip
    JSR cursor_blink_board
    JMP visual_hud
; Turn the physical panel through its narrow back, edge and narrow face.
; Time and controls resume only after the final face reaches the LCD.
.global range_flip_frame
.global range_flip_step
range_flip:
    CLR range_flip_pending
    CLR range_flip_bytes
    CLR range_flip_bytes + 1
    LDAA #1
    STAA range_flip_step
range_flip_draw:
    CLR cursor_blink_mask
    JSR cursor_blink_board
    JSR visual_hud
    JSR range_flip_transfer
range_flip_frame:
    LDAA input_ticks
    STAA range_flip_clock
range_flip_wait:
    JSR input_poll
    LDAA input_ticks
    SUBA range_flip_clock
    CMPA #3
    BCS range_flip_wait
    INC range_flip_step
    LDAA range_flip_step
    CMPA #4
    BNE range_flip_draw
    CLR range_flip_step
    LDAA #128
    STAA cursor_blink_mask
    LDAA input_ticks
    STAA cursor_blink_clock
    JSR cursor_blink_board
    LDAA phase
    CMPA #5
    BNE range_flip_finish
    JSR lose_game
range_flip_finish:
    JSR range_flip_transfer
    JSR input_gate
    JSR input_poll
    LDAA #1
    STAA resume_pending
    CLR redraw
    LDD range_flip_bytes
    STD dirty_bytes
    LDS #$5FFF
    JMP frame_ready
range_flip_transfer:
    JSR dirty_begin
range_flip_transfer_loop:
    JSR dirty_next
    BEQ range_flip_transfer_done
    JSR input_poll
    BRA range_flip_transfer_loop
range_flip_transfer_done:
    LDD dirty_bytes
    ADDD range_flip_bytes
    STD range_flip_bytes
    RTS
.section .bss, bss
range_flip_pending: .space 1
range_flip_step: .space 1
range_flip_cell: .space 1
range_flip_clock: .space 1
range_flip_bytes: .space 2
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

; The JR-800 input timer drives focus blinking; input immediately restores it.
.global cursor_blink_mask
.global cursor_blink_clock
.section .text, code
cursor_blink_prepare:
    TST hud_ready
    BEQ cursor_blink_show
    RTS
cursor_blink_tick:
    TST input_event
    BNE cursor_blink_show
    LDAA input_ticks
    SUBA cursor_blink_clock
    CMPA #25
    BCS cursor_blink_idle
    LDAA cursor_blink_mask
    EORA #128
    BRA cursor_blink_store
cursor_blink_show:
    LDAA #128
cursor_blink_store:
    CMPA cursor_blink_mask
    BEQ cursor_blink_time
    STAA cursor_blink_mask
    LDAA #1
    STAA redraw
cursor_blink_time:
    LDAA input_ticks
    STAA cursor_blink_clock
cursor_blink_idle:
    RTS
cursor_blink_board:
    LDAA cursor
    PSHA
    TST cursor_blink_mask
    BNE cursor_blink_paint
    LDAA #255
    STAA cursor
cursor_blink_paint:
    JSR paint_board
    PULA
    STAA cursor
    RTS
.section .bss, bss
cursor_blink_mask: .space 1
cursor_blink_clock: .space 1
