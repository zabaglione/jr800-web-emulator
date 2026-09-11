; SPDX-License-Identifier: MIT
.global putt_board
.global putt_x
.global putt_y
.global putt_vx
.global putt_vy
.global putt_cup_x
.global putt_cup_y
.global putt_wind
.global putt_aim
.global putt_power
.global putt_shots
.global putt_left
.global putt_step
.global putt_blocked
.global putt_launch
.section .text, code
game_start:
    LDAA stage
    LDAB #117
    MUL
    ADDD #putt_levels
    STD putt_pointer
    CLR putt_index
putt_load:
    LDX putt_pointer
    LDAA 0,X
    INX
    STX putt_pointer
    LDAB putt_index
    LDX #putt_board
    ABX
    STAA 0,X
    INC putt_index
    LDAB putt_index
    CMPB #112
    BNE putt_load
    LDX putt_pointer
    LDAA 0,X
    STAA putt_tee_x
    LDAA 1,X
    STAA putt_tee_y
    LDAA 2,X
    STAA putt_cup_x
    LDAA 3,X
    STAA putt_cup_y
    LDAA 4,X
    STAA putt_wind
    CLR putt_shots
    CLR putt_left
    LDAA #2
    STAA putt_aim
    LDAA #4
    STAA putt_power
putt_tee:
    LDAA putt_tee_x
    STAA putt_x
    LDAA putt_tee_y
    STAA putt_y
    CLR putt_x + 1
    CLR putt_y + 1
    CLR putt_left
    INC putt_hud
    RTS
game_aux:
    CMPA #2
    BEQ game_start
    JSR putt_erase
    INC putt_shots
    JSR putt_tee
    LDAA putt_shots
    CMPA #12
    BCS putt_aux_done
    LDAA #5
    STAA phase
putt_aux_done:
    RTS
game_update:
    TST resume_pending
    BEQ putt_resumed
    CLR resume_pending
    LDAA input_ticks
    STAA putt_clock
putt_resumed:
    TST putt_left
    BEQ putt_controls
    LDAA input_ticks
    SUBA putt_clock
    CMPA #2
    BCS putt_update_done
    LDAA input_ticks
    STAA putt_clock
    JSR putt_erase
    JSR putt_step
    LDAA #1
    STAA redraw
    RTS
putt_controls:
    LDAA input_event
    ANDA #31
    BEQ putt_update_done
    JSR putt_erase
    LDAA input_event
    BITA #4
    BEQ putt_right
    DEC putt_aim
    LDAA putt_aim
    ANDA #7
    STAA putt_aim
    BRA putt_aimed
putt_right:
    BITA #8
    BEQ putt_up
    INC putt_aim
    LDAA putt_aim
    ANDA #7
    STAA putt_aim
    BRA putt_aimed
putt_up:
    BITA #1
    BEQ putt_down
    LDAA putt_power
    CMPA #8
    BEQ putt_aimed
    INC putt_power
    BRA putt_aimed
putt_down:
    BITA #2
    BEQ putt_aimed
    LDAA putt_power
    CMPA #1
    BEQ putt_aimed
    DEC putt_power
putt_aimed:
    LDAA #1
    STAA putt_hud
    STAA redraw
    LDAA input_event
    BITA #16
    BEQ putt_update_done
    JSR putt_launch
putt_update_done:
    RTS
putt_launch:
    TST putt_left
    BNE putt_update_done
    LDAA putt_aim
    ASLA
    ASLA
    TAB
    LDX #putt_vectors
    ABX
    LDD 0,X
    STD putt_vx
    LDD 2,X
    STD putt_vy
    LDD putt_vx
    TST putt_wind
    BEQ putt_launch_vector
    BMI putt_launch_west
    ADDD #32
    BRA putt_launch_vector
putt_launch_west:
    SUBD #32
putt_launch_vector:
    STD putt_vx
    LDAA putt_power
    ASLA
    ASLA
    ASLA
    STAA putt_left
    INC putt_shots
    LDAA input_ticks
    STAA putt_clock
    RTS
; Q8.8 velocities are table-derived. Arithmetic shifts slow the last eight ticks.
putt_step:
    LDAA putt_left
    CMPA #8
    BNE putt_step_x
    LDD putt_vx
    ASRA
    RORB
    STD putt_vx
    LDD putt_vy
    ASRA
    RORB
    STD putt_vy
putt_step_x:
    LDD putt_x
    ADDD putt_vx
    STD putt_candidate
    LDAB putt_y
    JSR putt_blocked
    TSTA
    BEQ putt_accept_x
    LDD #0
    SUBD putt_vx
    STD putt_vx
    BRA putt_step_y
putt_accept_x:
    LDD putt_candidate
    STD putt_x
putt_step_y:
    LDD putt_y
    ADDD putt_vy
    STD putt_candidate
    TAB
    LDAA putt_x
    JSR putt_blocked
    TSTA
    BEQ putt_accept_y
    LDD #0
    SUBD putt_vy
    STD putt_vy
    BRA putt_capture
putt_accept_y:
    LDD putt_candidate
    STD putt_y
putt_capture:
    LDAA putt_x
    SUBA putt_cup_x
    ADDA #2
    CMPA #5
    BCC putt_countdown
    LDAA putt_y
    SUBA putt_cup_y
    ADDA #2
    CMPA #5
    BCC putt_countdown
    CLR putt_left
    LDAA #4
    STAA phase
    RTS
putt_countdown:
    DEC putt_left
    BNE putt_step_done
    CLR putt_x + 1
    CLR putt_y + 1
    LDAA #1
    STAA putt_hud
    LDAA putt_shots
    CMPA #12
    BCS putt_step_done
    LDAA #5
    STAA phase
putt_step_done:
    RTS
; A/B are the top-left pixel. Check the complete 2x2 ball, including seams.
putt_blocked:
    CMPA #1
    BCS putt_blocked_yes
    CMPA #126
    BCC putt_blocked_yes
    CMPB #9
    BCS putt_blocked_yes
    CMPB #62
    BCC putt_blocked_yes
    STAA putt_test_x
    STAB putt_test_y
    JSR putt_wall_at
    BNE putt_blocked_yes
    LDAA putt_test_x
    INCA
    LDAB putt_test_y
    JSR putt_wall_at
    BNE putt_blocked_yes
    LDAA putt_test_x
    LDAB putt_test_y
    INCB
    JSR putt_wall_at
    BNE putt_blocked_yes
    LDAA putt_test_x
    INCA
    LDAB putt_test_y
    INCB
    JSR putt_wall_at
    BNE putt_blocked_yes
    CLRA
    RTS
putt_blocked_yes:
    LDAA #1
    RTS
putt_wall_at:
    LSRA
    LSRA
    LSRA
    STAA putt_col
    SUBB #8
    ANDB #248
    ASLB
    ADDB putt_col
    LDX #putt_board
    ABX
    LDAA 0,X
    CMPA #1
    BEQ putt_wall_found
    CLRA
    RTS
putt_wall_found:
    LDAA #1
    RTS
putt_aim_position:
    LDAB putt_aim
    LDX #putt_aim_dx
    ABX
    LDAA 0,X
    ADDA putt_x
    STAA putt_marker_x
    LDAB putt_aim
    LDX #putt_aim_dy
    ABX
    LDAA 0,X
    ADDA putt_y
    STAA putt_marker_y
    RTS
putt_erase:
    LDAA #2
    STAA scene_w
    STAA scene_h
    LDAA putt_x
    LDAB putt_y
    JSR scene_rect
    TST putt_left
    BNE putt_erase_done
    JSR putt_aim_position
    LDAA #3
    STAA scene_w
    STAA scene_h
    LDAA putt_marker_x
    LDAB putt_marker_y
    JMP scene_rect
putt_erase_done:
    RTS
game_tile:
    LDX #putt_board
    ABX
    LDAA 0,X
    INCA
    RTS
game_render:
    JSR paint_board
    LDAA #1
    STAA putt_draw_step
    LDAA putt_x
    LDAB putt_y
    JSR putt_ball
    TST putt_left
    BNE putt_render_hud
    JSR putt_aim_position
    LDAA #2
    STAA putt_draw_step
    LDAA putt_marker_x
    LDAB putt_marker_y
    JSR putt_ball
putt_render_hud:
    TST resume_pending
    BNE putt_hud_all
    TST putt_hud
    BNE putt_hud_values
    RTS
putt_hud_all:
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
    LDX #putt_wind_none
    TST putt_wind
    BEQ putt_wind_text
    BMI putt_wind_left
    LDX #putt_wind_right
    BRA putt_wind_text
putt_wind_left:
    LDX #putt_wind_west
putt_wind_text:
    LDAA #78
    CLRB
    JSR paint_text
    LDX #putt_angle_label
    LDAA #132
    LDAB #1
    JSR paint_text
    LDX #putt_power_label
    LDAA #132
    LDAB #3
    JSR paint_text
    LDX #putt_shot_label
    LDAA #132
    LDAB #5
    JSR paint_text
    LDX #putt_space_label
    LDAA #132
    LDAB #7
    JSR paint_text
    LDX #putt_limit_label
    LDAA #150
    LDAB #6
    JSR paint_text
putt_hud_values:
    CLR putt_hud
    LDAA #132
    STAA paint_x
    LDAA #2
    STAA paint_band
    LDAB putt_aim
    ASLB
    LDX #putt_angles
    ABX
    LDD 0,X
    JSR paint_number16
    LDAA #132
    STAA paint_x
    LDAA #4
    STAA paint_band
    LDAA putt_power
    JSR paint_number
    LDAA #132
    STAA paint_x
    LDAA #6
    STAA paint_band
    LDAA putt_shots
    JMP paint_number
putt_ball:
    STAA putt_draw_x
    STAB putt_draw_y
    JSR scene_pixel
    LDAA putt_draw_x
    ADDA putt_draw_step
    LDAB putt_draw_y
    JSR scene_pixel
    LDAA putt_draw_x
    LDAB putt_draw_y
    ADDB putt_draw_step
    JSR scene_pixel
    LDAA putt_draw_x
    ADDA putt_draw_step
    LDAB putt_draw_y
    ADDB putt_draw_step
    JMP scene_pixel
.section .bss, bss
putt_board: .space 112
putt_x: .space 2
putt_y: .space 2
putt_vx: .space 2
putt_vy: .space 2
putt_cup_x: .space 1
putt_cup_y: .space 1
putt_tee_x: .space 1
putt_tee_y: .space 1
putt_wind: .space 1
putt_aim: .space 1
putt_power: .space 1
putt_shots: .space 1
putt_left: .space 1
putt_clock: .space 1
putt_pointer: .space 2
putt_index: .space 1
putt_candidate: .space 2
putt_test_x: .space 1
putt_test_y: .space 1
putt_col: .space 1
putt_marker_x: .space 1
putt_marker_y: .space 1
putt_draw_step: .space 1
putt_draw_x: .space 1
putt_draw_y: .space 1
putt_hud: .space 1
.section .data, data
putt_aim_dx: .byte 0,4,6,4,0,252,250,252
putt_aim_dy: .byte 250,252,0,4,6,4,0,252
putt_angle_label: .byte 65,78,71,76,69,0
putt_power_label: .byte 80,79,87,69,82,0
putt_shot_label: .byte 83,72,79,84,0
putt_space_label: .byte 83,80,65,67,69,0
putt_limit_label: .byte 47,48,49,50,0
putt_wind_none: .byte 87,73,78,68,32,45,0
putt_wind_west: .byte 87,73,78,68,32,60,0
putt_wind_right: .byte 87,73,78,68,32,62,0
