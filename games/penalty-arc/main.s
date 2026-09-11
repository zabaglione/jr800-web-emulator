; SPDX-License-Identifier: MIT
.global penalty_mode
.global penalty_power
.global penalty_power_dir
.global penalty_keeper
.global penalty_keeper_dir
.global penalty_target
.global penalty_height
.global penalty_progress
.global penalty_x
.global penalty_y
.global penalty_kicks
.global penalty_goals
.global penalty_reason
.global penalty_count
.global penalty_world
.global penalty_result
.global penalty_patrol
.section .text, code
game_start:
    CLR penalty_kicks
    CLR penalty_goals
    CLR penalty_height
    CLR penalty_count
    LDAA #2
    STAA penalty_target
penalty_ready:
    CLR penalty_mode
    CLR penalty_power
    CLR penalty_progress
    CLR penalty_reason
    LDAA #1
    STAA penalty_power_dir
    STAA penalty_keeper_dir
    LDAA penalty_kicks
penalty_start_mod:
    CMPA #3
    BCS penalty_start_position
    SUBA #3
    BRA penalty_start_mod
penalty_start_position:
    LDAB #40
    MUL
    ADDB #24
    STAB penalty_keeper
    CMPB #64
    BCS penalty_start_ball
    LDAA #255
    STAA penalty_keeper_dir
penalty_start_ball:
    LDAA #64
    STAA penalty_x
    LDAA #60
    STAA penalty_y
    LDAA #1
    STAA penalty_hud
    RTS
game_aux:
    CMPA #2
    BEQ game_start
    LDAA penalty_mode
    CMPA #1
    BNE penalty_aux_done
    JSR penalty_erase
    JSR penalty_ready
penalty_aux_done:
    RTS
game_update:
    TST resume_pending
    BEQ penalty_resumed
    CLR resume_pending
    LDAA input_ticks
    STAA penalty_clock
penalty_resumed:
    LDAA penalty_mode
    CMPA #2
    BCC penalty_action
    LDAA input_event
    ANDA #15
    BEQ penalty_action
    JSR penalty_erase
    LDAA input_event
    BITA #4
    BEQ penalty_right
    TST penalty_target
    BNE penalty_decrease
    LDAA #5
    STAA penalty_target
penalty_decrease:
    DEC penalty_target
    BRA penalty_aim_changed
penalty_right:
    BITA #8
    BEQ penalty_height_up
    INC penalty_target
    LDAA penalty_target
    CMPA #5
    BNE penalty_aim_changed
    CLR penalty_target
    BRA penalty_aim_changed
penalty_height_up:
    BITA #1
    BEQ penalty_height_down
    LDAA #1
    STAA penalty_height
    BRA penalty_aim_changed
penalty_height_down:
    CLR penalty_height
penalty_aim_changed:
    LDAA #1
    STAA redraw
    STAA penalty_hud
penalty_action:
    LDAA input_event
    BITA #16
    BEQ penalty_timer
    LDAA penalty_mode
    CMPA #2
    BEQ penalty_timer
    CMPA #3
    BEQ penalty_next
    INC penalty_mode
    LDAA input_ticks
    STAA penalty_clock
    LDAA #1
    STAA penalty_hud
    STAA redraw
    RTS
penalty_next:
    JSR penalty_erase
    JSR penalty_ready
    JSR input_gate
    LDAA #1
    STAA redraw
    RTS
penalty_timer:
    LDAA penalty_mode
    BEQ penalty_update_done
    CMPA #3
    BEQ penalty_update_done
    LDAA input_ticks
    SUBA penalty_clock
    CMPA #3
    BCS penalty_update_done
    LDAA input_ticks
    STAA penalty_clock
    JSR penalty_erase
    JSR penalty_world
    LDAA #1
    STAA redraw
penalty_update_done:
    RTS
penalty_world:
    INC penalty_count
    LDAA penalty_mode
    CMPA #2
    BEQ penalty_flight
    LDAA penalty_power
    ADDA penalty_power_dir
    STAA penalty_power
    TSTA
    BEQ penalty_meter_up
    CMPA #10
    BNE penalty_meter_ready
    LDAA #255
    STAA penalty_power_dir
    BRA penalty_meter_ready
penalty_meter_up:
    LDAA #1
    STAA penalty_power_dir
penalty_meter_ready:
    LDAA #1
    STAA penalty_hud
    JMP penalty_patrol
penalty_flight:
    INC penalty_progress
    LDAA penalty_progress
    CMPA #5
    BCC penalty_dive
    JSR penalty_patrol
    BRA penalty_ball_position
penalty_dive:
    LDAB penalty_target
    LDX #penalty_targets
    ABX
    LDAA 0,X
    SUBA penalty_keeper
    BEQ penalty_ball_position
    BMI penalty_dive_left
    STAA penalty_distance
    LDAA stage
    INCA
    CMPA penalty_distance
    BCS penalty_move_right
    LDAA penalty_distance
penalty_move_right:
    ADDA penalty_keeper
    STAA penalty_keeper
    BRA penalty_ball_position
penalty_dive_left:
    NEGA
    STAA penalty_distance
    LDAA stage
    INCA
    CMPA penalty_distance
    BCS penalty_move_left
    LDAA penalty_distance
penalty_move_left:
    STAA penalty_distance
    LDAA penalty_keeper
    SUBA penalty_distance
    STAA penalty_keeper
penalty_ball_position:
    LDAA penalty_target
    LDAB #17
    MUL
    ADDB penalty_progress
    ADDD #penalty_flight_x
    XGDX
    LDAA 0,X
    STAA penalty_x
    LDAA penalty_height
    LDAB #17
    MUL
    ADDB penalty_progress
    ADDD #penalty_flight_y
    XGDX
    LDAA 0,X
    STAA penalty_y
    LDAA penalty_progress
    CMPA #16
    BNE penalty_world_done
    JSR penalty_result
    JMP input_gate
penalty_world_done:
    RTS
penalty_patrol:
    LDAA stage
    ADDA #2
    TST penalty_keeper_dir
    BMI penalty_patrol_left
    ADDA penalty_keeper
    CMPA #105
    BCS penalty_patrol_store
    LDAA #255
    STAA penalty_keeper_dir
    LDAA #104
    BRA penalty_patrol_store
penalty_patrol_left:
    STAA penalty_distance
    LDAA penalty_keeper
    SUBA penalty_distance
    CMPA #16
    BCC penalty_patrol_store
    LDAA #1
    STAA penalty_keeper_dir
    LDAA #16
penalty_patrol_store:
    STAA penalty_keeper
    RTS
penalty_result:
    LDAA #3
    STAA penalty_reason
    STAA penalty_mode
    LDAA penalty_power
    TST penalty_height
    BEQ penalty_low_power
    CMPA #5
    BCS penalty_record
    CMPA #7
    BCC penalty_record
    LDAB #5
    BRA penalty_reach
penalty_low_power:
    CMPA #4
    BCS penalty_record
    CMPA #8
    BCC penalty_record
    LDAB #8
penalty_reach:
    STAB penalty_distance
    LDAB penalty_target
    LDX #penalty_targets
    ABX
    LDAA 0,X
    SUBA penalty_keeper
    BPL penalty_positive_distance
    NEGA
penalty_positive_distance:
    CMPA penalty_distance
    BHI penalty_goal
    LDAA #2
    STAA penalty_reason
    BRA penalty_record
penalty_goal:
    LDAA #1
    STAA penalty_reason
    INC penalty_goals
penalty_record:
    INC penalty_kicks
    LDAA #1
    STAA penalty_hud
    LDAA penalty_kicks
    CMPA #10
    BNE penalty_result_done
    LDAA stage
    ADDA #6
    CMPA penalty_goals
    BHI penalty_failed
    LDAA #4
    STAA phase
    RTS
penalty_failed:
    LDAA #5
    STAA phase
penalty_result_done:
    RTS
penalty_erase:
    LDAA #2
    STAA scene_w
    STAA scene_h
    LDAA penalty_x
    LDAB penalty_y
    JSR scene_rect
    LDAA #13
    STAA scene_w
    LDAA #21
    STAA scene_h
    LDAA penalty_keeper
    SUBA #6
    LDAB #10
    JSR scene_rect
    LDAA penalty_mode
    CMPA #2
    BCC penalty_erase_done
    JSR penalty_aim_position
    LDAA #5
    STAA scene_w
    STAA scene_h
    LDAA penalty_aim_x
    SUBA #2
    LDAB penalty_aim_y
    SUBB #2
    JMP scene_rect
penalty_erase_done:
    RTS
penalty_aim_position:
    LDAB penalty_target
    LDX #penalty_targets
    ABX
    LDAA 0,X
    STAA penalty_aim_x
    LDAA #24
    TST penalty_height
    BEQ penalty_aim_y_ready
    LDAA #14
penalty_aim_y_ready:
    STAA penalty_aim_y
    RTS
game_tile:
    TBA
    INCA
    RTS
game_render:
    JSR paint_board
    LDAA #2
    STAA penalty_w
    STAA penalty_h
    LDAA penalty_x
    LDAB penalty_y
    JSR penalty_rect
    LDAA #18
    LDAB penalty_mode
    CMPB #2
    BNE penalty_keeper_pose
    TST penalty_height
    BEQ penalty_keeper_pose
    LDAB penalty_progress
    CMPB #8
    BCS penalty_keeper_pose
    LDAA #12
penalty_keeper_pose:
    STAA penalty_pose_y
    LDAA #3
    STAA penalty_w
    STAA penalty_h
    LDAA penalty_keeper
    DECA
    LDAB penalty_pose_y
    JSR penalty_rect
    LDAA #5
    STAA penalty_h
    LDAA penalty_keeper
    DECA
    LDAB penalty_pose_y
    ADDB #3
    JSR penalty_rect
    LDAA #11
    STAA penalty_w
    LDAA #1
    STAA penalty_h
    LDAA penalty_keeper
    SUBA #5
    LDAB penalty_pose_y
    ADDB #4
    JSR penalty_rect
    LDAA #2
    STAA penalty_w
    LDAA #3
    STAA penalty_h
    LDAA penalty_keeper
    SUBA #3
    LDAB penalty_pose_y
    ADDB #8
    JSR penalty_rect
    LDAA penalty_keeper
    ADDA #2
    LDAB penalty_pose_y
    ADDB #8
    JSR penalty_rect
    LDAA penalty_mode
    CMPA #2
    BCC penalty_hud_check
    JSR penalty_aim_position
    LDAA penalty_aim_x
    SUBA #2
    LDAB penalty_aim_y
    SUBB #2
    JSR scene_pixel
    LDAA penalty_aim_x
    ADDA #2
    LDAB penalty_aim_y
    SUBB #2
    JSR scene_pixel
    LDAA penalty_aim_x
    SUBA #2
    LDAB penalty_aim_y
    ADDB #2
    JSR scene_pixel
    LDAA penalty_aim_x
    ADDA #2
    LDAB penalty_aim_y
    ADDB #2
    JSR scene_pixel
penalty_hud_check:
    TST resume_pending
    BNE penalty_hud_all
    TST penalty_hud
    BNE penalty_hud_values
    RTS
penalty_hud_all:
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
    LDX #penalty_shot_label
    LDAA #132
    LDAB #1
    JSR paint_text
    LDX #penalty_goal_label
    LDAA #132
    LDAB #3
    JSR paint_text
    LDX #penalty_power_label
    LDAA #132
    LDAB #5
    JSR paint_text
    LDX #penalty_ten_label
    LDAA #150
    LDAB #2
    JSR paint_text
    LDX #penalty_slash_label
    LDAA #150
    LDAB #4
    JSR paint_text
    LDAA #156
    STAA paint_x
    LDAA #4
    STAA paint_band
    LDAA stage
    ADDA #6
    JSR paint_number
penalty_hud_values:
    CLR penalty_hud
    LDAA #132
    STAA paint_x
    LDAA #2
    STAA paint_band
    LDAA penalty_kicks
    LDAB penalty_mode
    CMPB #3
    BEQ penalty_hud_shot
    INCA
penalty_hud_shot:
    JSR paint_number
    LDAA #132
    STAA paint_x
    LDAA #4
    STAA paint_band
    LDAA penalty_goals
    JSR paint_number
    LDAA #132
    STAA paint_x
    LDAA #6
    STAA paint_band
    LDAA penalty_power
    JSR paint_number
    LDX #penalty_low_label
    TST penalty_height
    BEQ penalty_height_text
    LDX #penalty_high_label
penalty_height_text:
    LDAA #132
    LDAB #7
    JSR paint_text
    LDAB penalty_mode
    CMPB #3
    BNE penalty_status_index
    LDAB penalty_reason
    ADDB #2
penalty_status_index:
    ASLB
    LDX #penalty_status_labels
    ABX
    LDX 0,X
    LDAA #78
    CLRB
    JMP paint_text
; Small silhouettes restore their old tiles; no background or second framebuffer copy.
penalty_rect:
    STAA penalty_draw_left
    STAB penalty_draw_y
    LDAA penalty_h
    STAA penalty_rows
penalty_rect_row:
    LDAA penalty_draw_left
    STAA penalty_draw_x
    LDAA penalty_w
    STAA penalty_cols
penalty_rect_pixel:
    LDAA penalty_draw_x
    LDAB penalty_draw_y
    JSR scene_pixel
    INC penalty_draw_x
    DEC penalty_cols
    BNE penalty_rect_pixel
    JSR input_poll
    INC penalty_draw_y
    DEC penalty_rows
    BNE penalty_rect_row
    RTS
.section .bss, bss
penalty_mode: .space 1
penalty_power: .space 1
penalty_power_dir: .space 1
penalty_keeper: .space 1
penalty_keeper_dir: .space 1
penalty_target: .space 1
penalty_height: .space 1
penalty_progress: .space 1
penalty_x: .space 1
penalty_y: .space 1
penalty_kicks: .space 1
penalty_goals: .space 1
penalty_reason: .space 1
penalty_count: .space 1
penalty_clock: .space 1
penalty_distance: .space 1
penalty_hud: .space 1
penalty_pose_y: .space 1
penalty_aim_x: .space 1
penalty_aim_y: .space 1
penalty_w: .space 1
penalty_h: .space 1
penalty_draw_left: .space 1
penalty_draw_x: .space 1
penalty_draw_y: .space 1
penalty_rows: .space 1
penalty_cols: .space 1
.section .data, data
penalty_status_labels: .word penalty_ready_label,penalty_charge_label,penalty_fly_label,penalty_scored_label,penalty_saved_label,penalty_wide_label
penalty_ready_label: .byte 82,69,65,68,89,0
penalty_charge_label: .byte 80,79,87,69,82,0
penalty_fly_label: .byte 70,76,89,32,32,0
penalty_scored_label: .byte 71,79,65,76,32,0
penalty_saved_label: .byte 83,65,86,69,32,0
penalty_wide_label: .byte 87,73,68,69,32,0
penalty_shot_label: .byte 83,72,79,84,0
penalty_goal_label: .byte 71,79,65,76,0
penalty_power_label: .byte 80,79,87,69,82,0
penalty_low_label: .byte 76,79,87,32,0
penalty_high_label: .byte 72,73,71,72,0
penalty_ten_label: .byte 47,48,49,48,0
penalty_slash_label: .byte 47,0
