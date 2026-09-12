; SPDX-License-Identifier: MIT
.global tail_body
.global tail_length
.global tail_direction
.global tail_queued
.global tail_running
.global tail_food
.global tail_eaten
.global tail_move
.global tail_spawn
.section .text, code
game_start:
    JSR actor_initial_state
    LDAA #1
    STAA actor_intro_pending
    RTS
actor_initial_state:
    JSR grid_reset
    CLR tail_eaten
    CLR tail_running
    LDAA #4
    STAA tail_length
    LDAA #1
    STAA tail_direction
    STAA tail_queued
    LDAA #49
    STAA cursor
    LDX #tail_body
    LDAB #4
tail_initial_body:
    STAA 0,X
    INX
    SUBA #14
    DECB
    BNE tail_initial_body
    LDX #board + 7
    LDAB #4
    LDAA #1
tail_initial_cells:
    STAA 0,X
    PSHB
    LDAB #14
    ABX
    PULB
    DECB
    BNE tail_initial_cells
    JMP tail_spawn
game_update:
    TST resume_pending
    BEQ tail_clock_ready
    CLR resume_pending
    LDAA input_ticks
    STAA tail_clock
tail_clock_ready:
    LDAA input_event
    BITA #16
    BEQ tail_read_direction
    LDAA #1
    STAA tail_running
    JSR grid_changed
tail_read_direction:
    LDAA input_event
    CLRB
    BITA #1
    BNE tail_queue
    INCB
    BITA #2
    BNE tail_queue
    INCB
    BITA #4
    BNE tail_queue
    INCB
    BITA #8
    BEQ tail_tick
tail_queue:
    TBA
    EORA #1
    CMPA tail_direction
    BEQ tail_tick
    STAB tail_queued
tail_tick:
    TST tail_running
    BEQ tail_idle
    LDAB stage
    LDX #tail_speeds
    ABX
    LDAA input_ticks
    SUBA tail_clock
    CMPA 0,X
    BCS tail_idle
    LDAA input_ticks
    STAA tail_clock
    JSR tail_move
    JMP grid_changed
tail_idle:
    RTS
game_aux:
    CMPA #2
    BNE tail_pause
    JMP game_start
tail_pause:
    CLR tail_running
    JMP grid_changed
; The moving tail vacates before a non-growing move, so its old cell is legal.
tail_move:
    LDAB tail_queued
    STAB tail_direction
    LDAA #98
    MUL
    ADDD #neighbors
    ADDB cursor
    ADCA #0
    XGDX
    LDAB 0,X
    CMPB #255
    BNE tail_inside
    JMP tail_dead
tail_inside:
    STAB tail_next
    LDX #board
    ABX
    LDAA 0,X
    CMPA #2
    BEQ tail_grow
    TSTA
    BEQ tail_release
    LDAB tail_length
    DECB
    LDX #tail_body
    ABX
    LDAA 0,X
    CMPA tail_next
    BNE tail_dead
tail_release:
    LDAB tail_length
    DECB
    LDX #tail_body
    ABX
    LDAB 0,X
    LDX #board
    ABX
    CLR 0,X
    BRA tail_shift_start
tail_grow:
    INC tail_length
    INC tail_eaten
    LDAA tail_eaten
    STAA grid_stat
tail_shift_start:
    LDAB tail_length
    DECB
    STAB tail_index
tail_shift:
    LDAB tail_index
    LDX #tail_body
    ABX
    DEX
    LDAA 0,X
    STAA 1,X
    DEC tail_index
    BNE tail_shift
    LDAA tail_next
    STAA tail_body
    STAA cursor
    TAB
    LDX #board
    ABX
    LDAA #1
    STAA 0,X
    LDAA tail_next
    CMPA tail_food
    BNE tail_move_done
    LDAB stage
    LDX #tail_goals
    ABX
    LDAA tail_eaten
    CMPA 0,X
    BCC tail_clear
    JMP tail_spawn
tail_clear:
    CLR tail_running
    LDAA #4
    STAA phase
tail_move_done:
    RTS
tail_dead:
    CLR tail_running
    LDAA #5
    STAA phase
    RTS
; Select an empty cell with a bounded scan over the board.
tail_spawn:
    LDAA #98
    SUBA tail_length
    STAA tail_free
    JSR random
tail_food_mod:
    CMPA tail_free
    BCS tail_food_rank
    SUBA tail_free
    BRA tail_food_mod
tail_food_rank:
    STAA tail_pick
    CLRB
    LDX #board
tail_food_scan:
    TST 0,X
    BNE tail_food_next
    TST tail_pick
    BEQ tail_food_here
    DEC tail_pick
tail_food_next:
    INX
    INCB
    BRA tail_food_scan
tail_food_here:
    LDAA #2
    STAA 0,X
    STAB tail_food
    RTS
grid_value:
    CMPB cursor
    BNE tail_other_tile
    LDAA tail_direction
    ADDA #3
    RTS
tail_other_tile:
    LDX #board
    ABX
    LDAA 0,X
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
tail_body: .space 98
tail_length: .space 1
tail_direction: .space 1
tail_queued: .space 1
tail_running: .space 1
tail_food: .space 1
tail_eaten: .space 1
tail_clock: .space 1
tail_next: .space 1
tail_index: .space 1
tail_free: .space 1
tail_pick: .space 1
tail_draw_eaten: .space 1
tail_draw_running: .space 1
.section .data, data
tail_food_label: .byte 70,79,79,68,32,0
tail_start_label: .byte 83,80,65,67,69,0
tail_goal_label: .byte 71,79,65,76,0
tail_size_label: .byte 83,73,90,69,0

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
