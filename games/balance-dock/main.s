; SPDX-License-Identifier: MIT
.global dock_x
.global dock_y
.global dock_left
.global dock_width
.global dock_direction
.global dock_speed
.global dock_period
.global dock_mode
.global dock_count
.global dock_goal
.global dock_flips
.global dock_score
.global dock_landing_y
.global dock_layers_left
.global dock_layers_width
.global dock_ticks
.global dock_judgement
.global dock_land
.global dock_world
.section .text, code
game_start:
    CLR dock_count
    CLR dock_score
    CLR dock_score + 1
    CLR dock_ticks
    CLR dock_landed
    CLR dock_old_valid
    LDAA #24
    STAA dock_left
    LDAA #80
    STAA dock_width
    LDAA #3
    STAA dock_flips
    LDAA stage
    ASLA
    ASLA
    ADDA #12
    STAA dock_goal
    LDAA #2
    TST stage
    BNE dock_period_set
    INCA
dock_period_set:
    STAA dock_period
    JSR dock_prepare
    CLR dock_mode
    CLR dock_judgement
    LDAA #1
    STAA dock_hud
    RTS
dock_prepare:
    LDAA stage
    INCA
    STAA dock_speed
    LDAA dock_count
dock_speed_loop:
    CMPA #5
    BCS dock_speed_limit
    SUBA #5
    INC dock_speed
    BRA dock_speed_loop
dock_speed_limit:
    LDAA dock_speed
    CMPA #5
    BCS dock_prepare_y
    LDAA #4
    STAA dock_speed
dock_prepare_y:
    LDAA dock_count
    ASLA
    STAA dock_temp
    LDAA #58
    SUBA dock_temp
    STAA dock_landing_y
    SUBA #6
    STAA dock_y
    CLR dock_x
    LDAA #1
    STAA dock_direction
    STAA dock_mode
    LDAA dock_count
    BITA #1
    BEQ dock_prepare_done
    LDAA #128
    SUBA dock_width
    STAA dock_x
    LDAA #255
    STAA dock_direction
dock_prepare_done:
    RTS
game_aux:
    CMPA #2
    BNE dock_branch_1
    JMP game_start
dock_branch_1:
    LDAA dock_mode
    CMPA #1
    BNE dock_aux_done
    TST dock_flips
    BEQ dock_aux_done
    DEC dock_flips
    NEG dock_direction
    LDAA #1
    STAA dock_hud
dock_aux_done:
    RTS
game_update:
    TST resume_pending
    BEQ dock_resumed
    CLR resume_pending
    LDAA input_ticks
    STAA dock_clock
dock_resumed:
    TST dock_mode
    BNE dock_action
    LDAA input_event
    BITA #4
    BEQ dock_ready_right
    CLR dock_x
    LDAA #1
    STAA dock_direction
    STAA redraw
    BRA dock_action
dock_ready_right:
    BITA #8
    BEQ dock_action
    LDAA #128
    SUBA dock_width
    STAA dock_x
    LDAA #255
    STAA dock_direction
    LDAA #1
    STAA redraw
dock_action:
    LDAA input_event
    BITA #16
    BEQ dock_timer
    LDAA dock_mode
    CMPA #2
    BCC dock_timer
    INC dock_mode
    LDAA dock_mode
    STAA dock_judgement
    LDAA #1
    STAA dock_hud
    STAA redraw
    LDAA input_ticks
    STAA dock_clock
    RTS
dock_timer:
    LDAA dock_mode
    BEQ dock_update_done
    CMPA #3
    BEQ dock_update_done
    LDAA input_ticks
    SUBA dock_clock
    CMPA dock_period
    BCS dock_update_done
    LDAA input_ticks
    STAA dock_clock
    JSR dock_world
    LDAA #1
    STAA redraw
dock_update_done:
    RTS
dock_world:
    INC dock_ticks
    LDAA dock_mode
    CMPA #2
    BEQ dock_fall
    LDAA #128
    SUBA dock_width
    STAA dock_limit
    TST dock_direction
    BMI dock_move_left
    LDAA dock_x
    ADDA dock_speed
    CMPA dock_limit
    BLS dock_move_store
    LDAA #255
    STAA dock_direction
    LDAA dock_limit
    BRA dock_move_store
dock_move_left:
    LDAA dock_x
    SUBA dock_speed
    BCC dock_move_store
    LDAA #1
    STAA dock_direction
    CLRA
dock_move_store:
    STAA dock_x
    RTS
dock_fall:
    LDAA dock_y
    ADDA #2
    CMPA dock_landing_y
    BCC dock_touchdown
    STAA dock_y
    RTS
dock_touchdown:
    LDAA dock_landing_y
    STAA dock_y
    JSR dock_land
    JMP input_gate
dock_land:
    LDAA dock_x
    CMPA dock_left
    BCC dock_lower_ready
    LDAA dock_left
dock_lower_ready:
    STAA dock_lower
    LDAA dock_x
    ADDA dock_width
    STAA dock_upper
    LDAA dock_left
    ADDA dock_width
    CMPA dock_upper
    BCC dock_upper_ready
    STAA dock_upper
dock_upper_ready:
    LDAA dock_upper
    SUBA dock_lower
    BLS dock_miss
    STAA dock_overlap
    CMPA dock_width
    BNE dock_trimmed
    LDAA #3
    STAA dock_judgement
    LDD dock_score
    ADDD #20
    STD dock_score
    BRA dock_record
dock_trimmed:
    LDAA #4
    STAA dock_judgement
dock_record:
    LDAA dock_lower
    STAA dock_left
    LDAB dock_count
    LDX #dock_layers_left
    ABX
    STAA 0,X
    LDAA dock_overlap
    STAA dock_width
    LDAB dock_count
    LDX #dock_layers_width
    ABX
    STAA 0,X
    LDD dock_score
    ADDB dock_width
    ADCA #0
    STD dock_score
    INC dock_count
    LDAA #1
    STAA dock_landed
    STAA dock_hud
    LDAA dock_count
    CMPA dock_goal
    BCS dock_more
    LDAA #4
    STAA phase
    LDAA #3
    STAA dock_mode
    RTS
dock_more:
    JMP dock_prepare
dock_miss:
    LDAA #5
    STAA phase
    STAA dock_judgement
    LDAA #1
    STAA dock_hud
    RTS
game_tile:
    CMPB #96
    BCS dock_sky
    LDAA #2
    RTS
dock_sky:
    CLRA
    RTS
game_render:
    CLR dock_partial
    TST resume_pending
    BEQ dock_incremental
    JSR paint_board
    LDAA #24
    STAA dock_span_left
    LDAA #60
    STAA dock_span_y
    LDAA #80
    STAA dock_span_size
    CLR dock_pattern
    LDAA #1
    STAA dock_colour
    JSR dock_span
    CLR dock_render_index
dock_restore_layers:
    LDAA dock_render_index
    CMPA dock_count
    BCC dock_restore_done
    JSR dock_draw_layer
    INC dock_render_index
    BRA dock_restore_layers
dock_restore_done:
    CLR dock_old_valid
    CLR dock_landed
    JMP dock_draw_current
dock_incremental:
    TST dock_old_valid
    BNE dock_branch_2
    JMP dock_draw_landed
dock_branch_2:
    LDAA dock_mode
    CMPA #3
    BNE dock_branch_3
    JMP dock_erase_full
dock_branch_3:
    TST dock_landed
    BEQ dock_branch_4
    JMP dock_erase_full
dock_branch_4:
    LDAA dock_y
    CMPA dock_old_y
    BEQ dock_branch_5
    JMP dock_erase_full
dock_branch_5:
    LDAA dock_width
    CMPA dock_old_width
    BEQ dock_branch_6
    JMP dock_erase_full
dock_branch_6:
    LDAA dock_x
    SUBA dock_old_x
    BEQ dock_no_horizontal_change
    BCS dock_shift_left
    STAA dock_delta
    CMPA dock_width
    BCS dock_branch_7
    JMP dock_erase_full
dock_branch_7:
    STAA dock_span_size
    LDAA dock_old_x
    STAA dock_span_left
    LDAA dock_y
    STAA dock_span_y
    CLR dock_colour
    JSR dock_span
    LDAA dock_old_x
    ADDA dock_width
    STAA dock_span_left
    BRA dock_enter_strip
dock_shift_left:
    LDAA dock_old_x
    SUBA dock_x
    STAA dock_delta
    CMPA dock_width
    BCS dock_branch_8
    JMP dock_erase_full
dock_branch_8:
    STAA dock_span_size
    LDAA dock_x
    ADDA dock_width
    STAA dock_span_left
    LDAA dock_y
    STAA dock_span_y
    CLR dock_colour
    JSR dock_span
    LDAA dock_x
    STAA dock_span_left
dock_enter_strip:
    LDAA #1
    STAA dock_colour
    LDAA dock_count
    STAA dock_pattern
    JSR dock_span
dock_no_horizontal_change:
    LDAA #1
    STAA dock_partial
    BRA dock_draw_landed
dock_erase_full:
    LDAA dock_old_x
    STAA dock_span_left
    LDAA dock_old_y
    STAA dock_span_y
    LDAA dock_old_width
    STAA dock_span_size
    CLR dock_colour
    JSR dock_span
dock_draw_landed:
    TST dock_landed
    BEQ dock_draw_current
    LDAA dock_count
    DECA
    JSR dock_draw_layer
    CLR dock_landed
dock_draw_current:
    LDAA dock_mode
    CMPA #3
    BEQ dock_hide_cargo
    TST dock_partial
    BNE dock_remember_cargo
    LDAA dock_x
    STAA dock_span_left
    LDAA dock_y
    STAA dock_span_y
    LDAA dock_width
    STAA dock_span_size
    LDAA dock_count
    STAA dock_pattern
    LDAA #1
    STAA dock_colour
    JSR dock_span
dock_remember_cargo:
    LDAA dock_x
    STAA dock_old_x
    LDAA dock_y
    STAA dock_old_y
    LDAA dock_width
    STAA dock_old_width
    LDAA #1
    STAA dock_old_valid
    BRA dock_hud_check
dock_hide_cargo:
    CLR dock_old_valid
dock_hud_check:
    JMP visual_hud
dock_draw_layer:
    STAA dock_pattern
    TAB
    LDX #dock_layers_left
    ABX
    LDAA 0,X
    STAA dock_span_left
    LDAB dock_pattern
    LDX #dock_layers_width
    ABX
    LDAA 0,X
    STAA dock_span_size
    LDAA dock_pattern
    ASLA
    STAA dock_temp
    LDAA #58
    SUBA dock_temp
    STAA dock_span_y
    LDAA #1
    STAA dock_colour
    JMP dock_span
; Crates are two pixels high at even Y, so the mask never crosses an LCD band.
; Horizontal movement only clears the leaving strip and paints the entering strip.
dock_span:
    TST dock_span_size
    BNE dock_span_begin
    RTS
dock_span_begin:
    LDAA dock_span_y
    LSRA
    LSRA
    LSRA
    STAA dock_span_band
    LDAB #192
    MUL
    ADDD #framebuffer + VIEW_X
    ADDB dock_span_left
    ADCA #0
    STD dock_destination
    LDAA dock_span_y
    ANDA #6
    LSRA
    TAB
    LDX #dock_masks
    ABX
    LDAA 0,X
    STAA dock_mask
    ANDA #85
    STAA dock_top_mask
    LDAA dock_mask
    COMA
    STAA dock_inverse
    LDAA dock_span_size
    STAA dock_columns
    LDAA dock_span_left
    STAA dock_column
    CLR dock_changed
dock_span_column:
    LDX dock_destination
    LDAA 0,X
    ANDA dock_inverse
    STAA dock_pixel_value
    TST dock_colour
    BEQ dock_span_compare
    LDAA dock_column
    ADDA dock_pattern
    ANDA #3
    BNE dock_span_solid
    LDAA dock_top_mask
    BRA dock_span_ink
dock_span_solid:
    LDAA dock_mask
dock_span_ink:
    ORAA dock_pixel_value
    STAA dock_pixel_value
dock_span_compare:
    LDAA dock_pixel_value
    CMPA 0,X
    BEQ dock_span_unchanged
    STAA 0,X
    LDAA #1
    STAA dock_changed
dock_span_unchanged:
    INX
    STX dock_destination
    INC dock_column
    DEC dock_columns
    BEQ dock_span_finish
    LDAA dock_columns
    ANDA #7
    BNE dock_span_column
    JSR input_poll
    BRA dock_span_column
dock_span_finish:
    TST dock_changed
    BEQ dock_span_done
    LDAA dock_span_band
    LDAB dock_span_left
    ADDB #VIEW_X
    JSR dirty_mark
    LDAA dock_span_band
    LDAB dock_column
    DECB
    ADDB #VIEW_X
    JMP dirty_mark
dock_span_done:
    RTS
.section .bss, bss
dock_x: .space 1
dock_y: .space 1
dock_left: .space 1
dock_width: .space 1
dock_direction: .space 1
dock_speed: .space 1
dock_period: .space 1
dock_mode: .space 1
dock_count: .space 1
dock_goal: .space 1
dock_flips: .space 1
dock_score: .space 2
dock_landing_y: .space 1
dock_layers_left: .space 20
dock_layers_width: .space 20
dock_ticks: .space 1
dock_judgement: .space 1
dock_clock: .space 1
dock_temp: .space 1
dock_limit: .space 1
dock_lower: .space 1
dock_upper: .space 1
dock_overlap: .space 1
dock_hud: .space 1
dock_landed: .space 1
dock_old_valid: .space 1
dock_old_x: .space 1
dock_old_y: .space 1
dock_old_width: .space 1
dock_partial: .space 1
dock_delta: .space 1
dock_render_index: .space 1
dock_span_left: .space 1
dock_span_y: .space 1
dock_span_size: .space 1
dock_pattern: .space 1
dock_colour: .space 1
dock_span_band: .space 1
dock_destination: .space 2
dock_mask: .space 1
dock_top_mask: .space 1
dock_inverse: .space 1
dock_columns: .space 1
dock_column: .space 1
dock_changed: .space 1
dock_pixel_value: .space 1
.section .data, data
dock_masks: .byte 3,12,48,192
dock_score_label: .byte 83,0
dock_height_label: .byte 72,69,73,71,72,84,0
dock_width_label: .byte 87,73,68,84,72,0
dock_flip_label: .byte 70,76,73,80,83,0
dock_slash_label: .byte 47,0
dock_status_labels: .word dock_ready_label,dock_move_label,dock_fall_label,dock_perfect_label,dock_trim_label,dock_miss_label
dock_ready_label: .byte 82,69,65,68,89,32,32,0
dock_move_label: .byte 77,79,86,69,32,32,32,0
dock_fall_label: .byte 70,65,76,76,32,32,32,0
dock_perfect_label: .byte 80,69,82,70,69,67,84,0
dock_trim_label: .byte 84,82,73,77,32,32,32,0
dock_miss_label: .byte 77,73,83,83,32,32,32,0
