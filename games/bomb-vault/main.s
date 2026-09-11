; SPDX-License-Identifier: MIT
.global bomb_cell
.global bomb_fuse
.global bomb_flames
.global bomb_flame_time
.global bomb_enemies
.global bomb_dirs
.global bomb_enemy_clock
.global bomb_lives
.global bomb_running
.global bomb_score
.global bomb_steps
.global bomb_world
.global bomb_explode
.global bomb_contact
.section .text, code
game_start:
    JSR grid_reset
    CLR bomb_score
    CLR bomb_score + 1
    CLR bomb_steps
    CLR bomb_running
    LDAA #3
    STAA bomb_lives
    STAA grid_stat
    LDAA stage
    LDAB #105
    MUL
    ADDD #bomb_levels
    STD bomb_pointer
    CLR bomb_index
bomb_load:
    LDX bomb_pointer
    LDAA 0,X
    INX
    STX bomb_pointer
    LDAB bomb_index
    LDX #board
    ABX
    STAA 0,X
    INC bomb_index
    LDAA bomb_index
    CMPA #105
    BNE bomb_load
    LDAA #49
    STAA bomb_enemies
    LDAA #77
    STAA bomb_enemies + 1
    LDAA #1
    STAA bomb_dirs
    STAA bomb_dirs + 1
bomb_ready:
    LDAA #16
    STAA cursor
    LDAA #255
    STAA bomb_cell
    CLR bomb_fuse
    CLR bomb_flame_time
    CLR bomb_enemy_clock
    CLR bomb_running
    LDX #bomb_flames
    LDAB #105
    CLRA
bomb_clear_fire:
    STAA 0,X
    INX
    DECB
    BNE bomb_clear_fire
    LDAA input_ticks
    STAA bomb_clock
    RTS
game_aux:
    CMPA #2
    BNE bomb_aux_pause
    JMP game_start
bomb_aux_pause:
    CLR bomb_running
    RTS
game_update:
    TST resume_pending
    BEQ bomb_input
    CLR resume_pending
    LDAA input_ticks
    STAA bomb_clock
bomb_input:
    JSR grid_move
    CMPB #255
    BEQ bomb_action
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BEQ bomb_action
    CMPA #2
    BEQ bomb_action
    CMPA #4
    BEQ bomb_action
    CMPB bomb_cell
    BEQ bomb_action
    STAB cursor
    LDAA #1
    STAA bomb_running
    JSR grid_count_move
    JSR bomb_contact
bomb_action:
    LDAA phase
    CMPA #2
    BEQ bomb_action_live
    RTS
bomb_action_live:
    LDAA input_event
    BITA #16
    BEQ bomb_timer
    LDAA #1
    STAA bomb_running
    STAA redraw
    LDAA bomb_cell
    CMPA #255
    BNE bomb_timer
    TST bomb_flame_time
    BNE bomb_timer
    LDAA cursor
    STAA bomb_cell
    LDAA #24
    STAA bomb_fuse
bomb_timer:
    TST bomb_running
    BEQ bomb_update_done
    LDAA input_ticks
    SUBA bomb_clock
    CMPA #6
    BCS bomb_update_done
    LDAA input_ticks
    STAA bomb_clock
    JSR bomb_world
    LDAA #1
    STAA redraw
bomb_update_done:
    RTS
bomb_world:
    INC bomb_steps
    TST bomb_flame_time
    BEQ bomb_tick_fuse
    DEC bomb_flame_time
    BNE bomb_tick_fuse
    LDX #bomb_flames
    LDAB #105
    CLRA
bomb_expire_loop:
    STAA 0,X
    INX
    DECB
    BNE bomb_expire_loop
bomb_tick_fuse:
    TST bomb_fuse
    BEQ bomb_enemy_tick
    DEC bomb_fuse
    BNE bomb_enemy_tick
    JSR bomb_explode
bomb_enemy_tick:
    INC bomb_enemy_clock
    LDAA bomb_enemy_clock
    CMPA #4
    BCS bomb_world_contact
    CLR bomb_enemy_clock
    CLR bomb_enemy_index
bomb_enemy_loop:
    LDAB bomb_enemy_index
    LDX #bomb_enemies
    ABX
    LDAA 0,X
    CMPA #255
    BEQ bomb_enemy_next
    STAA bomb_enemy_pos
    LDAB 0,X
    LDX #bomb_flames
    ABX
    TST 0,X
    BNE bomb_enemy_burn
    LDAB bomb_enemy_index
    LDX #bomb_dirs
    ABX
    LDAA 0,X
    ADDA bomb_enemy_pos
    STAA bomb_enemy_candidate
    TAB
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BEQ bomb_enemy_reverse
    CMPA #2
    BEQ bomb_enemy_reverse
    CMPA #4
    BEQ bomb_enemy_reverse
    LDAB bomb_enemy_candidate
    CMPB bomb_cell
    BEQ bomb_enemy_reverse
    LDAA bomb_enemy_candidate
    LDAB bomb_enemy_index
    LDX #bomb_enemies
    ABX
    STAA 0,X
    BRA bomb_enemy_next
bomb_enemy_reverse:
    LDAB bomb_enemy_index
    LDX #bomb_dirs
    ABX
    NEG 0,X
    BRA bomb_enemy_next
bomb_enemy_burn:
    JSR bomb_kill_enemy
bomb_enemy_next:
    INC bomb_enemy_index
    LDAA bomb_enemy_index
    CMPA #2
    BNE bomb_enemy_loop
bomb_world_contact:
    JMP bomb_contact
bomb_kill_enemy:
    LDAB bomb_enemy_index
    LDX #bomb_enemies
    ABX
    LDAA #255
    STAA 0,X
    LDD bomb_score
    ADDD #100
    STD bomb_score
    RTS
; The centre and four bounded rays stop at the first crate or stone.
bomb_explode:
    LDAB bomb_cell
    LDX #bomb_flames
    ABX
    LDAA #1
    STAA 0,X
    CLR bomb_ray
bomb_ray_start:
    LDAA bomb_cell
    STAA bomb_ray_pos
    LDAA #3
    STAA bomb_range
bomb_ray_step:
    LDAB bomb_ray
    LDX #bomb_offsets
    ABX
    LDAA 0,X
    ADDA bomb_ray_pos
    STAA bomb_ray_pos
    TAB
    LDX #board
    ABX
    LDAA 0,X
    CMPA #1
    BEQ bomb_ray_next
    STAA bomb_ray_value
    CMPA #2
    BNE bomb_key_crate
    CLR 0,X
    BRA bomb_crate_score
bomb_key_crate:
    CMPA #4
    BNE bomb_mark_ray
    LDAA #5
    STAA 0,X
bomb_crate_score:
    LDD bomb_score
    ADDD #30
    STD bomb_score
bomb_mark_ray:
    LDAB bomb_ray_pos
    LDX #bomb_flames
    ABX
    LDAA #1
    STAA 0,X
    LDAA bomb_ray_value
    CMPA #2
    BEQ bomb_ray_next
    CMPA #4
    BEQ bomb_ray_next
    DEC bomb_range
    BNE bomb_ray_step
bomb_ray_next:
    INC bomb_ray
    LDAA bomb_ray
    CMPA #4
    BNE bomb_ray_start
    LDAA #255
    STAA bomb_cell
    LDAA #4
    STAA bomb_flame_time
    RTS
bomb_contact:
    CLR bomb_enemy_index
bomb_contact_enemy:
    LDAB bomb_enemy_index
    LDX #bomb_enemies
    ABX
    LDAB 0,X
    CMPB #255
    BEQ bomb_contact_next
    STAB bomb_enemy_pos
    LDX #bomb_flames
    ABX
    TST 0,X
    BEQ bomb_contact_player
    JSR bomb_kill_enemy
    BRA bomb_contact_next
bomb_contact_player:
    CMPB cursor
    BEQ bomb_die
bomb_contact_next:
    INC bomb_enemy_index
    LDAA bomb_enemy_index
    CMPA #2
    BNE bomb_contact_enemy
    LDAB cursor
    LDX #bomb_flames
    ABX
    TST 0,X
    BNE bomb_die
    LDX #board
    ABX
    LDAA 0,X
    CMPA #5
    BNE bomb_check_exit
    CLR 0,X
    DEC grid_stat
    LDD bomb_score
    ADDD #50
    STD bomb_score
    RTS
bomb_check_exit:
    CMPA #3
    BNE bomb_contact_done
    TST grid_stat
    BNE bomb_contact_done
    LDD bomb_score
    ADDD #200
    STD bomb_score
    LDAA #4
    STAA phase
bomb_contact_done:
    RTS
bomb_die:
    DEC bomb_lives
    BNE bomb_restart
    LDAA #5
    STAA phase
    RTS
bomb_restart:
    JMP bomb_ready
grid_value:
    CMPB cursor
    BNE bomb_tile_fire
    LDAA #9
    RTS
bomb_tile_fire:
    LDX #bomb_flames
    ABX
    TST 0,X
    BEQ bomb_tile_bomb
    LDAA #7
    RTS
bomb_tile_bomb:
    CMPB bomb_cell
    BNE bomb_tile_enemy
    LDAA #6
    RTS
bomb_tile_enemy:
    CMPB bomb_enemies
    BEQ bomb_tile_enemy_yes
    CMPB bomb_enemies + 1
    BNE bomb_tile_board
bomb_tile_enemy_yes:
    LDAA #8
    RTS
bomb_tile_board:
    LDX #board
    ABX
    LDAA 0,X
    RTS
game_render:
    JSR paint_board
    CLR bomb_hud_force
    TST resume_pending
    BEQ bomb_hud_values
    INC bomb_hud_force
    LDX #game_name
    CLRA
    CLRB
    JSR paint_text
    LDX #bomb_action_label
    LDAA #72
    CLRB
    JSR paint_text
    LDAA #168
    STAA paint_x
    CLR paint_band
    LDAA stage
    INCA
    JSR paint_number
    LDX #bomb_keys_label
    LDAA #138
    LDAB #1
    JSR paint_text
    LDX #bomb_life_label
    LDAA #138
    LDAB #3
    JSR paint_text
    LDX #bomb_fuse_label
    LDAA #138
    LDAB #5
    JSR paint_text
    LDX #bomb_menu_label
    LDAA #132
    LDAB #7
    JSR paint_text
bomb_hud_values:
    TST bomb_hud_force
    BNE bomb_keys_value
    LDAA grid_stat
    CMPA bomb_old_keys
    BEQ bomb_life_check
bomb_keys_value:
    LDAA grid_stat
    STAA bomb_old_keys
    LDAA #138
    STAA paint_x
    LDAA #2
    STAA paint_band
    LDAA grid_stat
    JSR paint_number
bomb_life_check:
    TST bomb_hud_force
    BNE bomb_life_value
    LDAA bomb_lives
    CMPA bomb_old_lives
    BEQ bomb_fuse_check
bomb_life_value:
    LDAA bomb_lives
    STAA bomb_old_lives
    LDAA #138
    STAA paint_x
    LDAA #4
    STAA paint_band
    LDAA bomb_lives
    JSR paint_number
bomb_fuse_check:
    TST bomb_hud_force
    BNE bomb_fuse_value
    LDAA bomb_fuse
    CMPA bomb_old_fuse
    BEQ bomb_score_check
bomb_fuse_value:
    LDAA bomb_fuse
    STAA bomb_old_fuse
    LDAA #138
    STAA paint_x
    LDAA #6
    STAA paint_band
    LDAA bomb_fuse
    JSR paint_number
bomb_score_check:
    TST bomb_hud_force
    BNE bomb_score_value
    LDD bomb_score
    SUBD bomb_old_score
    BEQ bomb_render_done
bomb_score_value:
    LDD bomb_score
    STD bomb_old_score
    LDAA #138
    STAA paint_x
    LDAA #7
    STAA paint_band
    LDD bomb_score
    JMP paint_number16
bomb_render_done:
    RTS
.section .bss, bss
bomb_cell: .space 1
bomb_fuse: .space 1
bomb_flames: .space 105
bomb_flame_time: .space 1
bomb_enemies: .space 2
bomb_dirs: .space 2
bomb_enemy_clock: .space 1
bomb_lives: .space 1
bomb_running: .space 1
bomb_score: .space 2
bomb_steps: .space 1
bomb_clock: .space 1
bomb_pointer: .space 2
bomb_index: .space 1
bomb_enemy_index: .space 1
bomb_enemy_pos: .space 1
bomb_enemy_candidate: .space 1
bomb_ray: .space 1
bomb_ray_pos: .space 1
bomb_range: .space 1
bomb_ray_value: .space 1
bomb_hud_force: .space 1
bomb_old_keys: .space 1
bomb_old_lives: .space 1
bomb_old_fuse: .space 1
bomb_old_score: .space 2
.section .data, data
bomb_offsets: .byte 241,15,255,1
bomb_action_label: .byte 83,80,65,67,69,32,66,79,77,66,0
bomb_keys_label: .byte 75,69,89,83,0
bomb_life_label: .byte 76,73,70,69,0
bomb_fuse_label: .byte 70,85,83,69,0
bomb_menu_label: .byte 83,0
