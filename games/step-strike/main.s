; SPDX-License-Identifier: MIT
.global step_firing
.global step_shot_cell
.global step_shot_clock
.global board
.global player
.global guards
.global guard_count
.global bullet_dirs
.global bullet_cells
.global facing
.global turns
.section .text, code
game_start:
    JSR actor_initial_state
    LDAA #1
    STAA actor_intro_pending
    RTS
actor_initial_state:
    JSR challenge_start
    LDD #$FFFF
    STD challenge_view_cells
    CLR step_death_pending
    LDX #player
    JSR challenge_load
    LDX #bullet_cells
    LDAB #8
    LDAA #255
step_clear_bullets:
    STAA 0,X
    INX
    DECB
    BNE step_clear_bullets
    CLR step_firing
    CLR facing
    CLR step_help
    CLR turns
    CLR turn_mod
    RTS
game_update:
    TST step_firing
    BEQ step_controls
    LDAA input_ticks
    TST resume_pending
    BEQ step_clock_ready
    STAA step_shot_clock
    CLR resume_pending
step_clock_ready:
    SUBA step_shot_clock
    CMPA #4
    BCC step_shot_tick
    RTS
step_shot_tick:
    LDAA input_ticks
    STAA step_shot_clock
    TST step_range
    BNE step_shot_continue
    CLR step_firing
    JMP advance_turn
step_shot_continue:
    JMP step_shot
step_controls:
    LDAA input_event
    BITA #16
    BNE step_fire
    BITA #1
    BEQ step_down
    LDAB #3
    BRA step_move
step_down:
    BITA #2
    BEQ step_left
    LDAB #1
    BRA step_move
step_left:
    BITA #4
    BEQ step_right
    LDAB #2
    BRA step_move
step_right:
    BITA #8
    BEQ step_idle
    CLRB
step_move:
    STAB step_direction
    LDX #step_deltas
    ABX
    LDAA 0,X
    ADDA player
    TAB
    LDX #board
    ABX
    TST 0,X
    BNE step_idle
    STAB player
    JSR challenge_touch
    LDAA step_direction
    STAA facing
    LDAA player
    JSR kill_guard
    JMP advance_turn
step_fire:
    LDAA player
    STAA step_ray
    LDAA #4
    STAA step_range
    LDAA #1
    STAA step_firing
    LDAA player
    STAA step_shot_cell
    LDAA input_ticks
    STAA step_shot_clock
step_shot:
    LDAB facing
    LDX #step_deltas
    ABX
    LDAA 0,X
    ADDA step_ray
    STAA step_ray
    TAB
    LDX #board
    ABX
    TST 0,X
    BNE step_shot_end
    LDAA step_ray
    STAA step_shot_cell
    JSR kill_guard
    TST step_hit
    BNE step_shot_end
    DEC step_range
    JMP step_redraw
step_shot_end:
    CLR step_range
    JMP step_redraw
step_redraw:
    LDAA #1
    STAA redraw
step_idle:
    RTS
kill_guard:
    STAA step_target
    CLR step_hit
    LDX #guards
    LDAB #4
step_kill_loop:
    CMPA 0,X
    BNE step_kill_next
    LDAA #255
    STAA 0,X
    DEC guard_count
    INC step_hit
    RTS
step_kill_next:
    INX
    DECB
    BNE step_kill_loop
    RTS
game_aux:
    CMPA #1
    BEQ step_wait
    LDAA step_help
    EORA #1
    STAA step_help
    RTS
step_wait:
    TST step_firing
    BEQ step_wait_ready
    RTS
step_wait_ready:
    JMP advance_turn
advance_turn:
    JSR challenge_step
    INC turns
    LDAA #1
    STAA redraw
    CLR step_index
step_bullet_loop:
    LDAB step_index
    LDX #bullet_cells
    ABX
    LDAA 0,X
    CMPA #255
    BEQ step_bullet_next
    CMPA player
    BEQ step_dead
    STAA step_ray
    LDX #bullet_dirs
    ABX
    LDAB 0,X
    LDX #step_deltas
    ABX
    LDAA 0,X
    ADDA step_ray
    STAA step_ray
    TAB
    LDX #board
    ABX
    TST 0,X
    BEQ step_bullet_floor
    LDAA #255
    BRA step_store_bullet
step_bullet_floor:
    LDAA step_ray
    CMPA player
    BEQ step_dead
step_store_bullet:
    LDAB step_index
    LDX #bullet_cells
    ABX
    STAA 0,X
step_bullet_next:
    INC step_index
    LDAA step_index
    CMPA #8
    BNE step_bullet_loop
    INC turn_mod
    LDAA turn_mod
    CMPA #3
    BNE step_finish
    CLR turn_mod
    JSR guards_fire
step_finish:
    TST guard_count
    BEQ step_cleared
    RTS
step_cleared:
    LDAA #4
    STAA phase
    RTS
step_dead:
    LDAA #1
    STAA step_death_pending
    LDAA #5
    STAA phase
    RTS
guards_fire:
    CLR step_guard_index
step_guard_loop:
    LDAB step_guard_index
    LDX #guards
    ABX
    LDAA 0,X
    CMPA #255
    BNE step_guard_alive
    JMP step_guard_next
step_guard_alive:
    STAA step_guard_cell
    ANDA #$F0
    STAA step_axis
    LDAA player
    ANDA #$F0
    CMPA step_axis
    BNE step_guard_vertical
    CLRB
    LDAA player
    CMPA step_guard_cell
    BCC step_guard_aim
    LDAB #2
    BRA step_guard_aim
step_guard_vertical:
    LDAA step_guard_cell
    ANDA #15
    STAA step_axis
    LDAA player
    ANDA #15
    CMPA step_axis
    BEQ step_guard_column
    JMP step_guard_next
step_guard_column:
    LDAB #1
    LDAA player
    CMPA step_guard_cell
    BCC step_guard_aim
    LDAB #3
step_guard_aim:
    STAB step_direction
    LDAA step_guard_cell
    STAA step_ray
step_guard_los:
    LDAB step_direction
    LDX #step_deltas
    ABX
    LDAA 0,X
    ADDA step_ray
    STAA step_ray
    CMPA player
    BEQ step_guard_spawn
    TAB
    LDX #board
    ABX
    TST 0,X
    BEQ step_guard_los
    BRA step_guard_next
step_guard_spawn:
    CLRB
step_find_slot:
    LDX #bullet_cells
    ABX
    LDAA 0,X
    CMPA #255
    BEQ step_store_spawn
    INCB
    CMPB #8
    BNE step_find_slot
    BRA step_guard_next
step_store_spawn:
    LDAA step_guard_cell
    STAA 0,X
    LDX #bullet_dirs
    ABX
    LDAA step_direction
    STAA 0,X
step_guard_next:
    INC step_guard_index
    LDAA step_guard_index
    CMPA #4
    BEQ guards_fire_done
    JMP step_guard_loop
guards_fire_done:
    RTS
game_tile:
    TST step_firing
    BEQ step_normal_tile
    CMPB step_shot_cell
    BNE step_normal_tile
    CMPB player
    BEQ step_normal_tile
    LDAA facing
    ANDA #1
    ADDA #6
    RTS
step_normal_tile:
    STAB step_tile_cell
    CMPB player
    BNE step_tile_guard
    LDAA facing
    ADDA #2
    RTS
step_tile_guard:
    LDX #guards
    LDAA #4
step_tile_guard_loop:
    CMPB 0,X
    BEQ step_tile_enemy
    INX
    DECA
    BNE step_tile_guard_loop
    CLRA
    STAA step_tile_index
step_tile_bullet:
    LDAB step_tile_index
    LDX #bullet_cells
    ABX
    LDAA 0,X
    CMPA step_tile_cell
    BEQ step_tile_projectile
    INC step_tile_index
    LDAA step_tile_index
    CMPA #8
    BNE step_tile_bullet
    LDAB step_tile_cell
    LDX #board
    ABX
    LDAA 0,X
    BNE step_tile_done
    CMPB challenge_cells
    BEQ step_tile_intel_one
    CMPB challenge_cells + 1
    BNE step_tile_done
    LDAA challenge_bonus
    ANDA #2
    BRA step_tile_intel
step_tile_intel_one:
    LDAA challenge_bonus
    ANDA #1
step_tile_intel:
    TSTA
    BNE step_tile_collected
    LDAA #9
    RTS
step_tile_collected:
    LDAA #10
step_tile_done:
    RTS
step_tile_projectile:
    LDX #bullet_dirs
    ABX
    LDAA 0,X
    ANDA #1
    ADDA #6
    RTS
step_tile_enemy:
    LDAA #8
    RTS
game_bonus:
    RTS
game_render:
    JSR actor_render_scene
    TST actor_intro_pending
    BEQ step_render_death
    CLR actor_intro_pending
    JMP actor_intro
step_render_death:
    TST step_death_pending
    BEQ actor_render_done
    JMP actor_intro
actor_render_done:
    RTS
actor_render_scene:
    JSR paint_board
    JMP visual_hud
.section .bss, bss
step_firing: .space 1
step_shot_cell: .space 1
step_shot_clock: .space 1
player: .space 1
guard_count: .space 1
guards: .space 4
board: .space 112
bullet_cells: .space 8
bullet_dirs: .space 8
facing: .space 1
turns: .space 1
step_help: .space 1
turn_mod: .space 1
step_source: .space 2
step_dest: .space 2
step_direction: .space 1
step_ray: .space 1
step_range: .space 1
step_target: .space 1
step_hit: .space 1
step_index: .space 1
step_guard_index: .space 1
step_guard_cell: .space 1
step_axis: .space 1
step_tile_cell: .space 1
step_tile_index: .space 1
.section .data, data
step_deltas: .byte 1,16,255,240
step_heading: .byte 83,84,69,80,32,83,84,82,73,75,69,0 ; STEP STRIKE
step_enemies: .byte 69,78,69,77,73,69,83,0 ; ENEMIES
step_space: .byte 83,80,65,67,69,0 ; SPACE
step_fire_label: .byte 70,73,82,69,0 ; FIRE
step_return: .byte 82,69,84,85,82,78,0 ; RETURN

step_wait_help: .byte 77,69,78,85,32,87,65,73,84,0 ; MENU WAIT

; A short, cycle-timed start cue highlights the actor before controls begin.
.global actor_intro_frame
.global actor_intro_step
.global actor_intro_x
.global actor_intro_band
.section .text, code
actor_intro:
    LDAB player
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
    JSR step_intro_flush
actor_intro_frame:
    TST actor_intro_step
    BNE actor_intro_wait
    LDX #180
    TST step_death_pending
    BEQ step_intro_tone
    LDX #508
step_intro_tone:
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
    TST step_death_pending
    BEQ step_intro_finish
    LDAA input_ticks
    STAA actor_intro_clock
step_death_hold:
    JSR input_poll
    LDAA input_ticks
    SUBA actor_intro_clock
    CMPA #12
    BCS step_death_hold
    CLR step_death_pending
    JSR lose_game
    JSR step_intro_flush
step_intro_finish:
    JSR input_gate
    LDAA #1
    STAA resume_pending
    CLR redraw
    LDD actor_intro_bytes
    STD dirty_bytes
    ; Finish the complete shell frame for both stage starts and menu resets.
    LDS #$5FFF
    JMP frame_ready
step_intro_flush:
    JSR dirty_begin
step_intro_transfer:
    JSR dirty_next
    BEQ step_intro_sum
    JSR input_poll
    BRA step_intro_transfer
step_intro_sum:
    LDD dirty_bytes
    ADDD actor_intro_bytes
    STD actor_intro_bytes
    RTS
.global step_death_pending
.section .bss, bss
step_death_pending: .space 1
actor_intro_pending: .space 1
actor_intro_x: .space 1
actor_intro_band: .space 1
actor_intro_step: .space 1
actor_intro_clock: .space 1
actor_intro_rows: .space 1
actor_intro_bytes: .space 2
