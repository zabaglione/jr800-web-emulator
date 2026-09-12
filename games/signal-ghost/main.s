; SPDX-License-Identifier: MIT
.global signal_tick
.global signal_disabled
.global signal_vision
.global signal_cameras
.global signal_bases
.global signal_score
.global signal_trace
.global signal_turn
.section .text, code
game_start:
    JSR actor_initial_state
    LDAA #1
    STAA actor_intro_pending
    RTS
actor_initial_state:
    JSR grid_reset
    CLR signal_tick
    CLR signal_disabled
    CLR signal_score
    CLR signal_score + 1
    LDAA #2
    STAA grid_stat
    LDAA #15
    STAA cursor
    LDAA stage
    LDAB #55
    MUL
    ADDD #signal_levels
    STD signal_pointer
    CLR signal_index
signal_load:
    LDX signal_pointer
    LDAA 0,X
    INX
    STX signal_pointer
    PSHA
    LSRA
    LSRA
    LSRA
    LSRA
    LDAB signal_index
    LDX #board
    ABX
    STAA 0,X
    PULA
    ANDA #15
    STAA 1,X
    INC signal_index
    INC signal_index
    LDAA signal_index
    CMPA #98
    BNE signal_load
    LDX signal_pointer
    LDD 0,X
    STD signal_cameras
    LDAA 2,X
    STAA signal_cameras + 2
    LDD 3,X
    STD signal_bases
    LDAA 5,X
    STAA signal_bases + 2
    JMP signal_trace
game_aux:
    CMPA #1
    BNE signal_reset
    JMP signal_turn
signal_reset:
    JMP game_start
game_update:
    CLR resume_pending
    LDAA input_event
    BITA #16
    BEQ signal_move
    LDAB cursor
    LDX #board
    ABX
    LDAA 0,X
    CMPA #2
    BEQ signal_terminal_a
    CMPA #3
    BEQ signal_terminal_b
    JMP signal_turn
signal_terminal_a:
    LDAA #1
    BRA signal_disable
signal_terminal_b:
    LDAA #2
signal_disable:
    BITA signal_disabled
    BNE signal_turn
    ORAA signal_disabled
    STAA signal_disabled
    DEC grid_stat
    LDD signal_score
    ADDD #100
    STD signal_score
    BRA signal_turn
signal_move:
    JSR grid_move
    CMPB #255
    BNE signal_neighbor_ok
    RTS
signal_neighbor_ok:
    STAB signal_next
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BEQ signal_no_move
    JSR signal_camera_at
    CMPA #255
    BNE signal_no_move
    LDAA signal_next
    STAA cursor
signal_turn:
    JSR grid_count_move
    INC signal_tick
    LDAA signal_tick
    ANDA #3
    STAA signal_tick
    JSR signal_trace
    LDAB cursor
    LDX #signal_vision
    ABX
    TST 0,X
    BEQ signal_exit_check
    LDAA #5
    STAA phase
    RTS
signal_exit_check:
    LDAA signal_disabled
    CMPA #3
    BNE signal_no_move
    LDX #board
    ABX
    LDAA 0,X
    CMPA #4
    BNE signal_no_move
    LDAA moves
    CMPA #100
    BCC signal_clear
    LDAA #100
    SUBA moves
    TAB
    CLRA
    ADDD signal_score
    STD signal_score
signal_clear:
    LDAA #4
    STAA phase
signal_no_move:
    RTS
; B = cell, A = camera index or 255. Inactive cameras still block rays.
signal_camera_at:
    CMPB signal_cameras
    BNE signal_camera_b
    CLRA
    RTS
signal_camera_b:
    CMPB signal_cameras + 1
    BNE signal_camera_c
    LDAA #1
    RTS
signal_camera_c:
    CMPB signal_cameras + 2
    BNE signal_camera_none
    LDAA #2
    RTS
signal_camera_none:
    LDAA #255
    RTS
signal_trace:
    LDX #signal_vision
    LDAB #98
    CLRA
signal_trace_clear:
    STAA 0,X
    INX
    DECB
    BNE signal_trace_clear
    CLR signal_actor
signal_actor_loop:
    LDAB signal_actor
    LDX #signal_masks
    ABX
    LDAA 0,X
    BITA signal_disabled
    BEQ signal_actor_active
    JMP signal_actor_next
signal_actor_active:
    LDX #signal_cameras
    ABX
    LDAA 0,X
    CLR signal_origin_y
signal_origin_row:
    CMPA #14
    BCS signal_origin_ready
    SUBA #14
    INC signal_origin_y
    BRA signal_origin_row
signal_origin_ready:
    STAA signal_origin_x
    CLR signal_ray
signal_ray_loop:
    LDAA signal_origin_x
    STAA signal_x
    LDAA signal_origin_y
    STAA signal_y
    LDAB signal_actor
    LDX #signal_bases
    ABX
    LDAA 0,X
    ADDA signal_tick
    ASLA
    ADDA signal_ray
    DECA
    ANDA #7
    STAA signal_direction
    LDAA #4
    STAA signal_length
signal_ray_step:
    LDAB signal_direction
    LDX #signal_dx
    ABX
    LDAA 0,X
    ADDA signal_x
    CMPA #14
    BCC signal_ray_next
    STAA signal_x
    LDX #signal_dy
    ABX
    LDAA 0,X
    ADDA signal_y
    CMPA #7
    BCC signal_ray_next
    STAA signal_y
    LDAB #14
    MUL
    ADDB signal_x
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BEQ signal_ray_next
    JSR signal_camera_at
    CMPA #255
    BNE signal_ray_next
    LDX #signal_vision
    ABX
    LDAA #1
    STAA 0,X
    DEC signal_length
    BNE signal_ray_step
signal_ray_next:
    INC signal_ray
    LDAA signal_ray
    CMPA #3
    BNE signal_ray_loop
signal_actor_next:
    JSR input_poll
    INC signal_actor
    LDAA signal_actor
    CMPA #3
    BEQ signal_trace_done
    JMP signal_actor_loop
signal_trace_done:
    RTS
grid_value:
    CMPB cursor
    BNE signal_tile_camera
    LDAA #11
    RTS
signal_tile_camera:
    STAB signal_tile_cell
    JSR signal_camera_at
    CMPA #255
    BEQ signal_tile_board
    TAB
    LDX #signal_masks
    ABX
    LDAA 0,X
    BITA signal_disabled
    BNE signal_tile_inactive
    LDX #signal_bases
    ABX
    LDAA 0,X
    ADDA signal_tick
    ANDA #3
    ADDA #7
    RTS
signal_tile_inactive:
    LDAA #12
    RTS
signal_tile_board:
    LDAB signal_tile_cell
    LDX #board
    ABX
    LDAA 0,X
    BEQ signal_tile_vision
    CMPA #2
    BNE signal_tile_terminal_b
    LDAA signal_disabled
    BITA #1
    BNE signal_tile_inactive
    LDAA #2
    RTS
signal_tile_terminal_b:
    CMPA #3
    BNE signal_tile_exit
    LDAA signal_disabled
    BITA #2
    BNE signal_tile_inactive
    LDAA #3
    RTS
signal_tile_exit:
    CMPA #4
    BNE signal_tile_done
    LDAA signal_disabled
    CMPA #3
    BEQ signal_tile_open
    LDAA #4
    RTS
signal_tile_open:
    LDAA #5
    RTS
signal_tile_vision:
    LDX #signal_vision
    ABX
    TST 0,X
    BEQ signal_tile_done
    LDAA #6
signal_tile_done:
    RTS
game_render:
    JSR actor_render_scene
    TST actor_intro_pending
    BEQ actor_render_done
    CLR actor_intro_pending
    JMP actor_intro
actor_render_done:
    RTS
actor_render_scene:
    JSR paint_board
    JMP visual_hud
.section .bss, bss
signal_tick: .space 1
signal_disabled: .space 1
signal_vision: .space 98
signal_cameras: .space 3
signal_bases: .space 3
signal_score: .space 2
signal_pointer: .space 2
signal_index: .space 1
signal_next: .space 1
signal_actor: .space 1
signal_origin_x: .space 1
signal_origin_y: .space 1
signal_ray: .space 1
signal_x: .space 1
signal_y: .space 1
signal_direction: .space 1
signal_length: .space 1
signal_tile_cell: .space 1
signal_hud_force: .space 1
signal_old_moves: .space 1
signal_old_grid_stat: .space 1
signal_old_score: .space 2
.section .data, data
signal_masks: .byte 1,2,1
signal_dx: .byte 0,1,1,1,0,255,255,255
signal_dy: .byte 255,255,0,1,1,1,0,255
signal_moves_label: .byte 83,84,69,80,83,0
signal_links_label: .byte 76,73,78,75,83,0
signal_space_label: .byte 83,80,65,67,69,0
signal_action_label: .byte 65,67,84,0
signal_return_label: .byte 82,69,84,85,82,78,0

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
    LDAB #8
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
    ADDB #7
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
