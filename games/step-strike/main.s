; SPDX-License-Identifier: MIT
.global board
.global player
.global guards
.global guard_count
.global bullet_cells
.global facing
.global turns
.section .text, code
game_start:
    LDAA stage
    LDAB #118
    MUL
    ADDD #levels
    STD step_source
    LDX #player
    STX step_dest
    LDAB #118
step_load:
    LDX step_source
    LDAA 0,X
    INX
    STX step_source
    LDX step_dest
    STAA 0,X
    INX
    STX step_dest
    DECB
    BNE step_load
    LDX #bullet_cells
    LDAB #8
    LDAA #255
step_clear_bullets:
    STAA 0,X
    INX
    DECB
    BNE step_clear_bullets
    CLR facing
    CLR step_help
    CLR turns
    CLR turn_mod
    RTS
game_update:
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
    JSR kill_guard
    TST step_hit
    BNE step_shot_end
    DEC step_range
    BNE step_shot
step_shot_end:
    JMP advance_turn
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
    JMP advance_turn
advance_turn:
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
game_render:
    JSR paint_board
    LDX #step_heading
    CLRA
    CLRB
    JSR paint_text
    LDAA #108
    STAA paint_x
    CLR paint_band
    LDAA stage
    INCA
    JSR paint_number
    LDX #step_enemies
    LDAA #132
    LDAB #1
    JSR paint_text
    LDAA #132
    STAA paint_x
    LDAA #2
    STAA paint_band
    LDAA guard_count
    JSR paint_number
    LDX #step_space
    LDAA #132
    LDAB #4
    JSR paint_text
    TST step_help
    BEQ step_normal_help
    LDX #step_wait_help
    BRA step_draw_help
step_normal_help:
    LDX #step_fire_label
step_draw_help:
    LDAA #132
    LDAB #5
    JSR paint_text
    LDX #step_return
    LDAA #132
    LDAB #7
    JMP paint_text
.section .bss, bss
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
