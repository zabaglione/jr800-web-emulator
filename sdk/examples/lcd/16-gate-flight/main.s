; SPDX-License-Identifier: MIT
.global app_init
.global app_update
.global app_draw
.global phase
.global score
.global hull
.global stage
.global gate_age
.global gate_lane
.global ship_x
.global flight_paused
.extern scene_clear
.extern text
.extern framebuffer
.extern ui_footer
.extern ui_number
.extern hud_dirty
.extern input_event
.extern input_held
.extern p3_draw
.extern p3_line
.extern p3_x
.extern p3_y
.extern p3_yaw
.extern p3_pitch
.extern p3_scale
.extern p3_mode
.extern p3_model_tetra
.extern p3_model_octa
.section .text, code
app_init:
    CLR phase
    CLR score
    CLR flight_tick
    LDAA #96
    STAA ship_x
    LDX #flight_title
    LDD #framebuffer + 63
    JSR text
    LDX #flight_intro
    LDD #framebuffer + 192 + 9
    JSR text
    LDX #flight_start
    JMP ui_footer
app_update:
    LDAA phase
    CMPA #1
    BEQ flight_play
    INC flight_tick
    LDAA input_event
    ANDA #48
    BNE flight_start_requested
    RTS
flight_start_requested:
    JMP flight_new
flight_play:
    LDAA input_event
    BITA #16
    BEQ flight_move
    LDAA flight_paused
    EORA #1
    STAA flight_paused
    BEQ flight_resumed
    LDX #flight_pause_help
    JSR ui_footer
    BRA flight_move
flight_resumed:
    LDX #flight_controls
    JSR ui_footer
flight_move:
    TST flight_paused
    BNE flight_update_done
    INC flight_tick
    LDAA input_held
    ORAA input_event
    BITA #4
    BEQ flight_right
    LDAB ship_x
    CMPB #40
    BLS flight_right
    SUBB #4
    STAB ship_x
flight_right:
    BITA #8
    BEQ flight_advance
    LDAB ship_x
    CMPB #152
    BCC flight_advance
    ADDB #4
    STAB ship_x
flight_advance:
    INC gate_age
    LDAA gate_age
    CMPA #36
    BCS flight_update_done
    LDX #flight_lanes
    LDAB gate_lane
    ABX
    LDAA ship_x
    SUBA 0,X
    BPL flight_distance
    NEGA
flight_distance:
    CMPA #10
    BCC flight_miss
    INC score
    LDAA score
    CMPA #8
    BEQ flight_won
    BRA flight_next
flight_miss:
    DEC hull
    BEQ flight_lost
flight_next:
    LDAA #1
    STAA hud_dirty
    CLR gate_age
    INC stage
    LDAA stage
    CMPA #12
    BCS flight_stage_ready
    CLR stage
flight_stage_ready:
    LDX #flight_route
    LDAB stage
    ABX
    LDAA 0,X
    STAA gate_lane
flight_update_done:
    RTS
flight_won:
    LDAA #2
    STAA phase
    JSR scene_clear
    LDX #flight_win
    BRA flight_outcome
flight_lost:
    LDAA #3
    STAA phase
    JSR scene_clear
    LDX #flight_lose
flight_outcome:
    LDD #framebuffer + 60
    JSR text
    LDX #flight_retry
    JMP ui_footer
flight_new:
    JSR scene_clear
    LDAA #1
    STAA phase
    STAA gate_lane
    LDAA #3
    STAA hull
    CLR score
    CLR stage
    CLR gate_age
    CLR flight_tick
    CLR flight_paused
    LDAA #96
    STAA ship_x
    LDX #flight_score
    LDD #framebuffer + 6
    JSR text
    LDX #flight_goal
    LDD #framebuffer + 54
    JSR text
    LDX #flight_hull
    LDD #framebuffer + 126
    JSR text
    LDX #flight_controls
    JMP ui_footer
app_draw:
    LDAA phase
    CMPA #1
    BEQ flight_world
    LDAA #96
    STAA p3_x
    LDAA #36
    STAA p3_y
    LDAA #64
    STAA p3_scale
    LDAA flight_tick
    ANDA #63
    STAA p3_yaw
    LDAA #4
    STAA p3_pitch
    LDX #p3_model_tetra
    JMP p3_draw
flight_world:
    TST hud_dirty
    BEQ flight_geometry
    CLR hud_dirty
    LDAA score
    LDX #framebuffer + 42
    JSR ui_number
    LDAA hull
    LDX #framebuffer + 156
    JSR ui_number
flight_geometry:
    LDAA gate_age
    LSRA
    ADDA #21
    STAA flight_gate_y
    LDAB gate_age
    ADDB #8
    STAB flight_spread
    LDAA #96
    LDAB gate_lane
    BEQ flight_gate_left
    CMPB #2
    BEQ flight_gate_right
    BRA flight_gate_x_ready
flight_gate_left:
    SUBA flight_spread
    BRA flight_gate_x_ready
flight_gate_right:
    ADDA flight_spread
flight_gate_x_ready:
    STAA flight_gate_x
    LDAA gate_age
    LDAB #4
flight_divide_age:
    CMPA #3
    BCS flight_gate_size
    SUBA #3
    INCB
    BRA flight_divide_age
flight_gate_size:
    STAB flight_half
    LDAA flight_gate_x
    SUBA flight_half
    STAA flight_box_x0
    LDAA flight_gate_x
    ADDA flight_half
    STAA flight_box_x1
    LDAA flight_gate_y
    SUBA flight_half
    STAA flight_box_y0
    LDAA flight_gate_y
    ADDA flight_half
    STAA flight_box_y1
    JSR flight_box
    ; An independently rotating solid occupies the next lane.
    LDAA gate_lane
    INCA
    CMPA #3
    BCS flight_rock_lane
    CLRA
flight_rock_lane:
    TAB
    LDAA #96
    TSTB
    BEQ flight_rock_left
    CMPB #2
    BEQ flight_rock_right
    BRA flight_rock_x
flight_rock_left:
    SUBA flight_spread
    BRA flight_rock_x
flight_rock_right:
    ADDA flight_spread
flight_rock_x:
    STAA p3_x
    LDAA flight_gate_y
    STAA p3_y
    LDAA gate_age
    ASLA
    ADDA #16
    STAA p3_scale
    LDAA flight_tick
    ASLA
    ANDA #63
    STAA p3_yaw
    LDAA #4
    STAA p3_pitch
    LDX #p3_model_octa
    JSR p3_draw
    ; The player is nearest to the viewer and is drawn last.
    LDAA ship_x
    STAA p3_x
    LDAA #46
    STAA p3_y
    LDAA #48
    STAA p3_scale
    CLR p3_yaw
    LDX #p3_model_tetra
    JMP p3_draw
flight_box:
    LDAA flight_box_x0
    STAA flight_line
    STAA flight_line + 2
    LDAA flight_box_y0
    STAA flight_line + 1
    LDAA flight_box_y1
    STAA flight_line + 3
    LDX #flight_line
    JSR p3_line
    LDAA flight_box_x1
    STAA flight_line
    STAA flight_line + 2
    LDX #flight_line
    JSR p3_line
    LDAA flight_box_x0
    STAA flight_line
    LDAA flight_box_y0
    STAA flight_line + 1
    STAA flight_line + 3
    LDX #flight_line
    JSR p3_line
    LDAA flight_box_y1
    STAA flight_line + 1
    STAA flight_line + 3
    LDX #flight_line
    JMP p3_line
.section .bss, bss
phase: .space 1
score: .space 1
hull: .space 1
stage: .space 1
gate_age: .space 1
gate_lane: .space 1
ship_x: .space 1
flight_paused: .space 1
flight_tick: .space 1
flight_gate_x: .space 1
flight_gate_y: .space 1
flight_spread: .space 1
flight_half: .space 1
flight_box_x0: .space 1
flight_box_x1: .space 1
flight_box_y0: .space 1
flight_box_y1: .space 1
flight_line: .space 4
.section .data, data
flight_lanes: .byte 53,96,139
flight_route: .byte 1,0,2,0,1,2,2,0,1,2,0,1
; GATE FLIGHT
flight_title: .byte 71,65,84,69,32,70,76,73,71,72,84,0
; PASS 8 GATES. AVOID THE ROCKS.
flight_intro: .byte 80,65,83,83,32,56,32,71,65,84,69,83,46,32,65,86,79,73,68,32,84,72,69,32,82,79,67,75,83,46,0
; 4/6 MOVE  SPACE/RETURN START
flight_start: .byte 52,47,54,32,77,79,86,69,32,32,83,80,65,67,69,47,82,69,84,85,82,78,32,83,84,65,82,84,0
; 4/6 MOVE SPACE PAUSE BREAK EXIT
flight_controls: .byte 52,47,54,32,77,79,86,69,32,83,80,65,67,69,32,80,65,85,83,69,32,66,82,69,65,75,32,69,88,73,84,0
; PAUSED SPACE RESUME BREAK EXIT
flight_pause_help: .byte 80,65,85,83,69,68,32,83,80,65,67,69,32,82,69,83,85,77,69,32,66,82,69,65,75,32,69,88,73,84,0
; GATE CLEAR
flight_win: .byte 71,65,84,69,32,67,76,69,65,82,0
; HULL LOST
flight_lose: .byte 72,85,76,76,32,76,79,83,84,0
; SPACE/RETURN RETRY  BREAK EXIT
flight_retry: .byte 83,80,65,67,69,47,82,69,84,85,82,78,32,82,69,84,82,89,32,32,66,82,69,65,75,32,69,88,73,84,0
; SCORE
flight_score: .byte 83,67,79,82,69,0
; /08
flight_goal: .byte 47,48,56,0
; HULL
flight_hull: .byte 72,85,76,76,0
