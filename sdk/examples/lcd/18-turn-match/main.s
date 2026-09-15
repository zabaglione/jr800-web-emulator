; SPDX-License-Identifier: MIT
; Match a tetrahedron's two-axis orientation in three rounds.
.global app_init
.global app_update
.global app_draw
.global phase
.global level
.global tries
.global moves
.global player_yaw
.global player_pitch
.global target_yaw
.global target_pitch
.extern scene_clear
.extern text
.extern framebuffer
.extern ui_footer
.extern ui_number
.extern hud_dirty
.extern input_event
.extern p3_draw
.extern p3_x
.extern p3_y
.extern p3_yaw
.extern p3_pitch
.extern p3_scale
.extern p3_model_tetra
.section .text, code
app_init:
    CLR phase
    CLR match_tick
    LDX #match_title
    LDD #framebuffer + 66
    JSR text
    LDX #match_intro
    LDD #framebuffer + 192 + 3
    JSR text
    LDX #match_start
    JMP ui_footer
app_update:
    LDAA phase
    CMPA #1
    BEQ match_play
    INC match_tick
    LDAA input_event
    ANDA #48
    BEQ match_idle
    JMP match_new
match_idle:
    RTS
match_play:
    LDAA input_event
    ANDA #15
    BEQ match_check
    BITA #4
    BEQ match_right
    LDAB player_yaw
    SUBB #8
    ANDB #63
    STAB player_yaw
match_right:
    BITA #8
    BEQ match_up
    LDAB player_yaw
    ADDB #8
    ANDB #63
    STAB player_yaw
match_up:
    BITA #1
    BEQ match_down
    LDAB player_pitch
    ADDB #8
    ANDB #63
    STAB player_pitch
match_down:
    BITA #2
    BEQ match_moved
    LDAB player_pitch
    SUBB #8
    ANDB #63
    STAB player_pitch
match_moved:
    LDAA moves
    CMPA #99
    BCC match_move_counted
    INC moves
match_move_counted:
    LDAA #1
    STAA hud_dirty
    TST match_feedback
    BEQ match_check
    CLR match_feedback
    LDX #match_controls
    JSR ui_footer
match_check:
    LDAA input_event
    BITA #16
    BEQ match_update_done
    LDAA player_yaw
    CMPA target_yaw
    BNE match_wrong
    LDAA player_pitch
    CMPA target_pitch
    BNE match_wrong
    INC level
    LDAA level
    CMPA #3
    BEQ match_won
    JMP match_round
match_wrong:
    DEC tries
    BEQ match_lost
    LDAA #1
    STAA hud_dirty
    STAA match_feedback
    LDX #match_miss
    JMP ui_footer
match_update_done:
    RTS
match_won:
    LDAA #2
    STAA phase
    JSR scene_clear
    LDX #match_win
    BRA match_outcome
match_lost:
    LDAA #3
    STAA phase
    JSR scene_clear
    LDX #match_lose
match_outcome:
    LDD #framebuffer + 60
    JSR text
    LDX #match_retry
    JMP ui_footer
match_new:
    JSR scene_clear
    LDAA #1
    STAA phase
    LDAA #6
    STAA tries
    CLR level
    CLR moves
    CLR match_tick
    LDX #match_round_label
    LDD #framebuffer + 6
    JSR text
    LDX #match_goal
    LDD #framebuffer + 54
    JSR text
    LDX #match_tries_label
    LDD #framebuffer + 90
    JSR text
    LDX #match_moves_label
    LDD #framebuffer + 138
    JSR text
    LDX #match_target_label
    LDD #framebuffer + 192 + 34
    JSR text
    LDX #match_player_label
    LDD #framebuffer + 192 + 131
    JSR text
match_round:
    LDX #match_targets
    LDAB level
    ASLB
    ABX
    LDAA 0,X
    STAA target_yaw
    LDAA 1,X
    STAA target_pitch
    CLR player_yaw
    CLR player_pitch
    CLR match_feedback
    LDAA #1
    STAA hud_dirty
    LDX #match_controls
    JMP ui_footer
app_draw:
    LDAA phase
    CMPA #1
    BEQ match_world
    LDAA #96
    STAA p3_x
    LDAA #36
    STAA p3_y
    LDAA #64
    STAA p3_scale
    LDAA match_tick
    ANDA #63
    STAA p3_yaw
    LDAA #4
    STAA p3_pitch
    LDX #p3_model_tetra
    JMP p3_draw
match_world:
    TST hud_dirty
    BEQ match_geometry
    CLR hud_dirty
    LDAA level
    INCA
    LDX #framebuffer + 42
    JSR ui_number
    LDAA tries
    LDX #framebuffer + 114
    JSR ui_number
    LDAA moves
    LDX #framebuffer + 168
    JSR ui_number
match_geometry:
    LDAA #52
    STAA p3_x
    LDAA #36
    STAA p3_y
    LDAA #68
    STAA p3_scale
    LDAA target_yaw
    STAA p3_yaw
    LDAA target_pitch
    STAA p3_pitch
    LDX #p3_model_tetra
    JSR p3_draw
    LDAA #140
    STAA p3_x
    LDAA player_yaw
    STAA p3_yaw
    LDAA player_pitch
    STAA p3_pitch
    LDX #p3_model_tetra
    JMP p3_draw
.section .bss, bss
phase: .space 1
level: .space 1
tries: .space 1
moves: .space 1
player_yaw: .space 1
player_pitch: .space 1
target_yaw: .space 1
target_pitch: .space 1
match_tick: .space 1
match_feedback: .space 1
.section .data, data
match_targets: .byte 16,8,40,56,8,24
; TURN MATCH
match_title: .byte 84,85,82,78,32,77,65,84,67,72,0
; MATCH THE SOLID. CLEAR 3 ROUNDS
match_intro: .byte 77,65,84,67,72,32,84,72,69,32,83,79,76,73,68,46,32,67,76,69,65,82,32,51,32,82,79,85,78,68,83,0
; SPACE/RETURN START  BREAK EXIT
match_start: .byte 83,80,65,67,69,47,82,69,84,85,82,78,32,83,84,65,82,84,32,32,66,82,69,65,75,32,69,88,73,84,0
; 4/6 YAW 8/2 PITCH SPACE CHECK
match_controls: .byte 52,47,54,32,89,65,87,32,56,47,50,32,80,73,84,67,72,32,83,80,65,67,69,32,67,72,69,67,75,0
; NO MATCH. TURN THEN SPACE CHECK
match_miss: .byte 78,79,32,77,65,84,67,72,46,32,84,85,82,78,32,84,72,69,78,32,83,80,65,67,69,32,67,72,69,67,75,0
; ALL MATCHED
match_win: .byte 65,76,76,32,77,65,84,67,72,69,68,0
; TRIES LOST
match_lose: .byte 84,82,73,69,83,32,76,79,83,84,0
; SPACE/RETURN RETRY  BREAK EXIT
match_retry: .byte 83,80,65,67,69,47,82,69,84,85,82,78,32,82,69,84,82,89,32,32,66,82,69,65,75,32,69,88,73,84,0
; ROUND
match_round_label: .byte 82,79,85,78,68,0
; /03
match_goal: .byte 47,48,51,0
; TRY
match_tries_label: .byte 84,82,89,0
; MOVE
match_moves_label: .byte 77,79,86,69,0
; TARGET
match_target_label: .byte 84,65,82,71,69,84,0
; YOU
match_player_label: .byte 89,79,85,0
